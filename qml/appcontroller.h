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
#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QUrl>
#include <QSet>
#include <QtQml/qqmlregistration.h>

#include "databasemanager.h"
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

    //提供给QML的群聊列表，每个元素由DatabaseManager::loadGroups()返回，主要包含：
    //groupId、groupName、creatorId、memberCount、memberSummary、createdAt和updatedAt
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

    //接受文件请求
    Q_INVOKABLE void acceptFile(const QString &ip, const QUrl &saveUrl);

    //拒绝文件请求
    Q_INVOKABLE void rejectFile(const QString &ip);

    Q_INVOKABLE QString savedUserName() const; //读取上次登录用户名
    Q_INVOKABLE void clearSavedUserName();     //清除上次登录用户名

    Q_INVOKABLE bool updateMyName(const QString &newName); //更新本机用户名

    //选择一个群聊，并加载该群的成员和历史消息
    Q_INVOKABLE void selectGroup(const QString &groupId);

    //退出当前群聊会话
    Q_INVOKABLE void clearGroupConversation();

signals:
    void peersChanged();    //用户列表发生变化时发出，通知QML重新读取peers属性
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

    void refreshPeers();    //从数据库重新读取用户列表，并在内容变化时通知 QML
    void refreshMessages();     //从数据库重新读取当前聊天对象的消息记录,并在内容变化时通知 QML
    void reportError(const QString &message);   //统一记录并报告业务错误

    //从数据库重新读取全部群聊，只有新数据与缓存不同时才更新属性并发出groupsChanged
    void refreshGroups();

    //读取当前选中群聊的成员，没有选中群聊时清空成员缓存
    void refreshGroupMembers();

    //读取当前选中群聊的历史消息，没有选中群聊时清空群消息缓存
    void refreshGroupMessages();

private:
    DatabaseManager m_database; //本地SQLite数据库管理对象
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
};
