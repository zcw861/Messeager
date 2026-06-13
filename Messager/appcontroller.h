// Module
// File: appcontroller.cpp   Version: 0.1.0   License: AGPLv3
// Created: HeZhiyuan      2026-06-13 14:46:30
// Description:AppController接收界面操作并更新前端所需的数据
//
#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>

#include <QtQml/qqmlregistration.h>

#include "databasemanager.h"
#include "privatechat.h"

class AppController : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QVariantList peers
                   READ peers
                       NOTIFY peersChanged
                           FINAL)

    Q_PROPERTY(QVariantList messages
                   READ messages
                       NOTIFY messagesChanged
                           FINAL)

    Q_PROPERTY(QString lastError
                   READ lastError
                       NOTIFY lastErrorChanged
                           FINAL)

    Q_PROPERTY(bool ready
                   READ ready
                       NOTIFY readyChanged
                           FINAL)

public:
    explicit AppController(QObject *parent = nullptr);

    QVariantList peers() const;
    QVariantList messages() const;
    QString lastError() const;
    bool ready() const;

    Q_INVOKABLE bool initialize(const QString &userName);

    Q_INVOKABLE void selectPeer(const QString &peerId);

    Q_INVOKABLE void clearConversation();

    Q_INVOKABLE void sendMessage(const QString &peerId,
                                 const QString &username,
                                 const QString &ip,
                                 const QString &content);

signals:
    void peersChanged();
    void messagesChanged();
    void lastErrorChanged();
    void readyChanged();

    void operationFailed(const QString &message);

private:
    void synchronizeOnlineUsers();

    void handleMessageReceived(const QString &fromName,
                               const QString &fromIp,
                               const QString &message);

    void refreshPeers();
    void refreshMessages();
    void reportError(const QString &message);

private:
    DatabaseManager m_database;
    PrivateChat m_privateChat;

    QVariantList m_peers;
    QVariantList m_messages;

    QString m_currentPeerId;
    QString m_lastError;

    bool m_ready{false};
};