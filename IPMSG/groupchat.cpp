// Module
// File: groupchat.cpp   Version: 0.1.0   License: AGPLv3
// Created: ZhouChengWei      2026-06-22 16:31:07
// Description:
//     群聊功能函数实现（EPOLL）
//     [v0.1.1] ZhouChengWei    2026-06-23 12:03:49
//         * 实现了一系列基础的群聊函数（连接成员，发送群消息，创建群聊)
//     [v0.1.2] ZhouChengWei    2026-06-23 13:23:06
//         * 添加了清理函数，以及接收函数
//     [v0.1.3] ZhouChengWei    2026-06-23 15:21:32
//         * 添加了群聊邀请处理函数
//     [v0.1.4] HeZhiyuan    2026-06-23 20:36:57
//         * 修改sendMsgToGroup()，通过bool值确认发送请求是否提交
//     [v0.2.0] ZhouChengWei    2026-06-25 17:35:48
//         * 完善所有群聊需要用到的函数，重构以前的群聊邀请/连接成员/发送消息/创建群聊
//         * 完善清理函数，同时封装所以线程，由m_workerThreads统一管理
//         * 添加了一系列错误检查，如等待对方ACK,连接重试
//         * 优化了群聊成员之间的TCP连接，实现两两连接
//         * 优化TCP消息的半包粘包问题，每个人一个接收缓冲区，一次性读取消息
//     [v0.2.1] ZhouChengWei    2026-06-27 14:39:20
//         * 添加了退出群聊/解散群聊的函数
//         * 修改了清理函数，现在会同步清理m_connectings，防止退群后下次不能进群聊了
//         * 添加用于保存正在清理的连接，防止多线程调用清理函数时清理一个连接2次

#include "groupchat.h"
#include "chat.h"

#include <QMetaObject>

#include <arpa/inet.h>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <netinet/in.h>
#include <poll.h>
#include <random>
#include <sstream>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

#define MAX_EVENTS 64                   //epoll_wait单次最多返回的事件数量
#define CONNECT_TIMEOUT 1000            //非阻塞TCP connect的超时时间
#define HELLO_ACK_TIMEOUT 1000          //主动连接后等待对端ACK的超时时间
#define CONNECT_RETRY_COUNT 6           //收到群邀请后，在连接建立阶段的重试次数（因为被邀请端可能还在保存数据库）
#define CONNECT_RETRY_INTERVAL 500      //两次TCP连接重试的间隔（毫秒）
#define MAX_INVITE_PAYLOAD_SIZE 60000   //UDP群邀请负载的最大字节数，防止包过大

GroupChat::GroupChat(Chat *chat, QObject *parent)
    : QObject(parent), m_chat(chat)
{}

GroupChat::~GroupChat()
{
    m_running = false;

    //等待所有工作线程结束
    {
        std::lock_guard<std::mutex> workerLock(m_workerMutex);
        for (std::thread &worker : m_workerThreads) {
            if (worker.joinable()) {
                worker.join();
            }
        }
        m_workerThreads.clear();
    }

    //关闭epoll描述符，使epoll线程退出
    if (m_epollFd != -1) {
        close(m_epollFd);
        m_epollFd = -1;
    }

    if (m_epollThread.joinable()) {
        m_epollThread.join();
    }

    //关闭所有群会话中的TCP连接
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto &[groupId, session] : m_sessions) {
        for (auto &[memberId, fd] : session.fds) {
            shutdown(fd, SHUT_RDWR);
            close(fd);
        }
    }

    m_sessions.clear();
    m_fdInfo.clear();
    m_receiveBuffers.clear();
    m_connectings.clear();
}

void GroupChat::startWorker(std::function<void()> task)
{
    std::lock_guard<std::mutex> lock(m_workerMutex);
    m_workerThreads.emplace_back(std::move(task));
}

bool GroupChat::ensureEpollStarted()
{
    if (m_running && m_epollFd >= 0) {
        return true;
    }

    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_running && m_epollFd >= 0) {
        return true;
    }

    const int epollFd = epoll_create1(EPOLL_CLOEXEC);
    if (epollFd < 0) {
        std::cerr << "创建群聊epoll失败: " << std::strerror(errno) << std::endl;
        return false;
    }

    m_epollFd = epollFd;
    m_running = true;

    m_epollThread = std::thread(&GroupChat::epollLoop, this);
    return true;
}

