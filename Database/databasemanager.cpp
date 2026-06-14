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
//     [v0.1.2] HeZhiyuan    2026-06-11 17:12:42
//         * 新增读取历史记录
//     [v0.1.2] HeZhiyuan    2026-06-11 20:25:06
//         * 增加saveMessage的失败检查
//     [v0.1.2] HeZhiyuan    2026-06-11 20:56:33
//         * 修改读取历史记录逻辑，修改messages表建立的索引
//     [v0.1.2] HeZhiyuan    2026-06-13 13:18:48
//         * 修改构造函数
//           修复open()未使用m_connectionName的问题
//           新增：析构函数、增加在线用户事务同步功能
//           修改：用户列表调整为在线优先、用户名排序
#include "databasemanager.h"

#include <QDir>
#include <QDebug>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QVariantMap>
#include <QVariant>
#include <QUuid>

DatabaseManager::DatabaseManager(QObject *parent)
    : QObject(parent), m_connectionName(QStringLiteral("messager_connection_") +
                       QUuid::createUuid().toString(QUuid::WithoutBraces))
{}//使每个DatabaseManager都有独立的连接名，不会直接覆盖同名连接

DatabaseManager::~DatabaseManager()
{
    const QString connectionName = m_db.connectionName();

    if (m_db.isValid()) {m_db.close();}

    //必须先清空当前QSqlDatabase句柄，
    //再从Qt的连接池中删除连接。
    m_db = QSqlDatabase();

    if (!connectionName.isEmpty()) {QSqlDatabase::removeDatabase(connectionName);}
}

//打开 SQLite 数据库文件。
bool DatabaseManager::open()
{
    if (m_db.isOpen()) {
        return true;
    }

    m_lastError.clear();

    if (!QSqlDatabase::isDriverAvailable(QStringLiteral("QSQLITE"))) {
        m_lastError = QStringLiteral("当前没有可用的QSQLITE驱动");
        return false;
    }

    const QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);

    if (!QDir().mkpath(dataDir)) {
        m_lastError = "无法创建数据目录：" + dataDir;
        return false;
    }

    m_databasePath = dataDir + "/messager.db";

    m_db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connectionName);
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
    //查询聊天记录时经常使用：WHERE peer_id = ? ORDER BY message_id。
    //建立(peer_id, message_id)组合索引，可以减少数据库扫描量。
    const QString createMessagesByIdIndexSql = R"(
        CREATE INDEX IF NOT EXISTS idx_messages_peer_message_id
        ON messages(peer_id, message_id)
    )";

    if (!execSql(createMessagesByIdIndexSql)) {
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

//执行一条SQL语句
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
        ORDER BY online DESC, username COLLATE NOCASE ASC
    )";//这样在线用户优先显示，同一状态下按用户名排序

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
    if (!m_db.isOpen()) {
        m_lastError = "数据库未打开";
        return false;
    }

    if (peerId.trimmed().isEmpty()) {
        m_lastError = "peerId 为空";
        return false;
    }

    if (content.trimmed().isEmpty()) {
        m_lastError = "消息内容为空";
        return false;
    }

    QSqlQuery query(m_db);

    const QString sql = R"(
        INSERT INTO messages(peer_id, from_me, content)
        VALUES(:peer_id, :from_me, :content)
    )";

    if (!query.prepare(sql)) {
        m_lastError = query.lastError().text();
        qWarning() << "准备saveMessages SQL失败:" << m_lastError;
        qWarning() << "SQL:" << sql;
        return false;
    }

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

