// Module
// File: peerdatabase.h   Version: 0.1.0   License: AGPLv3
// Created: HeZhiyuan      2026-06-24 15:05:25
// Description:     定义本机身份和用户信息相关的数据库操作
//
#pragma once

#include <QString>
#include <QVariantList>

#include "database_global.h"

class DatabaseCore;
class DATABASE_EXPORT PeerDatabase final
{
public:
    //接收DatabaseCore引用，使用DatabaseCore已经打开的默认连接
    explicit PeerDatabase(DatabaseCore &databaseCore);

    //创建local_peer_id和peers表
    bool initSchema();

    //读取或创建本机永久UUID
    bool loadOrCreateLocalPeerId(const QString &candidatePeerId, QString &persistentPeerId);

    //读取全部用户
    bool loadPeers(QVariantList &peers);

    //插入新用户或更新已有用户
    bool upsertPeer(const QString &peerId, const QString &username, const QString &ip, bool online);

    //删除用户，私聊消息由外键级联删除
    bool deletePeer(const QString &peerId);

    //同步网络层当前在线用户
    bool synchronizePeers(const QVariantList &onlinePeers);

    QString lastError() const;

private:
    //引用唯一连接管理对象，不拥有它
    DatabaseCore &m_databaseCore;

    QString m_lastError;
};
