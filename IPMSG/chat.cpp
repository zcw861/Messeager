// Module
// File: Chat.cpp   Version: 0.1.0   License: AGPLv3
// Created:  ZhouChengWei     2026-06-09 19:30:06
// Description:
//      见Chat.h文件的描述，本实现的广播，接收广播以及收发消息各一个线程
//     [v0.1.1]  ZhouChengWei   2026-06-11 08:39:46
//         * 添加了错误处理
//     [v0.1.2]  ZhouChengWei   2026-06-11 21:39:59
//         * 用c++进行封装，提供信号，与qml进行交互
//     [v0.1.3]  ZhouChengWei   2026-06-11 22:22:08
//         * 添加了离线检测以及清理离线功能
//     [v0.1.4]  ZhouChengWei   2026-06-12 11:44:33
//         * 添加了清理旧的socket连接，防止下次运行时导致的tcp bind fail而收不到消息
//     [v0.1.5]  ZhouChengWei    2026-06-14 15:37:05
//         * 添加了关闭阻塞调用，防止程序重启发送消息时会导致收不到的bug
//     [v0.1.6]  ZhouChengWei   2026-06-14 21:31:48
//         * 处理了因为给自己发送消息而接收导致显示2次的问题
//     [v0.2.0] ZhouChengWei    2026-06-18 17:53:47
//         * 将逻辑修改为用ID辨别唯一用户
//     [v0.2.1] HeZhiyuan    2026-06-18 22:22:53
//         * 新增：setLocalId()，在网络层启动前获取数据库提供的id，禁止网络线程启动后修改
//     [v0.2.2] ZhouChengWei    2026-06-22 22:25:51
//         * 把收发消息全部重构为MsgData（见common.h)类型，以便区分消息类型，为群聊做准备
//     [v0.2.3] ZhouChengWei    2026-06-23 15:20:14
//         * 广播消息分为发现用户和群聊邀请
//     [v0.2.4] ZhouChengWei    2026-06-25 17:25:08
//         * 处理了群聊连接，将处理权交给群聊模块
//     [v0.2.5] ZhouChengWei     2026-06-26 16:44:18
//         * 实现设置名字的函数，同时优化了广播时的包构建逻辑，由原来的只构建第一次包改为每次发送时都构建，用于向对方同步自己的名字

#include "chat.h"
#include "groupchat.h"

#include <iostream>
#include <cstring>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>

Chat::Chat(QObject *parent)
    : QObject(parent)
{
    m_localId = QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString();
}

Chat::~Chat()
{
    m_running = false;

    //关闭socket让阻塞调用返回
    if(m_tcp_serverFd != -1){
        shutdown(m_tcp_serverFd, SHUT_RDWR);
    }
    if(m_udp_listenFd != -1){
        shutdown(m_udp_listenFd, SHUT_RDWR);
    }

    //等待所有线程结束
    if(m_broadcastThread.joinable()){
        m_broadcastThread.join();
    }
    if(m_listenThread.joinable()){
        m_listenThread.join();
    }
    if(m_serverThread.joinable()){
        m_serverThread.join();
    }
    if (m_cleanThread.joinable()) {
        m_cleanThread.join();
    }
}


//在网络服务启动前设置本机永久UUID。
bool Chat::setLocalId(const QString &localId)
{
    //网络线程启动后不能再修改本机ID。
    if(m_running){
        return false;
    }

    const QString localId1 = localId.trimmed();

    //使用QUuid解析并校验数据库返回的ID。
    const QUuid parsedUuid = QUuid::fromString(localId1);

    if(parsedUuid.isNull()){
        return false;
    }

    //网络层也统一保存为不带大括号的格式。
    m_localId = parsedUuid.toString(QUuid::WithoutBraces).toStdString();

    return true;
}

QVariantList Chat::onlineUsers() const
{
    std::lock_guard<std::mutex> lock(m_mutex);  //保护m_peers同时只能被一个线程访问
    //添加当前在线用户
    QVariantList list;
    for(const auto &[id, info] : m_peers){
        QVariantMap user;
        user["id"] = QString::fromStdString(info.id);
        user["name"] = QString::fromStdString(info.name);
        user["ip"] = QString::fromStdString(info.ip);
        list.append(user);
    }
    return list;
}

