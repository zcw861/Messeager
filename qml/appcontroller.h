// Module
// File: appcontroller.h   Version: 0.1.0   License: AGPLv3
// Created: HeZhiyuan      2026-06-13 14:46:30
// Description: 定义应用控制器，负责协调QML界面、数据库层和网络通信层，
//              并向QML暴露用户列表、聊天记录、运行状态和错误信息。
//
//     [v0.1.2] HeZhiyuan    2026-06-18 19:03:47
//         *修改消息接收处理函数，增加发送者peerId参数
#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QUrl>
#include <QSet>
#include <QtQml/qqmlregistration.h>

#include "databasemanager.h"
#include "privatechat.h"
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

public:
    explicit AppController(QObject *parent = nullptr);

    ~AppController() override;

    QVariantList peers() const; //获取当前用户列表
    QVariantList messages() const;  //获取当前选中用户的聊天记录
    QString lastError() const;  //获取最近一次错误信息
    bool ready() const; //查询是否初始化完成

    Q_INVOKABLE bool initialize(const QString &userName);   //初始化数据库和网络聊天服务，如果已经初始化则直接返回 true

    Q_INVOKABLE void selectPeer(const QString &peerId); //选择聊天对象并加载其聊天记录

    Q_INVOKABLE void clearConversation();   //清除中的当前会话选择和消息缓存，使界面回到未选择用户状态

    Q_INVOKABLE bool deletePeer(const QString &peerId); //删除指定用户及其本地聊天记录

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

signals:
    void peersChanged();    //用户列表发生变化时发出，通知QML重新读取peers属性
    void messagesChanged();     //当前聊天消息列表发生变化时发出，通知QML刷新聊天列表
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

private:
    DatabaseManager m_database; //本地SQLite数据库管理对象
    PrivateChat m_privateChat;  //局域网用户发现和消息收发对象

    TranslateFile m_translateFile; //文件传输后端对象

    QVariantList m_peers;   //提供给QML的用户列表缓存
    QVariantList m_messages;    //当前聊天对象的消息列表缓存

    QString m_currentPeerId;    //当前选中聊天对象的唯一标识
    QString m_lastError;    //最近一次产生的错误信息

    bool m_ready{false};    //数据库和网络服务是否已经完成初始化
};
