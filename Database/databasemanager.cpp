//     [v0.1.2] HeZhiyuan    2026-06-07 21:43:58
//         *实现基础的数据库操作：打开、初始化、打印路径和报错、执行sql语句
//     [v0.1.3] HeZhiyuan    2026-06-08 15:44:38
//         *新增：读取用户列表、新增用户
//     [v0.1.4] HeZhiyuan    2026-06-08 21:14:38
//         * 在open()里增加：在每次打开连接后主动开启外键检查
//          *修改initSchema()：初始化数据库表1.peers：保存局域网用户信息，
//          *2.messages：保存与某个用户相关的聊天消息，3.给消息表加索引，加快按用户读取聊天记录的速度
//     [v0.1.5] HeZhiyuan    2026-06-09 22:56:40
//         * 新增消息保存
//     [v0.1.6] HeZhiyuan    2026-06-11 17:12:42
//         * 新增读取历史记录
//     [v0.1.7] HeZhiyuan    2026-06-11 20:25:06
//         * 增加saveMessage的失败检查
//     [v0.1.8] HeZhiyuan    2026-06-11 20:56:33
//         * 修改读取历史记录逻辑，修改messages表建立的索引
//     [v0.1.9] HeZhiyuan    2026-06-13 13:18:48
//         * 修改构造函数
//           修复open()未使用m_connectionName的问题
//           新增：析构函数、增加在线用户事务同步功能
//           修改：用户列表调整为在线优先、用户名排序
//     [v0.1.10] HeZhiyuan    2026-06-14 15:57:04
//         * 新增：删除用户
#include "databasemanager.h"

#include <QDir>
#include <QDebug>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QVariantMap>
#include <QVariant>
#include <QUuid>

//为当前 DatabaseManager 生成唯一连接名。QSqlDatabase 的连接由全局连接池按名称管理，使用UUID防止多个对象意外共用或覆盖同一连接
DatabaseManager::DatabaseManager(QObject *parent)
    : QObject(parent), m_connectionName(QStringLiteral("messager_connection_") +
                       QUuid::createUuid().toString(QUuid::WithoutBraces))
{}

//关闭数据库连接并将其从Qt SQL全局连接池中移除
DatabaseManager::~DatabaseManager()
{
    const QString connectionName = m_db.connectionName();

    if (m_db.isValid()) {m_db.close();}

    //必须先清空当前QSqlDatabase句柄
    //再从Qt的连接池中删除连接
    m_db = QSqlDatabase();

    // 从Qt SQL的全局连接池中移除当前连接
    if (!connectionName.isEmpty()) {QSqlDatabase::removeDatabase(connectionName);}
}

//打开 SQLite 数据库文件
bool DatabaseManager::open()
{
    if (m_db.isOpen()) {
        return true;
    }

    //开始新操作前清除上一次错误，防止上层读取到过期信息
    m_lastError.clear();

    if (!QSqlDatabase::isDriverAvailable(QStringLiteral("QSQLITE"))) {
        m_lastError = QStringLiteral("当前没有可用的QSQLITE驱动");
        return false;
    }

    //AppDataLocation会根据组织名和应用名生成应用私有数据目录
    const QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);

    if (!QDir().mkpath(dataDir)) {
        m_lastError = "无法创建数据目录：" + dataDir;
        return false;
    }

    m_databasePath = dataDir + "/messager.db";

    //使用构造函数生成的唯一名称，在Qt SQL连接池中创建连接
    m_db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connectionName);
    m_db.setDatabaseName(m_databasePath);

    //打开数据库文件；失败时保存驱动返回的错误信息
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

//执行不带参数绑定的SQL
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


    //从peers表读取全部用户，并转换为QML能直接读取的QVariantList。字段名称与QML中使用的modelData属性保持一致
    while (query.next()) {
        QVariantMap peer;
        //query.value(0)对应peer_id。
        peer["peerId"] = query.value(0).toString();
        //1对应username
        peer["username"] = query.value(1).toString();
        //2对应ip
        peer["ip"] = query.value(2).toString();
        //3对应online。数据库里是1/0，转成bool
        peer["online"] = query.value(3).toInt() != 0;
        //4对应updated_at
        peer["updatedAt"] = query.value(4).toString();
        peers.append(peer);
    }

    return peers;
}

