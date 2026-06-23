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
//     [v0.1.5] ZhouChengWei    2026-06-14 17:29:06
//         * 添加了用于标识本地ip的变量
//     [v0.1.6] ZhouChengWei    2026-06-14 21:32:12
//         * 添加了获取本机IP的函数
//     [v0.2.0] ZhouChengWei    2026-06-18 17:53:47
//         * 将逻辑修改为用ID辨别唯一用户
//     [v0.2.1] HeZhiyuan    2026-06-18 22:11:02
//         * 新增：setLocalId()，允许控制层在网络线程启动前设置UUID
//     [v0.2.2] ZhouChengWei    2026-06-23 15:20:14
//         * 增加了群聊邀请信号

#pragma once

#include <QObject>
#include <QVariantList>
#include <QUuid>

#include <unordered_map>
#include <string>
#include <thread>
#include <atomic>
#include <mutex>

#include "common.h"

class GroupChat;

class Chat : public QObject
{
    Q_OBJECT

public:
    explicit Chat(QObject *parent = nullptr);
    ~Chat();

    QVariantList onlineUsers() const;

    void start(const QString &userName);    //启动程序
    void sendMessageToUser(const QString &ip, const QString &msg);  //发送消息

    bool setLocalId(const QString &localId);    //在网络线程启动前设置本机永久ID

    QString localIp() const;    //提供本机IP
    QString localId() const;    //提供本机ID
    QString localName() const;  //提供本机名字

signals:
    void onlineUsersChanged();  //通知在线用户变化
    void messageReceived(const QString &fromId, const QString &fromName, const QString &fromIp, const QString &message);   //通知收到消息
    void groupInviteReceived(const QString &groupId, const QString &inviterId,
                             const QString &inviterName, const QString &inviterIp);     //收到群聊邀请

private:
    void broadcastThread();     //广播线程
    void listenThread();        //监听广播线程
    void tcpServerThread();     //收发消息线程
    void cleanOfflineThread();  //清理离线用户线程

    //线程安全的信号发射
    void emitMessageReceived(const std::string &id, const std::string &name, const std::string &ip, const std::string &msg);

    std::unordered_map<std::string, UserInfo> m_peers;  //id与用户的映射
    mutable std::mutex m_mutex; //锁，用于并发时保护m_peers的资源访问
    std::string m_localName;    //当前用户名字
    std::string m_localIp;      //当前用户IP
    std::string m_localId;      //当前用户ID
    std::atomic<bool> m_running{false}; //当前程序是否已经启动

    std::thread m_broadcastThread;
    std::thread m_listenThread;
    std::thread m_serverThread;
    std::thread m_cleanThread;

    int m_udp_listenFd = -1;
    int m_tcp_serverFd = -1;
};
