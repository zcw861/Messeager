// Module
// File: appcontroller.h   Version: 0.1.0   License: AGPLv3
// Created: HeZhiyuan      2026-06-13 14:46:30
// Description: 定义应用控制器，负责协调QML界面、数据库层和网络通信层，
//              并向QML暴露用户列表、聊天记录、运行状态和错误信息。
//
//     [v0.1.2] HeZhiyuan    2026-06-18 19:03:47
//         *修改消息接收处理函数，增加发送者peerId参数
//     [v0.1.3] ZhouChengWei    2026-06-22 11:29:12
//         * 添加了获取本机IP的函数
//     [v0.1.4] HeZhiyuan    2026-06-23 17:13:52
//         * 新增：createGroup()
//           接收群名称和群成员列表，并返回网络层生成的真实groupId
//     [v0.1.5] ZhouChengWei    2026-06-27 18:04:57
//         * 添加了退出/解散群聊函数
//     [v0.1.6] HeZhiyuan    2026-06-29 23:49:41
//         * 群聊列表增加isActive状态
//           新增：删除已退出群聊的QML调用接口

#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QUrl>
#include <QSet>
#include <QtQml/qqmlregistration.h>
#include <QStringList>
#include <QHash>
#include "databasecore.h"
#include "databasecheck.h"
#include "peerdatabase.h"
#include "privatechatdatabase.h"
#include "groupchatdatabase.h"
#include "chat.h"
#include "groupchat.h"
#include "translatefile.h"

class AppController : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    //提供给QML的用户列表，每个元素包含peerId、username、ip、online和updatedAt
    Q_PROPERTY(QVariantList peers READ peers NOTIFY peersChanged FINAL)

    //提供给QML的当前选中用户的聊天记录
    Q_PROPERTY(QVariantList messages READ messages NOTIFY messagesChanged FINAL)

    //最近一次的错误信息
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged FINAL)

    //是否已经完成数据库和网络服务初始化
    Q_PROPERTY(bool ready READ ready NOTIFY readyChanged FINAL)

    //提供给创建群聊窗口的候选成员。
    //第一项为本机用户，后续成员来自数据库peers表。
    //数据库中的在线和离线用户都会保留。
    Q_PROPERTY(QVariantList groupCandidates READ groupCandidates NOTIFY groupCandidatesChanged FINAL)

    //提供给QML的群聊列表，每个元素由GroupChatDatabase::loadGroups()返回，主要包含：
    //groupId、groupName、creatorId、isActive、memberCount、memberSummary、createdAt和updatedAt
    Q_PROPERTY(QVariantList groups READ groups NOTIFY groupsChanged FINAL)

    //提供给QML的当前选中群聊的成员列表，成员数据来自DatabaseManager::loadGroupMembers()，
    //其中包含成员ID、名称、当前IP、在线状态和isSelf标记
    Q_PROPERTY(QVariantList groupMembers READ groupMembers NOTIFY groupMembersChanged FINAL)

    //提供给QML的当前选中群聊的历史消息，每个元素包含发送者ID、发送者名称、消息内容、发送方向和时间
    Q_PROPERTY(QVariantList groupMessages READ groupMessages NOTIFY groupMessagesChanged FINAL)

