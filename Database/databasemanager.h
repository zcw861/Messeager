// Module
// File: databasemanager.h   Version: 0.1.0   License: AGPLv3
// Created: HeZhiyuan      2026-06-07 21:33:33
// Description: 定义数据库管理类，负责SQLite数据库连接、表结构初始化、
//              用户信息管理、聊天记录管理和在线用户状态同步。
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

    bool open();    //打开本地SQLite数据库
    bool initSchema();  //初始化数据库表结构和索引
    QString databasePath() const;   //获取当前数据库文件的完整路径
    QString lastError() const;  //获取数据库管理器最近记录的错误信息
    QVariantList loadPeers();   //从peers表读取全部用户
    bool upsertPeer(const QString &peerId,const QString &username,const QString &ip,bool online);   //插入或更新一个用户
    bool deletePeer(const QString &peerId);    //删除指定用户
    //保存一条聊天消息
    bool saveMessage(const QString &peerId,bool fromMe,const QString &content);
    //读取与指定用户之间的最近聊天记录
    QVariantList loadMessages(const QString &peerId, int limit = 100);
    //将网络层提供的在线用户列表同步到数据库
    bool synchronizePeers(const QVariantList &onlinePeers);
private:
    bool execSql(const QString &sql);   //执行一条不需要参数绑定的SQL语句

private:
    QSqlDatabase m_db;  //SQLite数据库连接句柄
    QString m_databasePath; //数据库文件的完整路径
    QString m_lastError;    //最近一次数据库操作产生的错误信息
    QString m_connectionName;   //当前对象使用的唯一数据库连接名称
};