bool GroupChat::isTenDigitGroupId(const std::string &groupId)
{
    if (groupId.size() != 10) {
        return false;
    }
    for (const char ch : groupId) {
        if (ch < '0' || ch > '9') {
            return false;
        }
    }
    return true;
}

std::vector<GroupMember> GroupChat::toMembers(const std::vector<UserInfo> &groupMembers) const
{
    std::vector<GroupMember> members;
    members.reserve(groupMembers.size());

    for (const UserInfo &user : groupMembers) {
        GroupMember member;
        member.id = user.id;
        member.name = user.name;
        member.ip = user.ip;
        member.port = TCP_PORT;
        members.push_back(std::move(member));
    }

    return members;
}

bool GroupChat::setNonBlocking(int fd)
{
    const int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) {
        return false;
    }
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK) == 0;
}

bool GroupChat::sendPacketAll(int fd, const std::vector<uint8_t> &packet) const
{
    size_t sendTotal = 0;

    while (sendTotal < packet.size()) {
        auto size = send(fd, packet.data() + sendTotal, packet.size() - sendTotal, MSG_NOSIGNAL);
        if (size > 0) {
            sendTotal += static_cast<size_t>(size);
            continue;
        }

        //被信号中断，重试
        if (size < 0 && errno == EINTR) {
            continue;
        }

        //发送缓冲区暂时满，等待可写
        if (size < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            pollfd waitSock{};
            waitSock.fd = fd;
            waitSock.events = POLLOUT;
            if (poll(&waitSock, 1, 1000) > 0) {
                continue;
            }
        }

        return false;
    }

    return true;
}

QString GroupChat::generateGroupId()
{
    //避免多线程竞争同一个生成器(thread_local)
    thread_local std::mt19937_64 generator(std::random_device{}());
    std::uniform_int_distribution<long long> distribution(1000000000LL, 9999999999LL);

    //确保不会与已存在的群ID重复
    for (int attempt = 0; attempt < 1000; ++attempt) {
        auto candidate = std::to_string(distribution(generator));

        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_sessions.contains(candidate)) {
            return QString::fromStdString(candidate);
        }
    }

    return {};
}

std::string GroupChat::buildInvitePayload(const std::string &groupId,
                                          const std::string &groupName,
                                          const std::string &inviterId,
                                          const std::string &inviterName,
                                          const std::vector<GroupMember> &members) const
{
    std::ostringstream stream;  //拼接邀请包载荷（自动转换为字符串）

    stream << groupId << '\n';
    stream << groupName << '\n';
    stream << inviterId << '\n';
    stream << inviterName << '\n';
    stream << members.size() << '\n';   //成员数量

    //每个成员一行，字段用\t分隔
    for (const GroupMember &member : members) {
        stream << member.id << '\t'
               << member.name << '\t'
               << member.ip << '\n';
    }

    return stream.str();
}

bool GroupChat::sendInvite(const GroupMember &member, const std::string &invitePayload) const
{
     int fd = socket(PF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        return false;
    }

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(UDP_PORT);
    if (inet_pton(AF_INET, member.ip.c_str(), &address.sin_addr) != 1) {
        close(fd);
        return false;
    }

    auto packet = buildUdpPacket(MSG_TYPE_UDP_UNICAST, invitePayload);
    bool sendSuccess = false;

    //重复发送3次，一旦发送成功就退出
    for (int i = 0; i < 3 && m_running; ++i) {
        auto size= sendto(fd, packet.data(), packet.size(),0,
                          (struct sockaddr*)&address, sizeof(address));
        if (size == static_cast<ssize_t>(packet.size())) {
            sendSuccess = true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    close(fd);
    return sendSuccess;
}

bool GroupChat::createGroup(const QString &groupId, const QString &groupName,
                            const std::vector<UserInfo> &groupMembers)
{
    std::string normalGroupId = groupId.trimmed().toStdString();
    std::string normalGroupName = groupName.trimmed().toStdString();

    if (!isTenDigitGroupId(normalGroupId) || normalGroupName.empty() || groupMembers.size() < 3) {
        return false;
    }

    if (!ensureEpollStarted()) {
        return false;
    }

    auto members = toMembers(groupMembers);

    GroupSession session;
    session.groupId = normalGroupId;
    session.groupName = normalGroupName;

    for (const GroupMember &member : members) {
        if (member.id.empty() || member.name.empty()) {
            return false;
        }
        session.members[member.id] = member;
    }

    if (session.members.size() < 3) {
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_sessions.contains(normalGroupId)) {
            return false;   //群ID已存在
        }
        m_sessions.emplace(normalGroupId, std::move(session));  //将构建的群会话存入全局会话
    }

    //获取本机信息，作为邀请人
    std::string localId = m_chat->localId().toStdString();
    std::string localName = m_chat->localName().toStdString();

    //构造邀请包载荷
    std::string invitePayload = buildInvitePayload(normalGroupId,
                                                   normalGroupName,
                                                   localId,
                                                   localName,
                                                   members);

    if (invitePayload.size() > MAX_INVITE_PAYLOAD_SIZE) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_sessions.erase(normalGroupId);
        return false;
    }

    //向所有其他成员发送UDP邀请
    for (const GroupMember &member : members) {
        if (member.id == localId || member.ip.empty()) {  //跳过自己及无IP的成员
            continue;
        }
        startWorker([this, member, invitePayload]() {
            sendInvite(member, invitePayload);
        });
    }

    //按peerId大小规则建立TCP连接：peerId较小的主动连接较大的，避免重复连接
    ensureConnectToHigherIds(normalGroupId, members);
    return true;
}