public:
    explicit AppController(QObject *parent = nullptr);

    ~AppController() override;

    QVariantList peers() const; //获取当前用户列表
    QVariantList groupCandidates() const;   //返回创建群聊窗口使用的全部候选成员
    QVariantList messages() const;  //获取当前选中用户的聊天记录
    QString lastError() const;  //获取最近一次错误信息
    bool ready() const; //查询是否初始化完成

    //返回当前缓存的群聊列表
    QVariantList groups() const;

    //返回当前选中群聊的成员列表
    QVariantList groupMembers() const;

    //返回当前选中群聊的历史消息列表
    QVariantList groupMessages() const;

    Q_INVOKABLE bool initialize(const QString &userName);   //初始化数据库和网络聊天服务，如果已经初始化则直接返回 true

    Q_INVOKABLE void selectPeer(const QString &peerId); //选择聊天对象并加载其聊天记录

    Q_INVOKABLE void clearConversation();   //清除中的当前会话选择和消息缓存，使界面回到未选择用户状态

    Q_INVOKABLE bool deletePeer(const QString &peerId); //删除指定用户及其本地聊天记录

    Q_INVOKABLE QString localIp(); //获取自己IP


    Q_INVOKABLE QUrl localFileUrl(const QString &pathOrUrl); //把本地文件路径转换为 QML Image.source 可用的 file URL

    //向指定局域网用户发送消息并在数据库中保存本机发送记录
    Q_INVOKABLE void sendMessage(const QString &peerId,
                                 const QString &username,
                                 const QString &ip,
                                 const QString &content);
    //向指定用户发送文件
    Q_INVOKABLE void sendFile(const QString &peerId,
                              const QString &username,
                              const QString &ip,
                              const QUrl &fileUrl);

    //调用系统默认程序打开本地文件
    Q_INVOKABLE void openLocalFile(const QString &url);

    //接受文件请求
    Q_INVOKABLE void acceptFile(const QString &ip, const QUrl &saveUrl);

    //接收普通文件到data/download。
    Q_INVOKABLE void acceptFileToDownload(const QString &ip, const QString &fileName);

    //拒绝文件请求
    Q_INVOKABLE void rejectFile(const QString &ip);

    //自动接收图片文件，保存到程序data目录
    Q_INVOKABLE void acceptImageFile(const QString &ip, const QString &fileName);

    Q_INVOKABLE QString savedUserName() const; //读取上次登录用户名
    Q_INVOKABLE void clearSavedUserName();     //清除上次登录用户名

    Q_INVOKABLE bool updateMyName(const QString &newName); //更新本机用户名

    //选择一个群聊，并加载该群的成员和历史消息
    Q_INVOKABLE void selectGroup(const QString &groupId);

    //退出当前群聊会话
    Q_INVOKABLE void clearGroupConversation();

    //解散群聊（群主）
    Q_INVOKABLE bool dismissGroup(const QString &groupId);

    //退出群聊（成员）
    Q_INVOKABLE bool leaveGroup(const QString &groupId);

    //彻底删除已经退出的群聊及其本地成员和消息记录，仍处于活动状态的群聊不能删除
    Q_INVOKABLE bool deleteExitedGroup(const QString &groupId);

    //创建一个新的群聊
    //成功时返回网络层生成的真实groupId
    //失败时返回空字符串，并通过operationFailed信号报告原因
    Q_INVOKABLE QString createGroup(const QString &groupName, const QVariantList &members);

    //向指定群聊发送文本消息
    Q_INVOKABLE bool sendGroupMessage(const QString &groupId, const QString &content);
signals:
    void peersChanged();    //用户列表发生变化时发出，通知QML重新读取peers属性
    void groupCandidatesChanged();      //群聊候选成员发生变化后，通知QML重新读取groupCandidates
    void messagesChanged();     //当前聊天消息列表发生变化时发出，通知QML刷新聊天列表

    //数据库中的群聊列表发生变化后发出，QML收到信号后会重新读取groups
    void groupsChanged();
    //当前群聊成员列表发生变化后发出
    void groupMembersChanged();
    //当前群聊消息列表发生变化后发出
    void groupMessagesChanged();

    void lastErrorChanged();    //近一次错误信息发生变化时发出
    void readyChanged();    //控制器初始化状态发生变化时发出
    void peerDeleted(const QString &peerId);    //删除成功后通知QML清理选中状态
    void groupDeleted(const QString &groupId);      //彻底删除群聊成功后清理当前群聊状态
    void operationFailed(const QString &message);   //业务操作失败时发出

    void fileRequestReceived(const QString &fromfp, const QString &fileName, qint64 fileSize); //收到对方文件发送请求
    void fileTransferProgress(const QString &ip, const QString &fileName, int percent); //传输进度条
    void fileTransferFinished(const QString &ip, const QString &fileName, bool isSuccess); //文件传输结果