//读取当前用户与某一用户的历史聊天记录
//用limit防止一次加载太多历史消息；
QVariantList DatabaseManager::loadMessages(const QString &peerId, int limit)
{
    QVariantList messages;

    if (!m_db.isOpen()) {
        m_lastError = "数据库未打开";
        return messages;
    }

    if (peerId.trimmed().isEmpty()) {
        m_lastError = "peerId 为空";
        return messages;
    }

    //对传入的limit做保护，避免出现负数或极大数
    if (limit <= 0) {
        limit = 100;
    }

    if (limit > 500) {
        limit = 500;
    }

    QSqlQuery query(m_db);

    //这里按message_id降序读取，保证聊天记录是最新的那一批
    //再升序排列，确保由旧到新显示
    const QString sql = R"(
        SELECT message_id, peer_id, from_me, content, created_at
        FROM(
            SELECT message_id, peer_id, from_me, content, created_at
            FROM messages
            WHERE peer_id = :peer_id
            ORDER BY message_id DESC
            LIMIT :limit
        )
        ORDER BY message_id ASC
    )";

    if (!query.prepare(sql)) {
        m_lastError = query.lastError().text();
        qWarning() << "准备loadMessages SQL失败:" << m_lastError;
        qWarning() << "SQL:" << sql;
        return messages;
    }

    query.bindValue(":peer_id", peerId);
    query.bindValue(":limit", limit);

    if (!query.exec()) {
        m_lastError = query.lastError().text();
        qWarning() << "执行loadMessages失败:" << m_lastError;
        return messages;
    }

    //把查询出来的每一行消息记录，转换成QVariantMap
    while (query.next()) {
        QVariantMap message;

        message["messageId"] = query.value(0).toLongLong();
        message["peerId"] = query.value(1).toString();
        message["fromMe"] = query.value(2).toInt() != 0;
        message["content"] = query.value(3).toString();
        message["createdAt"] = query.value(4).toString();

        messages.append(message);
    }

    return messages;

}

//增加在线用户同步接口同时保留历史用户和离线状态
bool DatabaseManager::synchronizePeers(const QVariantList &onlinePeers)
{
    if (!m_db.isOpen()) {
        m_lastError = QStringLiteral("数据库未打开");
        return false;
    }

    //保证“全部标记离线”和“重新标记在线”
    //要么全部成功，要么全部回滚。
    if (!m_db.transaction()) {
        m_lastError = m_db.lastError().text();
        return false;
    }

    QSqlQuery markOfflineQuery(m_db);

    if (!markOfflineQuery.exec(R"(
        UPDATE peers
        SET online = 0,updated_at = CURRENT_TIMESTAMP
        WHERE online <> 0
    )")) {
        m_lastError = markOfflineQuery.lastError().text();
        m_db.rollback();
        return false;
    }

    QSqlQuery upsertQuery(m_db);

    const QString sql = R"(
        INSERT INTO peers(peer_id, username, ip, online, updated_at)
        VALUES(:peer_id, :username, :ip, 1, CURRENT_TIMESTAMP)
        ON CONFLICT(peer_id) DO UPDATE SET
            username = excluded.username,
            ip = excluded.ip,
            online = 1,
            updated_at = CURRENT_TIMESTAMP
    )";

    if (!upsertQuery.prepare(sql)) {
        m_lastError = upsertQuery.lastError().text();
        m_db.rollback();
        return false;
    }

    for (const QVariant &item : onlinePeers) {
        const QVariantMap peer = item.toMap();

        const QString peerId =
            peer.value(QStringLiteral("peerId")).toString().trimmed();

        const QString username =
            peer.value(QStringLiteral("username")).toString().trimmed();

        const QString ip =
            peer.value(QStringLiteral("ip")).toString().trimmed();

        if (peerId.isEmpty() || username.isEmpty() || ip.isEmpty()) {
            m_lastError = QStringLiteral("在线用户数据不完整");
            m_db.rollback();
            return false;
        }

        upsertQuery.bindValue(QStringLiteral(":peer_id"), peerId);
        upsertQuery.bindValue(QStringLiteral(":username"), username);
        upsertQuery.bindValue(QStringLiteral(":ip"), ip);

        if (!upsertQuery.exec()) {
            m_lastError = upsertQuery.lastError().text();
            m_db.rollback();
            return false;
        }

        upsertQuery.finish();
    }

    if (!m_db.commit()) {
        m_lastError = m_db.lastError().text();
        m_db.rollback();
        return false;
    }

    return true;
}