bool GroupChat::restoreGroup(const QString &groupId, const QString &groupName,
                             const std::vector<UserInfo> &groupMembers)
{
    std::string normalGroupId = groupId.trimmed().toStdString();
    std::string normalGroupName = groupName.trimmed().toStdString();

    if (!isTenDigitGroupId(normalGroupId) || groupMembers.size() < 3) {
        return false;
    }

    if (!ensureEpollStarted()) {
        return false;
    }

    const std::vector<GroupMember> members = toMembers(groupMembers);

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        GroupSession &session = m_sessions[normalGroupId];
        session.groupId = normalGroupId;

        if (!normalGroupName.empty()) {
            session.groupName = normalGroupName;
        }

        //更新成员信息
        for (const GroupMember &member : members) {
            if (member.id.empty() || member.name.empty()) {
                continue;
            }
            session.members[member.id] = member;
        }
    }

    //为peerId大于本机的成员补建连接
    ensureConnectToHigherIds(normalGroupId, members);

    return true;
}

void GroupChat::ensureConnectToHigherIds(const std::string &groupId)
{
    std::vector<GroupMember> members;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_sessions.find(groupId);
        if (it  == m_sessions.end()) {
            return;
        }
        members.reserve(it->second.members.size());
        for (const auto &[memberId, member] : it->second.members) {
            members.push_back(member);  //将成员信息添加到vector中
        }
    }
    ensureConnectToHigherIds(groupId, members);
}

void GroupChat::ensureConnectToHigherIds(const std::string &groupId, const std::vector<GroupMember> &members)
{
    std::string localId = m_chat->localId().toStdString();

    for (const GroupMember &member : members) {
        // 只连接有IP，且peerId大于自己的成员
        if (member.id.empty() || member.id == localId || member.ip.empty() || localId >= member.id) {
            continue;
        }

        std::string connection = groupId + "\n" + member.id;

        {
            std::lock_guard<std::mutex> lock(m_mutex);
            const auto it = m_sessions.find(groupId);
            //已经存在连接或正在尝试连接，则跳过
            if (it == m_sessions.end()
                || it->second.fds.contains(member.id)
                || m_connectings.contains(connection)) {
                continue;
            }
            m_connectings.insert(connection);
        }

        //执行连接与注册流程
        startWorker([this, groupId, member, connection]() {
            connectAndRegister(groupId, member);
            std::lock_guard<std::mutex> lock(m_mutex);
            m_connectings.erase(connection);
        });
    }
}

