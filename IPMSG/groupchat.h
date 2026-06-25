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
//     [v0.2.0] ZhouChengWei    2026-06-25 17:53:13
//         * 完善所有群聊函数，实现群聊功能
//         * 添加了一系列键值对，用于确保获取每个人的消息
//         * 添加了确认/回复函数，用于建立群聊连接
//         * 重构成员TCP连接函数，添加多个锁用于保护各种并发情况

#pragma once

#include <QObject>
#include <QString>

#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "common.h"

//群成员信息结构体：包含ID、名字、IP和TCP端口
struct GroupMember
{
    std::string id;     //ID
    std::string name;   //名字
    std::string ip;     //IP
    uint16_t port = TCP_PORT;   //端口号
};

//群会话结构体：维护一个群的所有数据，包括群ID、群名、成员列表以及成员对应的TCP连接fd
struct GroupSession
{
    std::string groupId;    //群ID
    std::string groupName;  //群名称
    std::unordered_map<std::string, GroupMember> members;   // 成员ID -> 成员信息
    std::unordered_map<std::string, int> fds;               // 成员ID -> 已建立的TCP连接fd
};

class Chat;

class GroupChat : public QObject
{
    Q_OBJECT

public:
    explicit GroupChat(Chat *chat, QObject *parent = nullptr);
    ~GroupChat();

    //生成一个随机的10位数字群ID
    QString generateGroupId();

    //创建新群聊：向所有成员发送UDP邀请，并建立TCP连接
    bool createGroup(const QString &groupId, const QString &groupName,
                     const std::vector<UserInfo> &groupMembers);

    //恢复群聊（应用重启后，从数据库恢复群信息）：更新成员信息并补建TCP连接
    bool restoreGroup(const QString &groupId, const QString &groupName,
                      const std::vector<UserInfo> &groupMembers);

    //向指定群发送消息，消息将通过已有的TCP连接逐个发送给群内其他成员
    bool sendMsgToGroup(const std::string &groupId, const std::string &content);

    //当Chat模块收到对端主动发来的TCP连接并完成HELLO握手后，将连接移交给群聊模块
    void handleConnection(int fd, const std::string &helloPayload, const std::string &peerIp);

signals:
    //当收到群消息时发射信号，由AppController连接处理
    void groupMessageReceived(const QString &groupId, const QString &fromId,
                              const QString &fromName, const QString &content);

private:
    //确保epoll事件循环已经启动
    bool ensureEpollStarted();

    //为一个任务分配一个线程执行（主要用于建立连接、发送UDP邀请）
    void startWorker(std::function<void()> task);

    //检查群ID是否为10位数字
    static bool isTenDigitGroupId(const std::string &groupId);

    //将socket设置为非阻塞模式
    static bool setNonBlocking(int fd);

    //将UserInfo列表转换为GroupMember列表
    std::vector<GroupMember> toMembers(const std::vector<UserInfo> &groupMembers) const;

    //构建群聊邀请的UDP载荷（包含群ID、群名、邀请人ID、邀请人名字及完整成员列表）
    std::string buildInvitePayload(const std::string &groupId, const std::string &groupName,
                                   const std::string &inviterId, const std::string &inviterName,
                                   const std::vector<GroupMember> &members) const;

    //通过UDP单播向指定成员发送邀请
    bool sendInvite(const GroupMember &member, const std::string &invitePayload) const;

    //确保与所有peerId大于自己的成员建立TCP连接（避免重复连接）
    void ensureConnectToHigherIds(const std::string &groupId);

    //同上，但直接提供成员列表
    void ensureConnectToHigherIds(const std::string &groupId,
                                  const std::vector<GroupMember> &members);

    //对单个成员执行“建立TCP连接、发送HELLO、等待ACK、注册到epoll”的完整流程
    void connectAndRegister(const std::string &groupId, const GroupMember &member);

    //执行非阻塞TCP connect，并在成功后临时恢复阻塞模式以完成HELLO握手
    int connectToMember(const std::string &ip, const uint16_t port) const;

    //发送群聊HELLO帧（包含本机信息），用于向对端表明身份
    void sendHello(const int fd, const std::string &groupId, const GroupMember &self);

    //等待对端返回的群聊ACK确认帧，验证group是否匹配
    bool waitForHelloAck(const int fd, const std::string &groupId) const;

    //将一个已建立连接与群聊中的某个成员绑定，并注册到epoll监听
    bool addConnection(const std::string &groupId, const std::string &memberId, const int fd);

    //循环调用send直到整个数据包发送完毕或出错
    bool sendPacketAll(int fd, const std::vector<uint8_t> &packet) const;

    //epoll事件循环主函数
    void epollLoop();

    //从指定fd的缓冲区中解析出一个或多个完整TCP帧，进行消息处理
    bool handleBufferedFrames(int fd);

    //清理指定的fd：从epoll中移除、关闭socket、清理所有相关数据结构
    void cleanupFd(int fd);

private:
    //保护所有群会话数据的主锁
    std::mutex m_mutex;

    //防止多个线程同时向同一个TCP连接写入数据
    std::mutex m_sendMutex;

    //保护工作线程列表的锁
    std::mutex m_workerMutex;

    //群ID -> 群会话
    std::unordered_map<std::string, GroupSession> m_sessions;

    //fd -> (群ID, 成员ID)，用于查某个fd属于哪个群的哪个成员
    std::unordered_map<int, std::pair<std::string, std::string>> m_fdInfo;

    //每个fd独立保存尚未解析完的TCP缓冲数据，用于解决半包和粘包问题
    std::unordered_map<int, std::vector<uint8_t>> m_receiveBuffers;

    //防止对同一成员重复发起连接尝试的键集合
    std::unordered_set<std::string> m_connectings;

    //线程集合
    std::vector<std::thread> m_workerThreads;

    Chat *m_chat{nullptr};              //指向父Chat模块，用于获取本机信息等

    int m_epollFd{-1};                  //epoll实例的文件描述符
    std::thread m_epollThread;          //epoll事件循环线程
    std::atomic<bool> m_running{false}; //epoll线程运行标志
};