void Chat::start(const QString &userName)
{
    if(m_running) return;  //防止重复启动

    m_localName = userName.toStdString();

    //获取本机局域网IP
    int tmpSock = socket(PF_INET, SOCK_DGRAM, 0);
    if(tmpSock < 0){
        m_localIp = "127.0.0.1";
    }else{
        sockaddr_in remote{};
        remote.sin_family = AF_INET;
        remote.sin_port = htons(53);          //任意外部端口
        inet_pton(AF_INET, "8.8.8.8", &remote.sin_addr);  //任意外部 IP
        // connect绑定路由，以此来获取本机IP
        if(::connect(tmpSock, (sockaddr*)&remote, sizeof(remote)) == 0){
            sockaddr_in localAddr{};
            socklen_t addrLen = sizeof(localAddr);
            if(getsockname(tmpSock, (sockaddr*)&localAddr, &addrLen) == 0){
                m_localIp = inet_ntoa(localAddr.sin_addr);
            }else{
                m_localIp = "127.0.0.1";
            }
        }else{
            m_localIp = "127.0.0.1";
        }
        close(tmpSock);
    }

    m_running = true;

    //启动四个工作线程
    m_broadcastThread = std::thread(&Chat::broadcastThread, this);
    m_listenThread = std::thread(&Chat::listenThread, this);
    m_serverThread = std::thread(&Chat::tcpServerThread, this);
    m_cleanThread = std::thread(&Chat::cleanOfflineThread, this);

    std::cout << "信使聊天服务已启动，用户: " << m_localName << std::endl;
}

void Chat::sendMessageToUser(const QString &id, const QString &msg)
{
    std::string idStr = id.toStdString();
    std::string msgStr = msg.toStdString();

    //如果发给自己
    if (idStr == m_localId){
        //本地接收事件，不经过网络
        QMetaObject::invokeMethod(this, [this, msgStr](){
            emit messageReceived(
                QString::fromStdString(m_localId),
                QString::fromStdString(m_localName),
                QString::fromStdString(m_localIp),
                QString::fromStdString(msgStr)
                );
        }, Qt::QueuedConnection);
        return;
    }

    std::string targetIp;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_peers.find(idStr);
        if (it != m_peers.end()) {
            targetIp = it->second.ip;
        } else {
            std::cerr << "用户 " << idStr << " 不在线" << std::endl;
            return;
        }
    }

    //构造消息结构体
    MsgData msgData;
    msgData.type = MSG_TYPE_TCP_PRIVATE;
    msgData.senderId = m_localId;
    msgData.senderName = m_localName;
    msgData.targetId = idStr;
    msgData.message = msgStr;

    //序列化为二进制载荷
    std::string payload = serializeMsgData(msgData);
    //构建TCP包：类型 + 长度 + 载荷
    auto packet = buildTcpPacket(MSG_TYPE_TCP_PRIVATE, payload);

    std::thread([targetIp, packet = std::move(packet)](){
        int sendfd = socket(PF_INET, SOCK_STREAM, 0);
        if(sendfd < 0) {
            perror("tcp send socket create fail");
            return;
        }

        sockaddr_in user{};
        user.sin_family = AF_INET;
        user.sin_port = htons(TCP_PORT);
        inet_pton(AF_INET, targetIp.c_str(), &user.sin_addr);

        if(::connect(sendfd, (sockaddr*)&user, sizeof(user)) < 0) {
            std::cerr << "连接 " << targetIp << " 失败" << std::endl;
            close(sendfd);
            return;
        }

        send(sendfd, packet.data(), packet.size(), 0);
        close(sendfd);
    }).detach();
}

void Chat::emitMessageReceived(const std::string &id, const std::string &name,
                               const std::string &ip, const std::string &msg)
{
    //线程安全地发射信号到主线程（由于不是使用QThread,只能通过QMetaObject::invokeMethod间接发射信号）
    QMetaObject::invokeMethod(this, [this, id, name, ip, msg](){
        emit messageReceived(
            QString::fromStdString(id),
            QString::fromStdString(name),
            QString::fromStdString(ip),
            QString::fromStdString(msg)
            );
    }, Qt::QueuedConnection);
}

void Chat::broadcastThread()
{
    int listenfd = socket(PF_INET, SOCK_DGRAM, 0);
    if(listenfd < 0) {
        perror("udp broadcast socket create fail");
        return;
    }

    //设置广播能发送到"255.255.255.255"
    int broadcastEnable = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_BROADCAST, &broadcastEnable, sizeof(broadcastEnable));

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(UDP_PORT);
    address.sin_addr.s_addr = inet_addr("255.255.255.255");

    std::cout << "UDP广播线程已启动" << std::endl;

    while(m_running){
        //广播载荷：ID:用户名
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            std::string localName = m_localName;
        }
        std::string broadcastPayload = m_localId + ":" + m_localName;
        auto packet = buildUdpPacket(MSG_TYPE_UDP_BROADCAST, broadcastPayload);

        sendto(listenfd, packet.data(), packet.size(), 0,
               (struct sockaddr*)&address, sizeof(address));
        sleep(2);  //每2秒广播一次
    }

    close(listenfd);
    std::cout << "UDP广播线程已退出" << std::endl;
}