int GroupChat::connectToMember(const std::string &ip, const uint16_t port) const
{
    int sockfd = socket(PF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        return -1;
    }

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    if (inet_pton(AF_INET, ip.c_str(), &address.sin_addr) != 1) {
        close(sockfd);
        return -1;
    }

    if (!setNonBlocking(sockfd)) {
        close(sockfd);
        return -1;
    }

    int result = ::connect(sockfd, (struct sockaddr*)&address, sizeof(address));
    if (result < 0 && errno != EINPROGRESS) {
        close(sockfd);
        return -1;
    }

    //如果connect正在进行，等待完成
    if (result < 0) {
        pollfd waitSock{};
        waitSock.fd = sockfd;
        waitSock.events = POLLOUT;
        if (poll(&waitSock, 1, CONNECT_TIMEOUT) <= 0) {
            close(sockfd);
            return -1;
        }

        //检查连接是否真正成功
        int socketError = 0;
        socklen_t length = sizeof(socketError);
        if (getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &socketError, &length) < 0 || socketError != 0) {
            close(sockfd);
            return -1;
        }
    }

    //连接成功后暂时恢复阻塞模式，因为要进行HELLO握手确认，所以必须设置1秒超时
    int flags = fcntl(sockfd, F_GETFL, 0);
    if (flags < 0 || fcntl(sockfd, F_SETFL, flags & ~O_NONBLOCK) < 0) {
        close(sockfd);
        return -1;
    }

    timeval timeout{};
    timeout.tv_sec = HELLO_ACK_TIMEOUT / 1000;
    timeout.tv_usec = (HELLO_ACK_TIMEOUT % 1000) * 1000;
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    return sockfd;
}

void GroupChat::sendHello(const int fd, const std::string &groupId, const GroupMember &self)
{
    std::ostringstream stream;
    stream << groupId << '\n'
           << self.id << '\n'
           << self.name << '\n'
           << self.ip;

    auto packet = buildTcpPacket(MSG_TYPE_TCP_GROUP_HELLO, stream.str());
    sendPacketAll(fd, packet);
}

bool GroupChat::waitForHelloAck(const int fd, const std::string &groupId) const
{
    //读类型MSG_TYPE_TCP_GROUP_ACK
    uint8_t type = 0;
    if (recv(fd, &type, 1, MSG_WAITALL) != 1) {
        return false;
    }
    if (type != MSG_TYPE_TCP_GROUP_ACK) return false;

    //读取载荷长度
    uint32_t netLength = 0;
    if (recv(fd, &netLength, sizeof(netLength), MSG_WAITALL) != static_cast<ssize_t>(sizeof(netLength))) {
        return false;
    }

    uint32_t payloadLength = ntohl(netLength);
    if (payloadLength > MAX_TCP_PAYLOAD_SIZE) {
        return false;
    }

    //读数据
    std::string payload(payloadLength, '\0');
    if (payloadLength > 0 && recv(fd, payload.data(), payloadLength, MSG_WAITALL)
               != static_cast<ssize_t>(payloadLength)) {
        return false;
    }
    if(payload != groupId) return false;

    return true;
}

void GroupChat::connectAndRegister(const std::string &groupId, const GroupMember &member)
{
    //构造自己的成员结构体
    GroupMember self{
        m_chat->localId().toStdString(),
        m_chat->localName().toStdString(),
        m_chat->localIp().toStdString(),
        TCP_PORT
    };

    if (member.id == self.id) {
        return;
    }

    for (int i = 0; i < CONNECT_RETRY_COUNT && m_running; ++i) {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            auto it = m_sessions.find(groupId);
            if (it == m_sessions.end()) {      //群会话不存在
                return;
            }
            if (it->second.fds.contains(member.id)) {   //已有连接
                return;
            }
        }

        int fd = connectToMember(member.ip, member.port);
        if (fd >= 0) {
            sendHello(fd, groupId, self);

            if (waitForHelloAck(fd, groupId) && addConnection(groupId, member.id, fd)) {
                return;
            }

            shutdown(fd, SHUT_RDWR);
            close(fd);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(CONNECT_RETRY_INTERVAL));
    }

    std::cerr << "连接群成员失败: " << member.name << " (" << member.ip << ")" << std::endl;
}

