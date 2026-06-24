#pragma once

#include <QString>
#include <QVariantList>

#include "database_global.h"

class DatabaseCore;

class DATABASE_EXPORT GroupChatDatabase final
{
public:
    explicit GroupChatDatabase(
        DatabaseCore &databaseCore);

    bool initSchema();

    /*
     * 返回值表示查询是否成功。
     * exists表示群ID是否已存在。
     */
    bool groupExists(
        const QString &groupId,
        bool &exists);

    bool createGroup(
        const QString &groupId,
        const QString &groupName,
        const QString &creatorId,
        const QVariantList &members);

    bool deleteGroup(
        const QString &groupId);

    bool loadGroups(
        QVariantList &groups);

    bool loadGroupMembers(
        const QString &groupId,
        QVariantList &members);

    /*
     * 不再接收fromMe。
     *
     * 数据库根据senderId和local_peer_id计算消息方向。
     */
    bool saveGroupMessage(
        const QString &groupId,
        const QString &senderId,
        const QString &senderName,
        const QString &content);

    bool loadGroupMessages(
        const QString &groupId,
        QVariantList &messages,
        int limit = 5000);

    QString lastError() const;

private:
    DatabaseCore &m_databaseCore;
    QString m_lastError;
};
