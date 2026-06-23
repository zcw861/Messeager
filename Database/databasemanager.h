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
//     [v0.1.7] HeZhiyuan    2026-06-18 20:11:20
//         *新增：loadOrCreateLocalPeerId()，负责读取或创建本机永久peerId
//               normalizePeerId()，负责校验UUID并统一为不带大括号的格式
//     [v0.1.8] HeZhiyuan    2026-06-23 14:10:35
//         *新增: createGroup()，使用事务一次性保存群聊基本信息和全部群成员
//     [v0.1.9] HeZhiyuan    2026-06-23 14:36:17
//         *新增：loadGroups()和loadGroupMembers()
//               支持读取群聊列表、成员摘要、群成员当前在线状态和IP
//     [v0.1.10] HeZhiyuan    2026-06-23
//         *新增：saveGroupMessage()和loadGroupMessages()
//               支持保存群聊消息、更新群聊最近活动时间，并按照顺序读取指定群聊的历史消息。
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

    //插入或更新一个用户
    bool upsertPeer(const QString &peerId,const QString &username,const QString &ip,bool online);
    bool deletePeer(const QString &peerId);    //删除指定用户

    //保存一条聊天消息
    bool saveMessage(const QString &peerId,bool fromMe,const QString &content);

    //读取与指定用户之间的最近聊天记录
    QVariantList loadMessages(const QString &peerId, int limit = 5000);

    //将网络层提供的在线用户列表同步到数据库
    bool synchronizePeers(const QVariantList &onlinePeers);

    //读取数据库中已经保存的persistentPeerId
    //如果数据库中还没有本机ID，就保存candidatePeerId；如果数据库中已经有本机ID，则忽略candidatePeerId
    bool loadOrCreateLocalPeerId(const QString &candidatePeerId, QString &persistentPeerId);

    //创建一个群聊，并一次性保存群聊基本信息和全部成员,
    //该函数使用数据库事务：群信息和全部成员要么一起保存成功，要么任何内容都不写入数据库
    bool createGroup(const QString &groupId, const QString &groupName, const QString &creatorId, const QVariantList &members);

    //读取数据库中保存的全部群聊
    //每个群聊包含：groupId、groupName、creatorId、memberCount、memberSummary、createdAt和updatedAt
    QVariantList loadGroups();

    //读取指定群聊中的全部成员
    //成员按照member_order升序返回，保证成员顺序稳定
    //返回的成员信息同时包含peers表中的当前IP和在线状态
    QVariantList loadGroupMembers(const QString &groupId);

    //保存一条群聊消息,
    //保存消息和更新群聊最近活动时间使用同一个事务，避免出现消息保存成功但群聊排序时间没有更新的情况
    bool saveGroupMessage(const QString &groupId,
                          const QString &senderId,
                          const QString &senderName,
                          bool fromMe,
                          const QString &content);

    //读取指定群聊最近的历史消息
    //limit限制最多读取多少条，防止一次加载过多记录，返回顺序为从旧到新，能够直接交给聊天消息列表显示
    QVariantList loadGroupMessages(const QString &groupId, int limit = 5000);

private:
    bool execSql(const QString &sql);   //执行一条不需要参数绑定的SQL语句
    //将UUID转换成统一的不带大括号格式, 无效UUID返回空字符串
    static QString normalizePeerId(const QString &peerId);

private:
    QSqlDatabase m_db;  //SQLite数据库连接句柄
    QString m_databasePath; //数据库文件的完整路径
    QString m_lastError;    //最近一次数据库操作产生的错误信息
};