bool GroupChat::addConnection(const std::string &groupId, const std::string &memberId, const int fd)
{
    if (fd < 0 || m_epollFd < 0 || !setNonBlocking(fd)) {
        return false;
    }

    epoll_event event{};
    //监听数据可读，连接错误，连接挂起，对端半关闭写端
    event.events = EPOLLIN | EPOLLERR | EPOLLHUP | EPOLLRDHUP;
    event.data.fd = fd;

    if (epoll_ctl(m_epollFd, EPOLL_CTL_ADD, fd, &event) < 0) {
        return false;
    }

    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_sessions.find(groupId);
    if (it == m_sessions.end() || !it->second.members.contains(memberId)) {
        epoll_ctl(m_epollFd, EPOLL_CTL_DEL, fd, nullptr);
        return false;
    }

    GroupSession &session = it->second;

    //如果该成员已存在旧连接，则替换掉旧fd
    auto oldConnection = session.fds.find(memberId);
    if (oldConnection != session.fds.end() && oldConnection->second != fd) {
        int oldFd = oldConnection->second;
        epoll_ctl(m_epollFd, EPOLL_CTL_DEL, oldFd, nullptr);
        shutdown(oldFd, SHUT_RDWR);
        close(oldFd);
        m_fdInfo.erase(oldFd);
        m_receiveBuffers.erase(oldFd);
    }

    session.fds[memberId] = fd;
    m_fdInfo[fd] = {groupId, memberId};
    m_receiveBuffers.try_emplace(fd);   //为该fd创建接收缓冲区

    return true;
}


void GroupChat::handleConnection(int fd, const std::string &helloPayload, const std::string &peerIp)
{
    // 解析hello载荷：格式为 groupId\memberId\name\ip
    std::vector<std::string> lines;
    std::size_t start = 0;
    while (start <= helloPayload.size()) {
        std::size_t end = helloPayload.find('\n', start);
        if (end == std::string::npos) {
            lines.push_back(helloPayload.substr(start));
            break;
        }
        lines.push_back(helloPayload.substr(start, end - start));
        start = end + 1;
    }

    //至少需要groupId和memberId
    if (lines.size() < 2) {
        shutdown(fd, SHUT_RDWR);
        close(fd);
        return;
    }

    std::string groupId = lines[0];
    std::string memberId = lines[1];

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_sessions.find(groupId);

        //只允许属于本群已知成员列表中的连接
        if (it == m_sessions.end() || !it->second.members.contains(memberId)) {
            shutdown(fd, SHUT_RDWR);
            close(fd);
            return;
        }

        GroupMember &member = it->second.members[memberId];
        //更新对端提供的信息（名称与IP）
        if (lines.size() > 2 && !lines[2].empty()) {
            member.name = lines[2];
        }
        if (!peerIp.empty()) {
            member.ip = peerIp;
        }
    }

    //发送ACK确认
    auto ackPacket = buildTcpPacket(MSG_TYPE_TCP_GROUP_ACK, groupId);
    if (!sendPacketAll(fd, ackPacket) || !addConnection(groupId, memberId, fd)) {
        shutdown(fd, SHUT_RDWR);
        close(fd);
        return;
    }
}

void GroupChat::epollLoop()
{
    epoll_event events[MAX_EVENTS];
    std::vector<uint8_t> readBuffer(8192);

    while (m_running) {
        int nfds = epoll_wait(m_epollFd, events, MAX_EVENTS, 1000);
        if (nfds < 0) {
            if (errno == EINTR) {
                continue;
            }
            break;
        }

        for (int i = 0; i < nfds; ++i) {
            int fd = events[i].data.fd;
            uint32_t event = events[i].events;

            //错误或挂起事件
            if ((event & (EPOLLERR | EPOLLHUP | EPOLLRDHUP)) != 0) {
                cleanupFd(fd);
                continue;
            }

            //没数据可读时跳过
            if ((event & EPOLLIN) == 0) {
                continue;
            }

            bool disconnected = false;

            //循环读取，直到EAGAIN或对端关闭，将数据存入接收缓冲区
            while (true) {
                ssize_t received = recv(fd, readBuffer.data(), readBuffer.size(), 0);
                if (received > 0) {
                    std::lock_guard<std::mutex> lock(m_mutex);
                    std::vector<uint8_t> &buffer = m_receiveBuffers[fd];
                    buffer.insert(buffer.end(), readBuffer.begin(), readBuffer.begin() + received);
                    continue;
                }

                //对端关闭连接
                if (received == 0) {
                    disconnected = true;
                    break;
                }

                if (errno == EINTR) {
                    continue;
                }

                if (errno == EAGAIN || errno == EWOULDBLOCK) {     //本次数据已读完
                    break;
                }

                disconnected = true;
                break;
            }

            //如果连接断开或解析失败，则清理该连接
            if (disconnected || !handleBufferedFrames(fd)) {
                cleanupFd(fd);
            }
        }
    }
}

