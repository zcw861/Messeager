// Module
// File: groupchat.h   Version: 0.1.0   License: AGPLv3
// Created: ZhouChengWei      2026-06-22 16:29:25
// Description:
//     群聊相关的函数，在比较了组播与UDP单播之后决定使用单播来实现群聊邀请
//     组播在跨子网时才会有优势，但本项目基于纯局域网p2p,所以现有架构来说UDP单播更有优势
//     此群聊收发消息使用TCP全连接，群聊成员两两连接，发消息按照群成员IP依次发送
//     因此，群聊模块为聊天模块的子模块
//     [v0.1.1] ZhouChengWei    2026-06-23 13:23:06
//         * 添加了清理函数，以及接收函数
//     [v0.1.2] ZhouChengWei    2026-06-23 15:21:32
//         * 添加了群聊邀请处理函数

#pragma once

#include <QObject>

#include <sys/epoll.h>
#include <fcntl.h>
#include <unistd.h>
#include <atomic>
#include <unordered_map>
#include <thread>
#include <mutex>

#include "common.h"

//群聊结构体
struct GroupSession
{
    std::vector<UserInfo> members; //所有成员（包括自己）
    std::unordered_map<std::string, int> fds; //memberId -> socket fd
};

class Chat;

class GroupChat : public QObject
{
    Q_OBJECT

public:
    explicit GroupChat(Chat *chat, QObject *parent = nullptr);
    ~GroupChat();

    QString createGroup(const std::vector<UserInfo> &groupMembers); //创建群聊
    void sendMsgToGroup(const std::string &groupId, const std::string &content); //发送群聊消息
    void addConnection(const std::string &groupId, const std::string &memberId, int fd);    //添加链接

signals:
    void groupMessageReceived(const QString &groupId, const QString &fromId,
                              const QString &fromName, const QString &content);

public slots:
    void handleGroupInvite(const QString &groupId, const QString &inviterId,
                           const QString &inviterName, const QString &inviterIp);   //处理群聊邀请


private:
    static bool setNonBlocking(int fd);
    void epollLoop();   //epoll事件循环
    int connectToMember(const std::string &ip);     //建立到指定IP的TCP连接
    void cleanupFd(int fd);  //清理断开的连接
    int recvFull(int fd, void *buf, size_t len); //接收对应字节数据

    mutable std::mutex m_mutex;
    std::unordered_map<std::string, GroupSession> m_sessions; //群ID与群聊会话的映射
    std::unordered_map<int, std::pair<std::string, std::string>> m_fdInfo;  //fd的会话映射
    Chat *m_chat;

    int m_epollFd = -1;
    std::thread m_epollThread;
    std::atomic<bool> m_running{false};
};
