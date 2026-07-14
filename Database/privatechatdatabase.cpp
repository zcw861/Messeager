// Module
// File: privatechatdatabase.cpp   Version: 0.1.0   License: AGPLv3
// Created: HeZhiyuan      2026-06-24
// Description:
//
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

//创建messages私聊消息表和按peer_id、message_id排列的组合索引
bool PrivateChatDatabase::initSchema()
{
    m_lastError.clear();

    //取得DatabaseCore已经打开的默认连接
    QSqlDatabase database = m_databaseCore.database();

    //验证数据库句柄和打开状态
    if (!database.isValid() || !database.isOpen()) {
        m_lastError = QStringLiteral("数据库未打开");

        return false;
    }

    //开启结构初始化事务
    if (!database.transaction()) {
        m_lastError = database.lastError().text();

        return false;
    }

    //创建messages 表：
    //message_id使用INTEGER PRIMARY KEY AUTOINCREMENT，让SQLite自动生成递增编号
    //peer_id表示这条消息属于哪个用户
    //from_me表示消息方向：1：我发送的消息；0：对方发送的消息，用CHECK确定只能是0/1
    //FOREIGN KEY表示messages.peer_id必须对应peers.peer_id。
    //ON DELETE CASCADE表示如果某个peer被删除，该 peer 对应的消息也自动删除
    const QString createMessagesSql = R"(
    CREATE TABLE IF NOT EXISTS messages(
        message_id INTEGER PRIMARY KEY AUTOINCREMENT,
        peer_id TEXT NOT NULL,
        from_me INTEGER NOT NULL CHECK(from_me IN (0, 1)),
        content TEXT NOT NULL CHECK(length(trim(content)) > 0),

        transfer_status TEXT NOT NULL DEFAULT 'none'
            CHECK(transfer_status IN (
                'none',
                'transferring',
                'completed',
                'failed'
            )),

        created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,

        FOREIGN KEY(peer_id)
            REFERENCES peers(peer_id)
            ON DELETE CASCADE
        )
    )";

    if (!m_databaseCore.execute(createMessagesSql, m_lastError)) {
        database.rollback();
        return false;
    }

    //检查旧数据库中是否已经存在transfer_status字段。
    QSqlQuery tableInfoQuery(database);

    if (!tableInfoQuery.exec(
            QStringLiteral("PRAGMA table_info(messages)"))) {

        m_lastError = tableInfoQuery.lastError().text();
        database.rollback();
        return false;
    }

    bool hasTransferStatusColumn = false;

    while (tableInfoQuery.next()) {
        const QString columnName =
            tableInfoQuery.value(1).toString();

        if (columnName == QStringLiteral("transfer_status")) {
            hasTransferStatusColumn = true;
            break;
        }
    }

    tableInfoQuery.finish();

    //旧数据库没有该字段时，执行一次迁移。
    if (!hasTransferStatusColumn) {
        QSqlQuery alterQuery(database);

        const QString alterSql = QStringLiteral(
            "ALTER TABLE messages "
            "ADD COLUMN transfer_status "
            "TEXT NOT NULL DEFAULT 'none'"
            );

        if (!alterQuery.exec(alterSql)) {
            m_lastError = alterQuery.lastError().text();
            database.rollback();
            return false;
        }
    }

    //程序上一次运行时仍处于transferring的消息，
    //说明程序在传输结束前退出，应当恢复为失败状态。
    QSqlQuery recoverQuery(database);

    if (!recoverQuery.exec(QStringLiteral(
            "UPDATE messages "
            "SET transfer_status = 'failed' "
            "WHERE transfer_status = 'transferring'"
            ))) {

        m_lastError = recoverQuery.lastError().text();
        database.rollback();
        return false;
    }

    //给messages表建立索引。
    //查询聊天记录时经常使用：WHERE peer_id = ? ORDER BY message_id
    //建立(peer_id, message_id)组合索引，可以减少数据库扫描量
    const QString createMessagesIndexSql = R"(
        CREATE INDEX IF NOT EXISTS
            idx_messages_peer_message_id
        ON messages(peer_id, message_id)
    )";

    if (!m_databaseCore.execute(createMessagesIndexSql, m_lastError)) {
        database.rollback();
        return false;
    }

    //提交建表和建索引事务
    if (!database.commit()) {
        m_lastError = database.lastError().text();
        database.rollback();
        return false;
    }

    return true;
}

