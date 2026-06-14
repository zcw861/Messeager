// Module
// File: privatechat.cpp   Version: 0.1.0   License: AGPLv3
// Created:  ZhouChengWei     2026-06-09 19:30:06
// Description:
//      见privatechat.h文件的描述，本实现的广播，接收广播以及收发消息各一个线程
//     [v0.1.1]  ZhouChengWei   2026-06-11 08:39:46
//         * 添加了错误处理
//     [v0.1.2]  ZhouChengWei   2026-06-11 21:39:59
//         * 用c++进行封装，提供信号，与qml进行交互
//     [v0.1.3]  ZhouChengWei   2026-06-11 22:22:08
//         * 添加了离线检测以及清理离线功能
//     [v0.1.4]  ZhouChengWei   2026-06-12 11:44:33
//         * 添加了清理旧的socket连接，防止下次运行时导致的tcp bind fail而收不到消息
//     [v0.1.5] ZhouChengWei    2026-06-14 14:46:58
//         * 添加了关闭各种阻塞调用，防止每次重启发消息会导致第一次发送消息接收不到的bug

#include "privatechat.h"

#include <iostream>
#include <cstring>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#define UDP_PORT 45454  // UDP端口
#define TCP_PORT 45455  // TCP端口
#define BUF_SIZE 1024   // 缓冲区大小

PrivateChat::PrivateChat(QObject *parent) : QObject(parent){}

PrivateChat::~PrivateChat()
{
    m_running = false;

    //关闭socket让阻塞调用返回
    if(m_tcp_serverFd != -1){
        shutdown(m_tcp_serverFd, SHUT_RDWR);
    }
    if(m_udp_listenFd != -1){
        shutdown(m_tcp_serverFd, SHUT_RDWR);
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

    if (m_tcp_serverFd != -1) {
        close(m_tcp_serverFd);
        m_tcp_serverFd = -1;
    }
    if (m_udp_listenFd != -1) {
        close(m_udp_listenFd );
        m_udp_listenFd  = -1;
    }
}

QVariantList PrivateChat::onlineUsers() const
{
    std::lock_guard<std::mutex> lock(m_mutex);  //保护m_peers同时只能被一个线程访问
    //添加当前在线用户
    QVariantList list;
    for(const auto &[ip, info] : m_peers){
        QVariantMap user;
        user["name"] = QString::fromStdString(info.name);
        user["ip"] = QString::fromStdString(info.ip);
        list.append(user);
    }
    return list;
}

void PrivateChat::start(const QString &userName)
{
    if(m_running) return;  //防止重复启动

    m_localName = userName.toStdString();
    m_running = true;

    //启动四个工作线程
    m_broadcastThread = std::thread(&PrivateChat::broadcastThread, this);
    m_listenThread = std::thread(&PrivateChat::listenThread, this);
    m_serverThread = std::thread(&PrivateChat::tcpServerThread, this);
    m_cleanThread = std::thread(&PrivateChat::cleanOfflineThread, this);

    std::cout << "信使聊天服务已启动，用户: " << m_localName << std::endl;
}

void PrivateChat::sendMessageToUser(const QString &ip, const QString &msg)
{
    std::string ipStr = ip.toStdString();
    std::string msgStr = msg.toStdString();

    std::thread([ipStr, msgStr](){
        int sendfd = socket(PF_INET, SOCK_STREAM, 0);
        if(sendfd < 0) {
            perror("tcp send socket create fail");
            return;
        }

        sockaddr_in user{};
        user.sin_family = AF_INET;
        user.sin_port = htons(TCP_PORT);
        inet_pton(AF_INET, ipStr.c_str(), &user.sin_addr);

        if(::connect(sendfd, (sockaddr*)&user, sizeof(user)) < 0) {
            std::cerr << "连接 " << ipStr << " 失败" << std::endl;
            close(sendfd);
            return;
        }

        send(sendfd, msgStr.c_str(), msgStr.size(), 0);
        close(sendfd);
    }).detach();
}

void PrivateChat::emitMessageReceived(const std::string &name, const std::string &ip, const std::string &msg)
{
    //线程安全地发射信号到主线程（由于不是使用QThread,只能通过QMetaObject::invokeMethod间接发射信号）
    QMetaObject::invokeMethod(this, [this, name, ip, msg](){
        emit messageReceived(
            QString::fromStdString(name),
            QString::fromStdString(ip),
            QString::fromStdString(msg)
            );
    }, Qt::QueuedConnection);
}

void PrivateChat::broadcastThread()
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
        sendto(listenfd, m_localName.c_str(), m_localName.size(), 0,
               (struct sockaddr*)&address, sizeof(address));
        sleep(2);  //每2秒广播一次
    }

    close(listenfd);
    std::cout << "UDP广播线程已退出" << std::endl;
}

void PrivateChat::listenThread()
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

    char userName[BUF_SIZE];

    while(m_running){
        sockaddr_in sender{};
        socklen_t len = sizeof(sender);
        memset(&sender, 0, sizeof(sender));

        int n = recvfrom(listenfd, userName, BUF_SIZE - 1, 0, (sockaddr*)&sender, &len);
        if(n <= 0) continue;

        userName[n] = 0;
        std::string ip = inet_ntoa(sender.sin_addr);

        //更新在线用户列表
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            auto now = std::chrono::steady_clock::now();
            m_peers[ip] = {std::string(userName), ip, now};
        }

        //通知QML更新UI
        QMetaObject::invokeMethod(this, [this](){
            emit onlineUsersChanged();
        }, Qt::QueuedConnection);

        std::cout << "发现用户: " << userName << " (" << ip << ")" << std::endl;
    }

    close(listenfd);
    std::cout << "UDP监听线程已退出" << std::endl;
}

void PrivateChat::tcpServerThread()
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
            char buffer[BUF_SIZE];
            int n = recv(clientfd, buffer, BUF_SIZE - 1, 0);

            if(n > 0){
                buffer[n] = 0;
                std::string ip = inet_ntoa(client_address.sin_addr);

                //获取发送者名称
                std::string senderName = "Unknown";
                {
                    std::lock_guard<std::mutex> lock(m_mutex);
                    auto it = m_peers.find(ip);
                    if (it != m_peers.end()) {
                        senderName = it->second.name;
                    }
                }

                std::cout << "收到消息来自 " << senderName << "(" << ip << "): " << buffer << std::endl;

                //发射消息接收信号
                emitMessageReceived(senderName, ip, std::string(buffer));
            }

            close(clientfd);
        }).detach();
    }

    close(serverfd);
    std::cout << "TCP服务器线程已退出" << std::endl;
}

void PrivateChat::cleanOfflineThread(){
    constexpr auto timeout = std::chrono::seconds(6); //超过6秒未收到广播视为离线
    while(m_running) {
        std::this_thread::sleep_for(std::chrono::seconds(3)); //每3秒检查一次
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