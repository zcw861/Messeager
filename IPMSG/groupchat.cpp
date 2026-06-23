// Module
// File: groupchat.cpp   Version: 0.1.0   License: AGPLv3
// Created: ZhouChengWei      2026-06-22 16:31:07
// Description:
//     群聊功能函数实现（EPOLL）
//     [v0.1.1] ZhouChengWei    2026-06-23 12:03:49
//         * 实现了一系列基础的群聊函数（连接成员，发送群消息，创建群聊)
//     [v0.1.2] ZhouChengWei    2026-06-23 13:23:06
//         * 添加了清理函数，以及接收函数

#include "groupchat.h"
#include "chat.h"

#include <arpa/inet.h>
#include <fcntl.h>

#include <iostream>
#include <random>

#define MAX_EVENTS 64   //EPOLL事件最大数量

GroupChat::GroupChat(Chat *chat, QObject *parent)
    : m_chat(chat), QObject(parent){}

GroupChat::~GroupChat()
{
    m_running = false;

    //关闭epoll实例，立即唤醒 epoll_wait（避免它无限阻塞）
    //同时保证后续join前epollFd已失效，线程能够快速退出。
    if (m_epollFd != -1) {
        close(m_epollFd);
        m_epollFd = -1;
    }

    //等待epoll事件循环线程结束
    if (m_epollThread.joinable()) {
        m_epollThread.join();
    }

    //关闭所有活跃的群成员TCP连接
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (auto &[groupId, session] : m_sessions) {
            for (auto &[memberId, fd] : session.fds) {
                close(fd);
            }
        }
        m_sessions.clear();
        m_fdInfo.clear();
    }
}

void GroupChat::createGroup(const std::vector<UserInfo> &groupMembers)
{
    //生成群ID
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<long long> distrib(1000000000ll, 9999999999ll);
    std::string groupId = std::to_string((distrib(gen)));

    GroupSession session;
    session.members = groupMembers;

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_sessions[groupId] = session; //绑定群ID与会话
    }

    //启动epoll线程
    if (!m_running) {
        m_running = true;
        m_epollFd = epoll_create1(0);
        m_epollThread = std::thread(&GroupChat::epollLoop, this);
    }

    //为每个群成员建立连接
    QString localId = m_chat->localId();
    for (const auto &member : groupMembers) {
        if (member.id == localId) continue;
        int fd = connectToMember(member.ip);
        if (fd >= 0) {
            addConnection(groupId, member.id, fd);
        } else {
            std::cerr << "连接群成员失败: " << member.name << " (" << member.ip << ")" << std::endl;
        }
    }
}

int GroupChat::connectToMember(const std::string &ip)
{
    int sockfd = socket(PF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) return -1;
    setNonBlocking(sockfd); //设置非阻塞

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(TCP_PORT);
    inet_pton(AF_INET, ip.c_str(), &address.sin_addr);

    if (::connect(sockfd, (struct sockaddr *) &address, sizeof(address)) < 0) {
        if (errno != EINPROGRESS) { //非阻塞connect正常会返回EINPROGRESS
            close(sockfd);
            return -1;
        }
    }

    return sockfd;
}

bool GroupChat::setNonBlocking(int fd)
{
    int flag = fcntl(fd, F_GETFL, 0);
    if (flag == -1) return false;
    return fcntl(fd, F_SETFL, flag | O_NONBLOCK) != -1;
}

