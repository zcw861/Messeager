// Module
// File: groupchat.cpp   Version: 0.1.0   License: AGPLv3
// Created: ZhouChengWei      2026-06-22 16:31:07
// Description:
//     群聊功能函数实现（EPOLL）
//     [v0.1.1] ZhouChengWei    2026-06-23 12:03:49
//         * 实现了一系列基础的群聊函数（连接成员，发送群消息，创建群聊)

#include "groupchat.h"
#include "chat.h"

#include <arpa/inet.h>
#include <fcntl.h>

#include <iostream>
#include <random>

#define MAX_EVENTS 64   //EPOLL事件最大数量

GroupChat::GroupChat(QObject *parent)
    : QObject(parent)
{}

GroupChat::~GroupChat(){}

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
            if (revents & (EPOLLERR | EPOLLHUP)) { continue; }

            //连接建立
            if (revents & EPOLLOUT) {
                //取消监听EPOLLOUT,监听EPOLLIN
                struct epoll_event mod_ev;
                mod_ev.events = EPOLLIN | EPOLLET;
                mod_ev.data.fd = fd;
                epoll_ctl(m_epollFd, EPOLL_CTL_MOD, fd, &mod_ev);
            }

            //有消息可读
            if (revents & EPOLLIN) {
                uint8_t type; //消息类型
                if (recv(fd, &type, 1, MSG_WAITALL) != -1) { continue; }

                uint32_t payloadLen = 0; //网络字节序(载荷长度)
                if (recv(fd, &payloadLen, 4, MSG_WAITALL) != -1) continue;
                payloadLen = ntohl(payloadLen);

                std::vector<char> buf;
                if (recv(fd, buf.data(), payloadLen, MSG_WAITALL) != (int) payloadLen) continue;

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
        send(fd, packet.data(), packet.size(), MSG_NOSIGNAL);
    }
}

void GroupChat::removeConnection(const std::string &groupId, const std::string &memberId, int fd){

}