private:    
    void synchronizeOnlineUsers();  //从PrivateChat读取在线用户，并同步到数据库

    //处理网络层接收到的聊天消息，将发送者和消息保存到数据库
    void handleMessageReceived( const QString &fromId,
                                const QString &fromName,
                                const QString &fromIp,
                                const QString &message);

    //处理网络层接收到的群聊消息
    void handleGroupMessageReceived( const QString &groupId,
                                     const QString &fromId,
                                     const QString &fromName,
                                     const QString &content);

    //处理收到的UDP群邀请
    void handleGroupInviteReceived(const QString &groupId,
                                   const QString &groupName,
                                   const QString &inviterId,
                                   const QString &inviterName,
                                   const QString &inviterIp,
                                   const QStringList &memberRecords);

    //处理网络层报告的群成员退出事件，同步删除数据库成员并刷新群聊界面
    void handleGroupMemberLeft(const QString &groupId, const QString &memberId);

    //处理网络层报告的群聊解散事件，删除本地群聊和相关数据
    void handleGroupDismissed(const QString &groupId);

    //从数据库恢复全部GroupChat会话
    void restoreGroupSessions();

    static bool containsProtocolSeparator(const QString &text);

    void refreshPeers();    //从数据库重新读取用户列表，并在内容变化时通知 QML
    void refreshMessages();     //从数据库重新读取当前聊天对象的消息记录,并在内容变化时通知 QML
    void reportError(const QString &message);   //统一记录并报告业务错误

    //从数据库重新读取全部群聊，只有新数据与缓存不同时才更新属性并发出groupsChanged
    void refreshGroups();

    //读取当前选中群聊的成员，没有选中群聊时清空成员缓存
    void refreshGroupMembers();

    //读取当前选中群聊的历史消息，没有选中群聊时清空群消息缓存
    void refreshGroupMessages();

    //处理传输完成的函数
    void handleFileTransferFinished(const QString &ip, const QString &fileName, bool isSuccess);

private:
    DatabaseCore m_databaseCore;
    //用户身份和在线用户数据库
    PeerDatabase m_peerDatabase;
    //私聊消息数据库
    PrivateChatDatabase m_privateChatDatabase;
    //群聊、群成员和群消息数据库
    GroupChatDatabase m_groupChatDatabase;
    Chat m_chat;  //局域网用户发现和消息收发对象
    GroupChat m_groupChat;  //群聊对象

    TranslateFile m_translateFile; //文件传输后端对象

    QVariantList m_peers;       //提供给QML的用户列表缓存
    QVariantList m_messages;    //当前私聊对象的消息列表缓存

    //数据库中已经持久化的全部群聊
    //群聊创建、收到邀请或删除群聊后，需要重新加载该列表
    QVariantList m_groups;

    //当前选中群聊的成员列表
    //切换群聊时由loadGroupMembers()重新读取
    QVariantList m_groupMembers;

    //当前选中群聊的历史消息
    //切换群聊或收到新群消息后由loadGroupMessages()重新读取
    QVariantList m_groupMessages;

    QString m_currentPeerId;    //当前选中私聊对象的peerId

    //当前选中群聊的groupId
    //私聊与群聊分别保存选择状态，避免把peerId和groupId混用
    QString m_currentGroupId;

    QString m_lastError;    //最近一次产生的错误信息

    bool m_ready{false};    //数据库和网络服务是否已经完成初始化

    //自动接收图片时，记录发送方IP对应的本地保存路径
    QHash<QString, QString> m_pendingImageSavePaths;
};
