// Module
// File: privatechatdatabase.h   Version: 0.1.0   License: AGPLv3
// Created: HeZhiyuan      2026-06-24 14:58:02
// Description: 定义私聊消息表初始化、消息保存和历史记录读取接口
//
#pragma once

#include <QString>
#include <QVariantList>

#include "database_global.h"

class DatabaseCore;

class DATABASE_EXPORT PrivateChatDatabase final
{
public:
    //保存DatabaseCore引用并使用SQLite连接
    explicit PrivateChatDatabase(DatabaseCore &databaseCore);

    //创建私聊消息表和索引
    bool initSchema();

    //保存一条私聊消息
    bool saveMessage(const QString &peerId, bool fromMe, const QString &content);
    //读取指定用户的最近私聊记录
    bool loadMessages(const QString &peerId, QVariantList &messages, int limit = 5000);

    QString lastError() const;

private:
    DatabaseCore &m_databaseCore;
    QString m_lastError;
};