bool GroupChat::handleBufferedFrames(int fd)
{
    while (true) {
        uint8_t type = 0;
        std::vector<uint8_t> payload;
        std::string groupId;
        std::string memberId;

        {
            std::lock_guard<std::mutex> lock(m_mutex);

            auto infoIt = m_fdInfo.find(fd);
            auto bufferIt = m_receiveBuffers.find(fd);
            if (infoIt == m_fdInfo.end() || bufferIt == m_receiveBuffers.end()) {
                return false;
            }

            std::vector<uint8_t> &buffer = bufferIt->second;

            //1字节type + 4字节网络字节序载荷长度
            if (buffer.size() < 5) {
                return true;    //数据不够，继续等待
            }

            type = buffer[0];


            uint32_t netLength = 0;
            memcpy(&netLength, buffer.data() + 1, sizeof(netLength));
            uint32_t payloadLength = ntohl(netLength);

            if (payloadLength > MAX_TCP_PAYLOAD_SIZE) { return false; }

            size_t frameLength = 5U + payloadLength;

            //如果缓冲区中的数据不够一个完整帧，则继续等待
            if (buffer.size() < frameLength) { return true; }

            //提取数据部份
            payload.assign(buffer.begin() + 5, buffer.begin() + frameLength);

            //从缓冲区中移除已解析的帧
            buffer.erase(buffer.begin(), buffer.begin() + frameLength);

            groupId = infoIt->second.first;
            memberId = infoIt->second.second;
        }

        //群消息
        if (type == MSG_TYPE_TCP_GROUP_DATA) {
            MsgData message = deserializeMsgData(reinterpret_cast<const char *>(payload.data()),
                                                 payload.size());
            if (message.targetId != groupId || message.senderId != memberId || message.message.empty()) {
                return false;
            }

            //将消息发送到主线程
            QMetaObject::invokeMethod(this,[this, message]() {
                    emit groupMessageReceived(QString::fromStdString(message.targetId),
                                              QString::fromStdString(message.senderId),
                                              QString::fromStdString(message.senderName),
                                              QString::fromStdString(message.message));
                },Qt::QueuedConnection);
        //退群消息
        }else if (type == MSG_TYPE_TCP_GROUP_LEAVE) {
            std::string leaveInfo(payload.begin(), payload.end());
            size_t sep = leaveInfo.find('\n');
            if (sep == std::string::npos) return false;
            std::string gid = leaveInfo.substr(0, sep);
            std::string mid = leaveInfo.substr(sep + 1);

            //更新本地会话，移除该成员
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                auto it = m_sessions.find(gid);
                if (it != m_sessions.end()) {
                    it->second.members.erase(mid);
                    //对端会关闭fd，epoll会触发清理。
                    if (it->second.members.size() < 3) {
                        m_sessions.erase(it);   //本地解散
                    }
                }
            }
            continue;
        }else if (type == MSG_TYPE_TCP_GROUP_DISMISS) {
            std::string gid(payload.begin(), payload.end());
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_sessions.erase(gid); //整个群移除
            }
            //等待对端断开
            continue;
        }else {
            return false;
        }
    }
}

void GroupChat::cleanupFd(int fd)
{
    std::string connectKey;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_closingFds.insert(fd).second) {  //已经在被清理中
            return;
        }
        auto infoIt = m_fdInfo.find(fd);
        if (infoIt != m_fdInfo.end()) {
            std::string groupId = infoIt->second.first;
            std::string memberId = infoIt->second.second;
            auto sessionIt = m_sessions.find(groupId);
            if (sessionIt != m_sessions.end()) {
                sessionIt->second.fds.erase(memberId);
            }
            connectKey = groupId + "\n" + memberId;
            m_fdInfo.erase(infoIt);
        }
        m_receiveBuffers.erase(fd);
        if (!connectKey.empty()) {
            m_connectings.erase(connectKey);
        }
        m_closingFds.erase(fd);
    }

    if (m_epollFd >= 0) {
        epoll_ctl(m_epollFd, EPOLL_CTL_DEL, fd, nullptr);
    }

    shutdown(fd, SHUT_RDWR);
    close(fd);
}

