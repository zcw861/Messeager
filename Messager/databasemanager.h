// Module
// File: databasemanager.h   Version: 0.1.0   License: AGPLv3
// Created: HeZhiyuan      2026-06-07 21:33:33
// Description:实现基础的数据库操作：打开、初始化、打印路径和报错、执行sql语句
//
//     [v0.1.2] HeZhiyuan    2026-06-08 15:36:22
//         *新增：读取用户列表、新增用户
//     [v0.1.2] HeZhiyuan    2026-06-09 22:37:04
//         *新增：聊天消息保存
#pragma once

#include <QObject>
#include <QString>
#include <QSqlDatabase>
#include <QVariantList>

class DatabaseManager : public QObject
{
    Q_OBJECT

public:
    explicit DatabaseManager(QObject *parent = nullptr);

    bool open();
    bool initSchema();
    QString databasePath() const;
    QString lastError() const;
    QVariantList loadPeers();
    bool upsertPeer(const QString &peerId,const QString &username,const QString &ip,bool online);
    bool saveMessage(const QString &peerId,bool fromMe,const QString &content);

private:
    bool execSql(const QString &sql);

private:
    QSqlDatabase m_db;
    QString m_databasePath;
    QString m_lastError;
};
