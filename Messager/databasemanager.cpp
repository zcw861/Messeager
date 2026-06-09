//     [v0.1.2] HeZhiyuan    2026-06-07 21:43:58
//         *实现基础的数据库操作：打开、初始化、打印路径和报错、执行sql语句
//     [v0.1.2] HeZhiyuan    2026-06-08 15:44:38
//         *新增：读取用户列表、新增用户
//     [v0.1.2] HeZhiyuan    2026-06-08 21:14:38
//         * 在open()里增加：在每次打开连接后主动开启外键检查
//          *修改initSchema()：初始化数据库表1.peers：保存局域网用户信息，
//          *2.messages：保存与某个用户相关的聊天消息，3.给消息表加索引，加快按用户读取聊天记录的速度
//     [v0.1.2] HeZhiyuan    2026-06-09 22:56:40
//         * 新增消息保存
#include "databasemanager.h"

#include <QDir>
#include <QDebug>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QVariantMap>
#include <QVariant>

DatabaseManager::DatabaseManager(QObject *parent)
    : QObject(parent)
{}

//打开 SQLite 数据库文件。
bool DatabaseManager::open()
{
    const QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);

    if (!QDir().mkpath(dataDir)) {
        m_lastError = "无法创建数据目录：" + dataDir;
        return false;
    }

    m_databasePath = dataDir + "/messager.db";

    m_db = QSqlDatabase::addDatabase("QSQLITE", "messager_connection");
    m_db.setDatabaseName(m_databasePath);

    if (!m_db.open()) {
        m_lastError = m_db.lastError().text();
        return false;
    }

    //SQLite的外键约束默认不是稳定依赖项，应该在每次打开连接后主动开启外键检查
    if (!execSql("PRAGMA foreign_keys = ON")) {
        return false;
    }

    return true;
}

//初始化数据库表。
//目前创建两张表：
//1.peers：保存局域网用户信息：id,用户名,ip,是否在线,更新时间
//2.messages：保存与某个用户相关的聊天消息。
bool DatabaseManager::initSchema()
{
    const QString createPeersSql = R"(
        CREATE TABLE IF NOT EXISTS peers(
            peer_id TEXT PRIMARY KEY,
            username TEXT NOT NULL,
            ip TEXT NOT NULL,
            online INTEGER NOT NULL DEFAULT 0,
            updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
        )
    )";

    if (!execSql(createPeersSql)) {
        return false;
    }


    //创建messages 表：
    //message_id使用INTEGER PRIMARY KEY AUTOINCREMENT，让SQLite自动生成递增编号。
    //peer_id表示这条消息属于哪个用户。
    //from_me表示消息方向：1：我发送的消息；0：对方发送的消息，用CHECK确定只能是0/1
    //FOREIGN KEY表示messages.peer_id必须对应peers.peer_id。
    //ON DELETE CASCADE表示如果某个peer被删除，该 peer 对应的消息也自动删除。

    const QString createMessagesSql = R"(
        CREATE TABLE IF NOT EXISTS messages(
            message_id INTEGER PRIMARY KEY AUTOINCREMENT,
            peer_id TEXT NOT NULL,
            from_me INTEGER NOT NULL CHECK(from_me IN (0, 1)),
            content TEXT NOT NULL,
            created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
            FOREIGN KEY(peer_id) REFERENCES peers(peer_id) ON DELETE CASCADE
        )
    )";

    if (!execSql(createMessagesSql)) {
        return false;
    }

    //给messages表建立索引。
    const QString createMessagesIndexSql = R"(
        CREATE INDEX IF NOT EXISTS idx_messages_peer_time
        ON messages(peer_id, created_at)
    )";

    if (!execSql(createMessagesIndexSql)) {
        return false;
    }

    return true;
}

//当前数据库文件的完整路径，方便运行后检查数据库文件位置。
QString DatabaseManager::databasePath() const
{
    return m_databasePath;
}

//最近一次错误信息
QString DatabaseManager::lastError() const
{
    return m_lastError;
}

