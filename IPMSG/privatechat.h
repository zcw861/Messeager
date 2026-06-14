// Module
// File: privatechat.h   Version: 0.1.0   License: AGPLv3
// Created:  ZhouChengWei     2026-06-9 19:29:14
// Description:
//     部分函数实现基础发现用户user、私聊功能
//
//     [v0.1.1] ZhouChengWei   2026-06-11 21:37:57
//         * 将0.1.0版本的posix socket函数封装为c++，提供给qml进行交互
//     [v0.1.2] ZhouChengWei    2026-06-11 22:19:35
//         * 添加了离线检测以及离线清理功能
//     [v0.1.3] HeZhiyuan    2026-06-13 14:02:50
//         * 移除start()和sendMessageToUser()的Q_INVOKABLE。
//           PrivateChat不再直接暴露给QML。
//           网络调用统一由AppController转发。
//     [v0.1.4]  ZhouChengWei    2026-06-14 15:37:05
//         * 添加了用于关闭阻塞调用的文件描述符

#ifndef PRIVATECHAT_H
#define PRIVATECHAT_H

#include <QObject>
#include <QVariantList>
#include <unordered_map>
#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include <chrono>

//用户结构体
struct UserInfo {
    std::string name;
    std::string ip;
    std::chrono::steady_clock::time_point lastSeen; //最后活跃时刻
};

class PrivateChat : public QObject
{
    Q_OBJECT
    //通知在线用户的前后端交互
    Q_PROPERTY(QVariantList onlineUsers READ onlineUsers NOTIFY onlineUsersChanged)

public:
    explicit PrivateChat(QObject *parent = nullptr);
    ~PrivateChat();

    QVariantList onlineUsers() const;

    void start(const QString &userName);    //启动程序
    void sendMessageToUser(const QString &ip, const QString &msg);  //发送消息

signals:
    void onlineUsersChanged();  //通知在线用户变化
    void messageReceived(const QString &fromName, const QString &fromIp, const QString &message);   //通知收到消息

private:
    void broadcastThread(); //广播线程
    void listenThread();    //监听广播线程
    void tcpServerThread(); //收发消息线程
    void cleanOfflineThread(); //清理离线用户线程

    //线程安全的信号发射
    void emitMessageReceived(const std::string &name, const std::string &ip, const std::string &msg);

    std::unordered_map<std::string, UserInfo> m_peers;  //ip与用户的映射
    mutable std::mutex m_mutex; //锁，用于并发时保护m_peers的资源访问
    std::string m_localName;    //当前用户名字
    std::atomic<bool> m_running{false}; //当前程序是否已经启动

    std::thread m_broadcastThread;
    std::thread m_listenThread;
    std::thread m_serverThread;
    std::thread m_cleanThread;

    int m_udp_listenFd = -1;
    int m_tcp_serverFd = -1;
};

#endif // PRIVATECHAT_H
