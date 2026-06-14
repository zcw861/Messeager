// Module
// File: databasemanager.h   Version: 0.1.0   License: AGPLv3
// Created: HeZhiyuan      2026-06-07 21:33:33
// Description:实现基础的数据库操作：打开、初始化、打印路径和报错、执行sql语句
//
//     [v0.1.2] HeZhiyuan    2026-06-08 15:36:22
//         *新增：读取用户列表、新增用户
//     [v0.1.3] HeZhiyuan    2026-06-09 22:37:04
//         *新增：聊天消息保存
//     [v0.1.4] HeZhiyuan    2026-06-11 17:11:07
//         *新增：读取历史记录
//     [v0.1.5] HeZhiyuan    2026-06-13 13:15:36
//         *修改数据库层文件路径，将database改为动态库
//     [v0.1.6] HeZhiyuan    2026-06-14 15:52:32
//         *新增：删除指定用户
#pragma once

#include <QObject>
#include <QString>
#include <QSqlDatabase>
#include <QVariantList>

#include "database_global.h"

class DATABASE_EXPORT DatabaseManager : public QObject
{
    Q_OBJECT

public:
    explicit DatabaseManager(QObject *parent = nullptr);

    ~DatabaseManager() override;

    bool open();
    bool initSchema();
    QString databasePath() const;
    QString lastError() const;
    QVariantList loadPeers();
    bool upsertPeer(const QString &peerId,const QString &username,const QString &ip,bool online);
    bool deletePeer(const QString &peerId);    //删除指定用户
    bool saveMessage(const QString &peerId,bool fromMe,const QString &content);
    QVariantList loadMessages(const QString &peerId, int limit = 100);
    bool synchronizePeers(const QVariantList &onlinePeers);
private:
    bool execSql(const QString &sql);

private:
    QSqlDatabase m_db;
    QString m_databasePath;
    QString m_lastError;
    QString m_connectionName;
};
