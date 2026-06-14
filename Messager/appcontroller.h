// Module
// File: appcontroller.h   Version: 0.1.0   License: AGPLv3
// Created: HeZhiyuan      2026-06-13 14:46:30
// Description: 定义应用控制器，负责协调QML界面、数据库层和网络通信层，
//              并向QML暴露用户列表、聊天记录、运行状态和错误信息。
//
#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QSet>
#include <QtQml/qqmlregistration.h>

#include "databasemanager.h"
#include "privatechat.h"

class AppController : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    //提供给QML的用户列表，每个元素包含peerId、username、ip等字段
    Q_PROPERTY(QVariantList peers READ peers NOTIFY peersChanged FINAL)

    //提供给QML的当前聊天对象的历史消息列表
    Q_PROPERTY(QVariantList messages READ messages NOTIFY messagesChanged FINAL)

    //最近一次的错误信息
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged FINAL)

    //是否已经完成数据库和网络服务初始化
    Q_PROPERTY(bool ready READ ready NOTIFY readyChanged FINAL)

public:
    explicit AppController(QObject *parent = nullptr);

    QVariantList peers() const; //获取当前用户列表
    QVariantList messages() const;  //获取当前选中用户的聊天记录
    QString lastError() const;  //获取最近一次错误信息
    bool ready() const; //查询是否初始化完成

    Q_INVOKABLE bool initialize(const QString &userName);   //初始化数据库和网络聊天服务

    Q_INVOKABLE void selectPeer(const QString &peerId); //选择聊天对象并加载其聊天记录

    Q_INVOKABLE void clearConversation();   //清除中的当前会话选择和消息缓存

    Q_INVOKABLE bool deletePeer(const QString &peerId); //删除指定用户及其本地聊天记录

    //向指定局域网用户发送消息并保存本地聊天记录
    Q_INVOKABLE void sendMessage(const QString &peerId,
                                 const QString &username,
                                 const QString &ip,
                                 const QString &content);

signals:
    void peersChanged();    //用户列表发生变化时发出，通知QML重新读取peers属性
    void messagesChanged();     //当前聊天消息列表发生变化时发出
    void lastErrorChanged();    //近一次错误信息发生变化时发出
    void readyChanged();    //控制器初始化状态发生变化时发出
    void peerDeleted(const QString &peerId);    //删除成功后通知QML。
    void operationFailed(const QString &message);   //业务操作失败时发出

private:
    void synchronizeOnlineUsers();  //从PrivateChat读取在线用户，并同步到数据库

    //处理网络层接收到的聊天消息
    void handleMessageReceived(const QString &fromName,
                               const QString &fromIp,
                               const QString &message);

    void refreshPeers();    //从数据库重新读取用户列表
    void refreshMessages();     //从数据库重新读取当前聊天对象的消息记录
    void reportError(const QString &message);   //统一记录并报告业务错误

private:
    DatabaseManager m_database; //本地SQLite数据库管理对象
    PrivateChat m_privateChat;  //局域网用户发现和消息收发对象

    QVariantList m_peers;   //提供给QML的用户列表缓存
    QVariantList m_messages;    //当前聊天对象的消息列表缓存

    QString m_currentPeerId;    //当前选中聊天对象的唯一标识
    QString m_lastError;    //最近一次产生的错误信息

    bool m_ready{false};    //数据库和网络服务是否已经完成初始化
};