//校验peerId和消息正文后保存私聊消息
bool PrivateChatDatabase::saveMessage(const QString &peerId, bool fromMe, const QString &content, qint64 *insertedMessageId, const QString &transferStatus)
{
    m_lastError.clear();

    //调用失败时，输出参数保持为无效ID
    if (insertedMessageId != nullptr)
        *insertedMessageId = -1;

    QSqlDatabase database = m_databaseCore.database();

    if (!database.isValid() || !database.isOpen()) {
        m_lastError = QStringLiteral("数据库未打开");
        return false;
    }

    //统一校验并格式化peerId
    const QString normalizedPeerId = DatabaseCheck::normalizePeerId(peerId);

    const QString normalizedContent = content.trimmed();

    const QString normalizedTransferStatus =
        transferStatus.trimmed().toLower();

    //只允许数据库定义的四种状态。
    if (normalizedTransferStatus != QStringLiteral("none")
        && normalizedTransferStatus != QStringLiteral("transferring")
        && normalizedTransferStatus != QStringLiteral("completed")
        && normalizedTransferStatus != QStringLiteral("failed")) {

        m_lastError = QStringLiteral("文件传输状态不合法");
        return false;
    }

    //检查peerId是否合法
    if (normalizedPeerId.isEmpty()) {
        m_lastError = QStringLiteral("peerId不是有效UUID");
        return false;
    }

    //检查规范化后的消息正文是否为空
    if (normalizedContent.isEmpty()) {
        m_lastError = QStringLiteral("消息内容为空");
        return false;
    }

    //创建绑定当前连接的查询对象
    QSqlQuery query(database);

    //定义带命名占位符的插入SQL
    const QString sql = R"(
        INSERT INTO messages(
            peer_id,
            from_me,
            content,
            transfer_status
        )
        VALUES(
            :peer_id,
            :from_me,
            :content,
            :transfer_status
        )
    )";

    //预处理插入SQL
    if (!query.prepare(sql)) {
        m_lastError = query.lastError().text();
        return false;
    }

    //绑定规范化后的用户ID，避免字符串拼接并保持UUID格式统一
    query.bindValue(QStringLiteral(":peer_id"), normalizedPeerId);
    //把bool消息方向转换为SQLite整数0或
    query.bindValue(QStringLiteral(":from_me"), fromMe ? 1 : 0);
    //绑定已经去除首尾空白的消息正文
    query.bindValue(QStringLiteral(":content"), normalizedContent);

    query.bindValue(QStringLiteral(":transfer_status"), normalizedTransferStatus);

    if (!query.exec()) {
        m_lastError = query.lastError().text();
        return false;
    }

    //如果调用者需要新消息ID，就读取SQLite刚刚自动生成的message_id。
    //lastInsertId()必须在INSERT语句成功执行后调用，不能放在SELECT查询里。
    if (insertedMessageId != nullptr) {
        const QVariant insertedId = query.lastInsertId();

        if (!insertedId.isValid()) {
            m_lastError = QStringLiteral("无法取得新消息ID");
            return false;
        }

        *insertedMessageId = insertedId.toLongLong();
    }

    return true;
}