//执行一条普通 SQL 语句
bool DatabaseManager::execSql(const QString &sql)
{
    QSqlQuery query(m_db);

    if (!query.exec(sql)) {
        m_lastError = query.lastError().text();
        qWarning() << "SQL执行失败：" << m_lastError;
        qWarning() << "SQL: " << sql;
        return false;
    }
    return true;
}

//从peers表中读取所有用户
QVariantList DatabaseManager::loadPeers()
{
    QVariantList peers;
    if (!m_db.isOpen()) {
        m_lastError = "数据库尚未打开";
        return peers;
    }

    QSqlQuery query(m_db);

    const QString sql = R"(
        SELECT peer_id, username, ip, online, updated_at
        FROM peers
        ORDER BY peer_id ASC
    )";

    if (!query.exec(sql)) {
        m_lastError = query.lastError().text();
        qWarning() << "查询 peers 失败:" << m_lastError;
        return peers;
    }

    while (query.next()) {
        QVariantMap peer;
        //query.value(0)对应peer_id。
        peer["peerId"] = query.value(0).toString();

        //1对应username。
        peer["username"] = query.value(1).toString();

        //2对应ip。
        peer["ip"] = query.value(2).toString();

        //3对应online。数据库里是 1 / 0，这里转成 bool。
        peer["online"] = query.value(3).toInt() != 0;

        //4对应updated_at。
        peer["updatedAt"] = query.value(4).toString();

        peers.append(peer);
    }

    return peers;

}

//插入或更新一个用户
bool DatabaseManager::upsertPeer(const QString &peerId,
                                const QString &username,
                                const QString &ip,
                                bool online)
{
    if (!m_db.isOpen()) {
        m_lastError = "数据库未打开";
        return false;
    }

    QSqlQuery query(m_db);
    //sql语句：如果 peer_id 不存在，就插入新用户。
    //如果 peer_id 存在，就更新username、ip、online、updated_at
    const QString sql = R"(
        INSERT INTO peers (peer_id, username, ip, online, updated_at)
        VALUES (:peer_id, :username, :ip, :online, CURRENT_TIMESTAMP)
        ON CONFLICT(peer_id) DO UPDATE SET
            username = excluded.username,
            ip = excluded.ip,
            online = excluded.online,
            updated_at = CURRENT_TIMESTAMP
    )";

    //没有prepare()，后面的bindValue()没有绑定目标
    if (!query.prepare(sql)) {
        m_lastError = query.lastError().text();
        qWarning() << "准备upsertPeer SQL失败:" << m_lastError;
        qWarning() << "SQL:" << sql;
        return false;
    }

    //把 C++ 变量绑定到 SQL 占位符
    query.bindValue(":peer_id", peerId);
    query.bindValue(":username", username);
    query.bindValue(":ip", ip);
    query.bindValue(":online", online ? 1 : 0);

    //执行sql语句
    if (!query.exec()) {
        m_lastError = query.lastError().text();
        qWarning() << "执行upsertPeer失败" << m_lastError;
        return false;
    }
    return true;
}

//保存一条聊天记录到message表：peerId:聊天对象唯一id，fromMe:true是我发的;false是对方发的，content:正文
bool DatabaseManager::saveMessage(const QString &peerId, bool fromMe, const QString &content)
{
    if (content.trimmed().isEmpty()) {
        m_lastError = "消息内容为空";
        return false;
    }

    QSqlQuery query(m_db);

    query.prepare(R"(
        INSERT INTO messages(peer_id, from_me, content)
        VALUES(:peer_id, :from_me, :content)
    )");

    query.bindValue(":peer_id", peerId);
    query.bindValue(":from_me", fromMe ? 1 : 0);    //1：我发送;0：对方发送
    query.bindValue(":content", content.trimmed());

    //执行sql语句
    if (!query.exec()) {
        m_lastError = query.lastError().text();
        qWarning() << "执行saveMessage失败" << m_lastError;
        return false;
    }

    return true;
}