//插入或更新一个用户，peer_id 不存在时插入；已存在时更新用户名、IP、在线状态和时间
bool DatabaseManager::upsertPeer(const QString &peerId,
                                const QString &username,
                                const QString &ip,
                                bool online)
{
    if (!m_db.isOpen()) {
        m_lastError = "数据库未打开";
        return false;
    }

    //使用参数绑定而不是拼接SQL，避免特殊字符破坏语句
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
    //prepare()解析 SQL 并建立占位符
    //只有准备成功后，下面的bindValue()才有绑定目标。
    if (!query.prepare(sql)) {
        m_lastError = query.lastError().text();
        qWarning() << "准备upsertPeer SQL失败:" << m_lastError;
        qWarning() << "SQL:" << sql;
        return false;
    }

    //把C++变量绑定到SQL占位符
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

//删除一个用户，因为messages.peer_id使用了ON DELETE CASCADE，在删除 peers 记录时，对应聊天记录由SQLite删除。
bool DatabaseManager::deletePeer(const QString &peerId)
{
    m_lastError.clear();

    if (!m_db.isOpen()) {
        m_lastError = QStringLiteral("数据库未打开");
        return false;
    }

    //去除用户ID首尾空白，统一后续校验和查询使用的值
    const QString normalizedPeerId = peerId.trimmed();

    //空ID不能对应有效用户，直接阻止无意义的 DELETE
    if (normalizedPeerId.isEmpty()) {
        m_lastError = QStringLiteral("peerId 为空");
        return false;
    }

    QSqlQuery query(m_db);

    const QString sql = R"(
        DELETE FROM peers
        WHERE peer_id = :peer_id
    )";

    //SQL准备失败时不继续绑定和执行
    if (!query.prepare(sql)) {
        m_lastError = query.lastError().text();
        qWarning() << "准备 deletePeer SQL 失败:" << m_lastError;
        return false;
    }

    //将标准化后的用户ID绑定到删除条件
    query.bindValue(QStringLiteral(":peer_id"),normalizedPeerId);

    if (!query.exec()) {
        m_lastError = query.lastError().text();
        qWarning() << "执行 deletePeer 失败:" << m_lastError;
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

    //messages.peer_id是非空外键，不能写入空用户ID
    if (peerId.trimmed().isEmpty()) {
        m_lastError = "peerId 为空";
        return false;
    }

    //空白消息不进入数据库，避免产生无实际内容的历史记录
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
    //保存时去除消息首尾空白
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
        limit = 5000;
    }

    if (limit > 9999) {
        limit = 9999;
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

    //把查询出来的每一行消息记录，转换成QVariantMap，字段名与ChatPanel.qml中的modelData属性保持一致
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
//历史用户不会被删除：先统一标记离线，再将当前列表中的用户更新为在线
bool DatabaseManager::synchronizePeers(const QVariantList &onlinePeers)
{
    if (!m_db.isOpen()) {
        m_lastError = QStringLiteral("数据库未打开");
        return false;
    }

    //“全部标记离线”和“重新标记在线”必须作为一个原子操作
    //全部成功后提交；任何一步失败则回滚，避免数据库处于半更新状态
    //开启一个数据库事务，确保后面的一组数据库操作被视为一个整体执行
    if (!m_db.transaction()) {
        m_lastError = m_db.lastError().text();
        return false;
    }

    QSqlQuery markOfflineQuery(m_db);

    //把数据库中原本在线的用户全部标记为离线
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

    //逐个写入网络层当前发现的在线用户
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

    //遍历网络层传入的 QVariantList，每个元素都应包含peerId、username和ip
    for (const QVariant &item : onlinePeers) {
        const QVariantMap peer = item.toMap();
        //对所有字符串字段进行标准化，防止只含空格的数据写入数据库
        const QString peerId =
            peer.value(QStringLiteral("peerId")).toString().trimmed();

        const QString username =
            peer.value(QStringLiteral("username")).toString().trimmed();

        const QString ip =
            peer.value(QStringLiteral("ip")).toString().trimmed();

        //任意必要字段缺失都会使本次同步整体失败并回滚
        if (peerId.isEmpty() || username.isEmpty() || ip.isEmpty()) {
            m_lastError = QStringLiteral("在线用户数据不完整");
            m_db.rollback();
            return false;
        }

        upsertQuery.bindValue(QStringLiteral(":peer_id"), peerId);
        upsertQuery.bindValue(QStringLiteral(":username"), username);
        upsertQuery.bindValue(QStringLiteral(":ip"), ip);

        //任意一个用户写入失败，都不能保留前面已经写入的数据
        if (!upsertQuery.exec()) {
            m_lastError = upsertQuery.lastError().text();
            m_db.rollback();
            return false;
        }

        //结束本轮查询状态，使预处理语句可以安全用于下一位用户
        upsertQuery.finish();
    }

    //全部用户同步成功后提交事务，使修改正式生效
    if (!m_db.commit()) {
        m_lastError = m_db.lastError().text();
        m_db.rollback();
        return false;
    }

    return true;
}