//根据message_id更新文件传输状态。
bool PrivateChatDatabase::updateTransferStatus(
    qint64 messageId,
    const QString &transferStatus)
{
    m_lastError.clear();

    if (messageId <= 0) {
        m_lastError = QStringLiteral("消息ID无效");
        return false;
    }

    const QString normalizedStatus =
        transferStatus.trimmed().toLower();

    if (normalizedStatus != QStringLiteral("transferring")
        && normalizedStatus != QStringLiteral("completed")
        && normalizedStatus != QStringLiteral("failed")) {

        m_lastError = QStringLiteral("文件传输状态不合法");
        return false;
    }

    QSqlDatabase database = m_databaseCore.database();

    if (!database.isValid() || !database.isOpen()) {
        m_lastError = QStringLiteral("数据库未打开");
        return false;
    }

    QSqlQuery query(database);

    const QString sql = R"(
        UPDATE messages
        SET transfer_status = :transfer_status
        WHERE message_id = :message_id
    )";

    if (!query.prepare(sql)) {
        m_lastError = query.lastError().text();
        return false;
    }

    query.bindValue(
        QStringLiteral(":transfer_status"),
        normalizedStatus
        );

    query.bindValue(
        QStringLiteral(":message_id"),
        messageId
        );

    if (!query.exec()) {
        m_lastError = query.lastError().text();
        return false;
    }

    if (query.numRowsAffected() != 1) {
        m_lastError = QStringLiteral("没有找到需要更新的文件消息");
        return false;
    }

    return true;
}

//读取指定用户最近limit条私聊消息
bool PrivateChatDatabase::loadMessages(const QString &peerId, QVariantList &messages, int limit)
{
    m_lastError.clear();
    //清空输出列表
    messages.clear();

    //取得连接
    QSqlDatabase database = m_databaseCore.database();

    //验证连接有效且已打开
    if (!database.isValid() || !database.isOpen()) {
        m_lastError = QStringLiteral("数据库未打开");

        return false;
    }

    //规范化并验证目标用户UUID，使查询条件与messages.peer_id保存格式一致
    const QString normalizedPeerId = DatabaseCheck::normalizePeerId(peerId);

    if (normalizedPeerId.isEmpty()) {
        m_lastError = QStringLiteral("peerId不是有效UUID");

        return false;
    }

    //把读取数量规范化到安全范围
    const int normalizedLimit = DatabaseCheck::normalizeMessageLimit(limit);

    //创建查询对象
    QSqlQuery query(database);

    //内层倒序取得最新的normalizedLimit条消息，外层重新升序排列，使QML按从旧到新显示
    const QString sql = R"(
        SELECT
            message_id,
            peer_id,
            from_me,
            content,
            transfer_status,
            created_at
        FROM (
            SELECT
                message_id,
                peer_id,
                from_me,
                content,
                transfer_status,
                created_at
            FROM messages
            WHERE peer_id = :peer_id
            ORDER BY message_id DESC
            LIMIT :limit
        )
        ORDER BY message_id ASC
    )";

    //预处理查询SQ
    if (!query.prepare(sql)) {
        m_lastError = query.lastError().text();

        return false;
    }

    //绑定目标用户UUID，避免直接拼接用户输入
    query.bindValue(QStringLiteral(":peer_id"), normalizedPeerId);
    //绑定已经限制后的消息数量
    query.bindValue(QStringLiteral(":limit"), normalizedLimit);

    if (!query.exec()) {
        m_lastError = query.lastError().text();

        return false;
    }

    //逐行遍历查询结果，把每条记录转换为对应键值结构
    while (query.next()) {
        QVariantMap message;

        //保存数据库消息ID
        message.insert(QStringLiteral("messageId"), query.value(0).toLongLong());
        //保存消息所属用户ID
        message.insert(QStringLiteral("peerId"), query.value(1).toString());
        //把整数方向转换为bool，判断消息气泡方向
        message.insert(QStringLiteral("fromMe"), query.value(2).toInt() != 0);

        message.insert(QStringLiteral("content"), query.value(3).toString());

        message.insert(QStringLiteral("transferStatus"), query.value(4).toString());

        message.insert(QStringLiteral("createdAt"), query.value(5).toString());

        //把当前消息追加到输出列表
        messages.append(message);
    }

    return true;
}

QString PrivateChatDatabase::lastError() const
{
    return m_lastError;
}