void Chat::listenThread()
{
    int listenfd = socket(PF_INET, SOCK_DGRAM, 0);
    m_udp_listenFd = listenfd;
    if(listenfd < 0) {
        perror("udp listen socket create fail");
        return;
    }

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(UDP_PORT);
    address.sin_addr.s_addr = INADDR_ANY;

    //清除bind的超时错误
    int reuse = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    if(bind(listenfd, (sockaddr*)&address, sizeof(address)) < 0){
        perror("udp bind failed");
        close(listenfd);
        return;
    }

    std::cout << "UDP监听线程已启动" << std::endl;

    char buffer[MESSAGE_BUF_SIZE];

    while(m_running){
        sockaddr_in sender{};
        socklen_t len = sizeof(sender);
        memset(&sender, 0, sizeof(sender));

        int n = recvfrom(listenfd, buffer, MESSAGE_BUF_SIZE - 1, 0, (sockaddr*)&sender, &len);
        if(n < 1) continue;

        uint8_t type = buffer[0];
        std::string payload(buffer + 1, n - 1);
        std::string ip = inet_ntoa(sender.sin_addr);

        if(type == MSG_TYPE_UDP_BROADCAST){ //发现用户
            //解析"ID:用户名"
            size_t colonPos = payload.find(':');
            if (colonPos == std::string::npos) continue;
            std::string userId = payload.substr(0, colonPos);
            std::string userName = payload.substr(colonPos + 1);

            //if(userId == m_localId) continue; //忽略自己的广播

            {
                std::lock_guard<std::mutex> lock(m_mutex);
                auto now = std::chrono::steady_clock::now();
                m_peers[userId] = {userName, ip, now, userId};
            }

            QMetaObject::invokeMethod(this, [this](){
                emit onlineUsersChanged();
            }, Qt::QueuedConnection);

            std::cout << "发现用户: " << userName << " ID:" << userId << " (" << ip << ")" << std::endl;
        }else if(type == MSG_TYPE_UDP_UNICAST){ //群聊邀请

            std::vector<std::string> lines;
            std::size_t start = 0;

            while(start <= payload.size()) {
                const std::size_t end = payload.find('\n', start);

                //如果没找到换行符，说明已到达最后一行
                if(end == std::string::npos){
                    lines.push_back(payload.substr(start));
                    break;
                }
                //否则读一行
                lines.push_back(payload.substr(start, end - start));
                start = end + 1;    //+1跳过换行符
            }

            if(lines.size() < 5){
                continue;
            }

            std::string groupId = lines[0];       //群ID
            std::string groupName = lines[1];     //群名字
            std::string inviterId = lines[2];     //邀请者ID
            std::string inviterName = lines[3];   //邀请者名字

            int memberCount = 0;

            memberCount = std::stoi(lines[4]);    //成员数量

            //成员数量必须大于3,并且除了前5行头部外至少还有memberCount成员的信息
            if(memberCount < 3 || lines.size() < static_cast<std::size_t>(5 + memberCount)){
                continue;
            }

            QStringList memberRecords;
            //预先分配空间，避免后续逐个添加成员时反复扩容
            memberRecords.reserve(memberCount);

            for(int index = 0; index < memberCount; ++index){
                memberRecords.push_back(QString::fromStdString(lines[5 + index]));
            }

            //不在Chat中直接修改GroupChat，AppController必须先保存数据库，再恢复GroupChat会话。
            QMetaObject::invokeMethod(this,
                [this,groupId,groupName,inviterId,inviterName,ip,memberRecords]() {
                    emit groupInviteReceived(QString::fromStdString(groupId),
                                             QString::fromStdString(groupName),
                                             QString::fromStdString(inviterId),
                                             QString::fromStdString(inviterName),
                                             QString::fromStdString(ip),
                                             memberRecords);
            },Qt::QueuedConnection);
        }
    }
    close(listenfd);
    std::cout << "UDP监听线程已退出" << std::endl;
}

