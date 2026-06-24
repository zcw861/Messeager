#include "privatechatdatabase.h"

#include "databasecore.h"
#include "databasecheck.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QVariantMap>

PrivateChatDatabase::PrivateChatDatabase(DatabaseCore &databaseCore)
    : m_databaseCore(databaseCore)
{
}

bool PrivateChatDatabase::initSchema()
{
    m_lastError.clear();

    QSqlDatabase database = m_databaseCore.database();

    if (!database.isValid() || !database.isOpen()) {
        m_lastError = QStringLiteral("数据库未打开");

        return false;
    }

    if (!database.transaction()) {
        m_lastError = database.lastError().text();

        return false;
    }

    const QString createMessagesSql = R"(
        CREATE TABLE IF NOT EXISTS messages(
            message_id INTEGER PRIMARY KEY AUTOINCREMENT,
            peer_id TEXT NOT NULL,
            from_me INTEGER NOT NULL CHECK(from_me IN (0, 1)),
            content TEXT NOT NULL CHECK(length(trim(content)) > 0),
            created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,

            FOREIGN KEY(peer_id) REFERENCES peers(peer_id) ON DELETE CASCADE
        )
    )";

    if (!m_databaseCore.execute(createMessagesSql, m_lastError)) {
        database.rollback();
        return false;
    }

    const QString createMessagesIndexSql = R"(
        CREATE INDEX IF NOT EXISTS
            idx_messages_peer_message_id
        ON messages(peer_id, message_id)
    )";

    if (!m_databaseCore.execute(createMessagesIndexSql, m_lastError)) {
        database.rollback();
        return false;
    }

    if (!database.commit()) {
        m_lastError = database.lastError().text();
        database.rollback();
        return false;
    }

    return true;
}

bool PrivateChatDatabase::saveMessage(const QString &peerId, bool fromMe, const QString &content)
{
    m_lastError.clear();

    QSqlDatabase database = m_databaseCore.database();

    if (!database.isValid() || !database.isOpen()) {
        m_lastError = QStringLiteral("数据库未打开");
        return false;
    }

    const QString normalizedPeerId = DatabaseCheck::normalizePeerId(peerId);

    const QString normalizedContent = content.trimmed();

    if (normalizedPeerId.isEmpty()) {
        m_lastError = QStringLiteral("peerId不是有效UUID");
        return false;
    }

    if (normalizedContent.isEmpty()) {
        m_lastError = QStringLiteral("消息内容为空");
        return false;
    }

    QSqlQuery query(database);

    const QString sql = R"(
        INSERT INTO messages(
            peer_id,
            from_me,
            content
        )
        VALUES(
            :peer_id,
            :from_me,
            :content
        )
    )";

    if (!query.prepare(sql)) {
        m_lastError = query.lastError().text();
        return false;
    }

    query.bindValue(QStringLiteral(":peer_id"), normalizedPeerId);

    query.bindValue(QStringLiteral(":from_me"), fromMe ? 1 : 0);

    query.bindValue(QStringLiteral(":content"), normalizedContent);

    if (!query.exec()) {
        m_lastError = query.lastError().text();
        return false;
    }

    return true;
}

bool PrivateChatDatabase::loadMessages(const QString &peerId, QVariantList &messages, int limit)
{
    m_lastError.clear();
    messages.clear();

    QSqlDatabase database =
        m_databaseCore.database();

    if (!database.isValid() || !database.isOpen()) {
        m_lastError = QStringLiteral("数据库未打开");

        return false;
    }

    const QString normalizedPeerId =
        DatabaseCheck::normalizePeerId(
            peerId);

    if (normalizedPeerId.isEmpty()) {
        m_lastError = QStringLiteral("peerId不是有效UUID");

        return false;
    }

    const int normalizedLimit = DatabaseCheck::normalizeMessageLimit(limit);

    QSqlQuery query(database);

    //内层倒序取得最新的normalizedLimit条消息，外层重新升序排列，使QML按从旧到新显示
    const QString sql = R"(
        SELECT
            message_id,
            peer_id,
            from_me,
            content,
            created_at
        FROM (
            SELECT
                message_id,
                peer_id,
                from_me,
                content,
                created_at
            FROM messages
            WHERE peer_id = :peer_id
            ORDER BY message_id DESC
            LIMIT :limit
        )
        ORDER BY message_id ASC
    )";

    if (!query.prepare(sql)) {
        m_lastError = query.lastError().text();

        return false;
    }

    query.bindValue(QStringLiteral(":peer_id"), normalizedPeerId);

    query.bindValue(QStringLiteral(":limit"), normalizedLimit);

    if (!query.exec()) {
        m_lastError = query.lastError().text();

        return false;
    }

    while (query.next()) {
        QVariantMap message;

        message.insert(QStringLiteral("messageId"), query.value(0).toLongLong());

        message.insert(QStringLiteral("peerId"), query.value(1).toString());

        message.insert(QStringLiteral("fromMe"), query.value(2).toInt() != 0);

        message.insert(QStringLiteral("content"), query.value(3).toString());

        message.insert(QStringLiteral("createdAt"), query.value(4).toString());

        messages.append(message);
    }

    return true;
}

QString PrivateChatDatabase::lastError() const
{
    return m_lastError;
}
