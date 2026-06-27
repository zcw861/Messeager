// Module
// File: groupchatdatabase.h   Version: 0.1.0   License: AGPLv3
// Created: HeZhiyuan      2026-06-24 14:57:38
// Description: 定义群聊、群成员和群消息相关的数据库接口
//
//     [v0.1.1] HeZhiyuan    2026-06-27 15:43:11
//         * 新增updateMemberUsername()
#pragma once

#include <QString>
#include <QVariantList>

#include "database_global.h"

class DatabaseCore;

class DATABASE_EXPORT GroupChatDatabase final
{
public:
    //保存DatabaseCore引用并复用已经打开的SQLite连接，避免为群聊数据重复创建连接
    explicit GroupChatDatabase(DatabaseCore &databaseCore);

    //创建群聊基本信息表、群成员关系表、群消息表及其索引
    bool initSchema();

    //查询群ID是否已经存在，函数返回值表示查询是否成功，exists表示实际存在状态
    bool groupExists(const QString &groupId, bool &exists);

    //校验群信息和成员列表，并保存群聊基本信息及全部成员
    bool createGroup(const QString &groupId, const QString &groupName, const QString &creatorId, const QVariantList &members);

    //删除指定群聊；群成员和群消息由外键级联删除
    bool deleteGroup(const QString &groupId);

    //读取全部群聊的名称、创建者、时间、成员数量和成员摘要
    bool loadGroups(QVariantList &groups);

    //按member_order读取指定群的全部成员，并附加当前IP、在线状态和是否为本机
    bool loadGroupMembers(const QString &groupId, QVariantList &members);

    //更新该用户在所有群聊中的成员名称，空用户名不会覆盖原有名称
    bool updateMemberUsername(const QString &peerId, const QString &username);

    //校验群成员身份后保存群消息，并根据senderId和local_peer_id自动判断是否fromMe
    bool saveGroupMessage(const QString &groupId, const QString &senderId, const QString &senderName, const QString &content);

    //读取群最近limit条历史消息并按从旧到新返回
    bool loadGroupMessages(const QString &groupId, QVariantList &messages, int limit = 5000);

    //返回最近一次产生的错误
    QString lastError() const;

private:
    //引用数据库连接管理对象
    DatabaseCore &m_databaseCore;

    QString m_lastError;
};