void Chat::tcpServerThread()
{
    int serverfd = socket(PF_INET, SOCK_STREAM, 0);
    m_tcp_serverFd = serverfd;
    if(serverfd < 0){
        perror("tcp server socket create fail");
        return;
    }

    //清理之前的旧连接
    int reuse1 = 1;
    setsockopt(serverfd, SOL_SOCKET, SO_REUSEADDR, &reuse1, sizeof(reuse1));
    setsockopt(serverfd, SOL_SOCKET, SO_REUSEPORT, &reuse1, sizeof(reuse1));

    //清除bind超时错误
    int reuse2 = 1;
    setsockopt(serverfd, SOL_SOCKET, SO_REUSEADDR, &reuse2, sizeof(reuse2));

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(TCP_PORT);
    address.sin_addr.s_addr = INADDR_ANY;

    if(bind(serverfd, (struct sockaddr*)&address, sizeof(address)) < 0){
        perror("tcp bind failed");
        close(serverfd);
        return;
    }

    listen(serverfd, 10);

    std::cout << "TCP服务器线程已启动" << std::endl;

    while(m_running){
        sockaddr_in client_address{};
        socklen_t len = sizeof(client_address);

        int clientfd = accept(serverfd, (sockaddr*)&client_address, &len);
        if(clientfd < 0){
            if(m_running) perror("accept failed");
            continue;
        }

        //为每个客户端连接创建处理线程
        std::thread([this, clientfd, client_address](){
            //读类型
            uint8_t type;
            if(recv(clientfd, &type, 1, MSG_WAITALL) != 1){
                close(clientfd); return;
            }

            //读载荷长度
            uint32_t payloadLen = 0;
            if(recv(clientfd, &payloadLen, 4, MSG_WAITALL) != 4){
                close(clientfd); return;
            }
            payloadLen = ntohl(payloadLen);

            //读载荷
            std::vector<char> buf(payloadLen);
            if(recv(clientfd, buf.data(), payloadLen, MSG_WAITALL) != (int)payloadLen){
                close(clientfd); return;
            }

            std::string ip = inet_ntoa(client_address.sin_addr);

            if(type == MSG_TYPE_TCP_PRIVATE){   //私聊消息
                //反序列化MsgData
                MsgData msg = deserializeMsgData(buf.data(), payloadLen);
                //发射信号
                emitMessageReceived(msg.senderId, msg.senderName, ip, msg.message);
            }else if(type == MSG_TYPE_TCP_GROUP_HELLO){ //群聊连接
                if(!m_groupChat){
                    close(clientfd);
                    return;
                }

                std::string helloPayload(buf.begin(), buf.end());

                //群聊长连接交给GroupChat
                QMetaObject::invokeMethod(m_groupChat, [this,clientfd,helloPayload,ip](){
                        m_groupChat->handleConnection(clientfd,helloPayload,ip);
                    },Qt::QueuedConnection);
                return;
            }

            if(type == MSG_TYPE_TCP_GROUP_DATA){    //群消息
                //群消息只能出现在已经完成HELLO握手，并由GroupChat接管的TCP长连接上
                close(clientfd);
                return;
            }
            close(clientfd);
        }).detach();
    }
    close(serverfd);
    std::cout << "TCP服务器线程已退出" << std::endl;
}

void Chat::cleanOfflineThread(){
    constexpr auto timeout = std::chrono::seconds(10); //超过10秒未收到广播视为离线
    while(m_running) {
        std::this_thread::sleep_for(std::chrono::seconds(5)); //每5秒检查一次
        bool changed = false;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            auto now = std::chrono::steady_clock::now();
            for(auto it = m_peers.begin(); it != m_peers.end(); ){
                if(now - it->second.lastSeen > timeout){
                    it = m_peers.erase(it);
                    changed = true;
                }else{
                    ++it;
                }
            }
        }
        if(changed){
            //通知QML更新列表
            QMetaObject::invokeMethod(this, [this](){
                emit onlineUsersChanged();
            }, Qt::QueuedConnection);
        }
    }
}

QString Chat::localIp() const {
    return QString::fromStdString(m_localIp);
}

QString Chat::localId() const {
    return QString::fromStdString(m_localId);
}

QString Chat::localName() const{
    return QString::fromStdString(m_localName);
}

void Chat::setGroupChat(GroupChat *groupChat)
{
    m_groupChat = groupChat;
}

void Chat::setLocalName(const QString &name)
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_localName = name.toStdString();
    }

    //发送信号同步前端自己的名字
    QMetaObject::invokeMethod(this, [this](){
        emit onlineUsersChanged();
    }, Qt::QueuedConnection);
}