void GroupChat::addConnection(const std::string &groupId, const std::string &memberId, int fd)
{
    struct epoll_event ev;
    ev.events = EPOLLIN | EPOLLOUT | EPOLLET;
    ev.data.fd = fd;

    if (epoll_ctl(m_epollFd, EPOLL_CTL_ADD, fd, &ev) == -1) {
        std::cerr << "epoll_ctl添加失败" << std::endl;
        close(fd);
        return;
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    auto &session = m_sessions[groupId];
    session.fds[memberId] = fd;
    m_fdInfo[fd] = {groupId, memberId};
}

void GroupChat::epollLoop()
{
    struct epoll_event events[MAX_EVENTS];

    while (m_running) {
        int nfds = epoll_wait(m_epollFd, events, MAX_EVENTS, 1000); //1秒超时
        if (nfds == -1) {
            if (errno == EINTR) continue;
            break;
        }

        for (int i = 0; i < nfds; ++i) {
            int fd = events[i].data.fd;
            uint32_t revents = events[i].events;

            //连接成功或出错
            if (revents & (EPOLLERR | EPOLLHUP)) {
                cleanupFd(fd);
                continue;
            }

            //连接建立
            if (revents & EPOLLOUT) {
                int error = 0;
                socklen_t len = sizeof(error);
                if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &len) == 0 && error != 0) {
                    //连接失败，清理
                    cleanupFd(fd);
                    continue;
                }
                // 连接成功，改为监听可读
                struct epoll_event mod_ev;
                mod_ev.events = EPOLLIN | EPOLLET;
                mod_ev.data.fd = fd;
                epoll_ctl(m_epollFd, EPOLL_CTL_MOD, fd, &mod_ev);
            }

            //有消息可读
            if (revents & EPOLLIN) {
                uint8_t type;
                if (recvFull(fd, &type, 1) != 1) {
                    cleanupFd(fd);
                    continue;
                }

                //读长度
                uint32_t payloadLen = 0;
                if (recvFull(fd, &payloadLen, 4) != 4) {
                    cleanupFd(fd);
                    continue;
                }
                payloadLen = ntohl(payloadLen);

                //读数据
                std::vector<char> buf(payloadLen);
                if (recvFull(fd, buf.data(), payloadLen) != (int) payloadLen) {
                    cleanupFd(fd);
                    continue;
                }

                if (type == MSG_TYPE_TCP_GROUP) {
                    MsgData msg = deserializeMsgData(buf.data(), payloadLen);
                    QMetaObject::invokeMethod(this,[this, msg]() {
                        emit groupMessageReceived(QString::fromStdString(msg.targetId), //这里是群ID
                        QString::fromStdString(msg.senderId),
                        QString::fromStdString(msg.senderName),
                        QString::fromStdString(msg.message));
                        },Qt::QueuedConnection);
                }
            }
        }
    }
}

void GroupChat::sendMsgToGroup(const std::string &groupId, const std::string &content)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_sessions.find(groupId);
    if (it == m_sessions.end()) return;

    auto &session = it->second;

    MsgData msgData;
    msgData.type = MSG_TYPE_TCP_GROUP;
    msgData.senderId = m_chat->localId().toStdString();
    msgData.senderName = m_chat->localName().toStdString();
    msgData.targetId = groupId;
    msgData.message = content;

    std::string payload = serializeMsgData(msgData);
    auto packet = buildTcpPacket(MSG_TYPE_TCP_GROUP, payload);

    for (auto &[memberId, fd] : session.fds) {
        if (memberId == m_chat->localId().toStdString()) continue;
        size_t total = 0;
        while (total < packet.size()) {
            ssize_t n = send(fd, packet.data() + total, packet.size() - total, MSG_NOSIGNAL);
            if (n < 0) {
                if (errno == EINTR) {   //信号中断，重试
                    continue;
                }
                break;
            }
            total += n;
        }
    }
}

void GroupChat::cleanupFd(int fd) {
    auto info = m_fdInfo.find(fd);
    if (info != m_fdInfo.end()) {
        std::string groupId = info->second.first;
        std::string memberId = info->second.second;
        epoll_ctl(m_epollFd, EPOLL_CTL_DEL, fd, nullptr);
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            auto it = m_sessions.find(groupId);
            if (it != m_sessions.end()) {
                it->second.fds.erase(memberId);
            }
        }
        m_fdInfo.erase(info);
    }
    close(fd);
}

int GroupChat::recvFull(int fd, void* buf, size_t len) {
    size_t total = 0;
    while (total < len) {
        ssize_t n = recv(fd, (char*)buf + total, len - total, 0);
        if (n > 0) {
            total += n;
        } else if (n == 0) {
            return 0; //对端关闭
        } else if (errno != EINTR) {
            return -1; //错误
        }
    }
    return total;
}