bool GroupChat::sendMsgToGroup(const std::string &groupId, const std::string &content)
{
    if (!isTenDigitGroupId(groupId) || content.empty()) {
        return false;
    }

    std::vector<std::pair<std::string, int>> targets;   //目标ID和fd
    std::string localId = m_chat->localId().toStdString();
    std::string localName = m_chat->localName().toStdString();

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto sessionIt = m_sessions.find(groupId);
        if (sessionIt == m_sessions.end()) {
            return false;
        }

        //收集所有非本机的成员fd
        for (const auto &[memberId, fd] : sessionIt->second.fds) {
            if (memberId != localId) {
                targets.emplace_back(memberId, fd);
            }
        }
    }

    MsgData message;
    message.type = MSG_TYPE_TCP_GROUP_DATA;
    message.senderId = localId;
    message.senderName = localName;
    message.targetId = groupId;
    message.message = content;

    std::string payload = serializeMsgData(message);
    auto packet = buildTcpPacket(MSG_TYPE_TCP_GROUP_DATA, payload);

    //立即通过信号将消息本地显示，不经过网络
    QMetaObject::invokeMethod(this, [this, message](){
                                emit groupMessageReceived(
                                    QString::fromStdString(message.targetId),
                                    QString::fromStdString(message.senderId),
                                    QString::fromStdString(message.senderName),
                                    QString::fromStdString(message.message));
                              },Qt::QueuedConnection);

    std::vector<int> failedFds;

    //加锁，保证同一时刻只有一个线程进行TCP写入
    {
        std::lock_guard<std::mutex> sendLock(m_sendMutex);
        for (const auto &[memberId, fd] : targets) {
            if (!sendPacketAll(fd, packet)) {
                failedFds.push_back(fd);   //记录发送失败的fd
            }
        }
    }

    //清理发送失败的连接
    for (const int fd : failedFds) {
        cleanupFd(fd);
    }

    return true;
}

bool GroupChat::leaveGroup(const std::string &groupId, const std::string &memberId)
{
    int selfFd = -1;
    std::vector<int> otherFds;
    bool dismiss = false;

    std::vector<int> notifyFds;           //所有其他成员的fd
    std::vector<int> notifyTargets;       //实际用于发送通知的副本

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_sessions.find(groupId);
        if (it == m_sessions.end()) return false;

        m_connectings.erase(groupId + "\n" + memberId);

        //记录自己的fd并移除
        auto fdIt = it->second.fds.find(memberId);
        if (fdIt != it->second.fds.end()) {
            selfFd = fdIt->second;
            it->second.fds.erase(fdIt);
        }

        //收集其他成员的fd，用于发送通知
        for (const auto &[id, fd] : it->second.fds) {
            if (id != memberId) { notifyFds.push_back(fd); }
        }

        it->second.members.erase(memberId);

        notifyTargets = notifyFds;
        if (it->second.members.size() < 3) {
            dismiss = true;
            otherFds = std::move(notifyFds);
            m_sessions.erase(it);
        }
    }

    //向其他成员发送退出通知
    if (!notifyTargets.empty()) {
        std::string leavePayload = groupId + "\n" + memberId;
        auto packet = buildTcpPacket(MSG_TYPE_TCP_GROUP_LEAVE, leavePayload);

        {
            std::lock_guard<std::mutex> sendLock(m_sendMutex);
            for (int fd : notifyTargets) {
                sendPacketAll(fd, packet);
            }
        }
    }

    //清理自己的连接
    if (selfFd >= 0) { cleanupFd(selfFd); }

    //如果是解散，清理剩余所有连接
    if (dismiss) {
        for (int fd : otherFds) {
            cleanupFd(fd);
        }
    }

    return true;
}

bool GroupChat::dismissGroup(const std::string &groupId)
{
    std::vector<int> cleanFds;
    std::vector<int> notifyFds;
    std::string localId = m_chat->localId().toStdString();

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_sessions.find(groupId);
        if (it == m_sessions.end()) return false;

        for (const auto &[id, fd] : it->second.fds) {
            cleanFds.push_back(fd);
            m_connectings.erase(groupId + "\n" + id);
            if (id != localId) {
                notifyFds.push_back(fd);
            }
        }
        m_sessions.erase(it);
    }

    //发送解散通知
    if (!notifyFds.empty()) {
        std::string dismissPayload = groupId;
        auto packet = buildTcpPacket(MSG_TYPE_TCP_GROUP_DISMISS, dismissPayload);
        {
            std::lock_guard<std::mutex> sendLock(m_sendMutex);
            for (int fd : notifyFds) {
                sendPacketAll(fd, packet);
            }
        }
    }

    for (int fd : cleanFds) {
        cleanupFd(fd);
    }

    return true;
}