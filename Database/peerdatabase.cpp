#include "peerdatabase.h"

#include "databasecore.h"
#include "databasecheck.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QVariantMap>

PeerDatabase::PeerDatabase(DatabaseCore &databaseCore)
    : m_databaseCore(databaseCore)
{
}

bool PeerDatabase::initSchema()
{
    m_lastError.clear();

    //取得已经存在的默认连接,这里没有创建新连接
    QSqlDatabase database = m_databaseCore.database();

    if (!database.isValid() || !database.isOpen()) {
        m_lastError = QStringLiteral("数据库未打开");
        return false;
    }

    //两张用户相关表作为一个事务创建，避免出现只创建了一部分表结构
    if (!database.transaction()) {
        m_lastError = database.lastError().text();
        return false;
    }

    const QString createLocalPeerIdSql = R"(
        CREATE TABLE IF NOT EXISTS local_peer_id(
            id INTEGER PRIMARY KEY CHECK(id = 1),
            peer_id TEXT NOT NULL UNIQUE
        )
    )";

    if (!m_databaseCore.execute(createLocalPeerIdSql, m_lastError)) {
        database.rollback();
        return false;
    }

    //CHECK(length(trim(username)) > 0)防止向数据库写入空用户名或全空格用户名
    const QString createPeersSql = R"(
        CREATE TABLE IF NOT EXISTS peers(
            peer_id TEXT PRIMARY KEY,
            username TEXT NOT NULL CHECK(length(trim(username)) > 0),
            ip TEXT NOT NULL,
            online INTEGER NOT NULL DEFAULT 0 CHECK(online IN (0, 1)),
            updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
        )
    )";

    if (!m_databaseCore.execute(createPeersSql, m_lastError)) {
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

//读取或创建本机永久peer_id
//第一次运行时，local_peer_id表中没有id=1的记录，函数会把网络层生成的candidatePeerId保存进去
//后续运行时，id=1已经存在，INSERT OR IGNORE不会覆盖原来的peer_id，函数最终读取并返回第一次保存的永久peer_id
bool PeerDatabase::loadOrCreateLocalPeerId( const QString &candidatePeerId, QString &persistentPeerId)
{
    //清理上一次错误
    m_lastError.clear();

    //先清空persistentPeerId,这样调用失败后，不会误用之前残留的ID
    persistentPeerId.clear();

    QSqlDatabase database = m_databaseCore.database();

    if (!database.isValid() || !database.isOpen()) {
        m_lastError = QStringLiteral("数据库未打开");
        return false;
    }

    //第一次生成的ID也要经过UUID校验
    const QString normalizedCandidateId = DatabaseCheck::normalizePeerId(candidatePeerId);

    if (normalizedCandidateId.isEmpty()) {
        m_lastError = QStringLiteral("网络层生成的本机ID不是有效UUID");
        return false;
    }

    QSqlQuery insertQuery(database);

    //INSERT OR IGNORE的作用：
    //1. 如果local_peer_id不存在，就插入candidatePeerId
    //2. 如果local_peer_id已经存在，因为setting_key是主键，SQLite会忽略本次插入，不覆盖原来的永久ID
    const QString insertSql = R"(
        INSERT OR IGNORE INTO local_peer_id( id, peer_id)
        VALUES( 1, :peer_id)
    )";

    if (!insertQuery.prepare(insertSql)) {
        m_lastError = insertQuery.lastError().text();
        qWarning() << "准备本机ID保存语句失败:" << m_lastError;
        return false;
    }

    //把UUID绑定到:peer_id参数
    insertQuery.bindValue( QStringLiteral(":peer_id"), normalizedCandidateId);

    if (!insertQuery.exec()) {
        m_lastError = insertQuery.lastError().text();
        qWarning() << "保存本机ID失败:" << m_lastError;
        return false;
    }

    QSqlQuery selectQuery(database);

    //读取唯一一条本机身份记录，id固定为1，因此不需要依靠其他查找
    const QString selectSql = R"(
        SELECT peer_id
        FROM local_peer_id
        WHERE id = 1
    )";

    if (!selectQuery.prepare(selectSql)) {
        m_lastError = selectQuery.lastError().text();
        qWarning() << "准备读取本机ID语句失败:" << m_lastError;
        return false;
    }

    if (!selectQuery.exec()) {
        m_lastError = selectQuery.lastError().text();
        qWarning() << "读取本机ID失败:" << m_lastError;
        return false;
    }

    //正常情况下，上面的INSERT OR IGNORE执行后一定能查询到记录。
    if (!selectQuery.next()) {
        m_lastError = QStringLiteral("数据库中没有找到本机ID记录");
        return false;
    }

    //数据库里的值也要重新校验，避免数据库文件被手动修改后，把错误字符串作为网络身份继续广播
    const QString normalizedStoredId = DatabaseCheck::normalizePeerId(selectQuery.value(0).toString());

    if (normalizedStoredId.isEmpty()) {
        m_lastError = QStringLiteral("数据库中保存的本机ID不是有效UUID");
        return false;
    }

    //把数据库确定的ID返回
    persistentPeerId = normalizedStoredId;
    return true;
}

bool PeerDatabase::loadPeers(QVariantList &peers)
{
    m_lastError.clear();

    //调用前先清空输出参数
    peers.clear();

    QSqlDatabase database = m_databaseCore.database();

    if (!database.isValid() || !database.isOpen()) {
        m_lastError = QStringLiteral("数据库未打开");

        return false;
    }

    QSqlQuery query(database);

    const QString sql = R"(
        SELECT
            peer_id,
            username,
            ip,
            online,
            updated_at
        FROM peers
        ORDER BY
            online DESC,
            username COLLATE NOCASE ASC
    )";

    if (!query.exec(sql)) {
        m_lastError = query.lastError().text();
        return false;
    }

    while (query.next()) {
        QVariantMap peer;

        peer.insert(QStringLiteral("peerId"), query.value(0).toString());

        peer.insert(QStringLiteral("username"), query.value(1).toString());

        peer.insert(QStringLiteral("ip"), query.value(2).toString());

        peer.insert(QStringLiteral("online"), query.value(3).toInt() != 0);

        peer.insert(QStringLiteral("updatedAt"), query.value(4).toString());

        peers.append(peer);
    }

    //即使peers为空，只要查询正常完成，仍然返回true
    return true;
}

bool PeerDatabase::upsertPeer(const QString &peerId, const QString &username, const QString &ip, bool online)
{
    m_lastError.clear();

    QSqlDatabase database = m_databaseCore.database();

    if (!database.isValid() || !database.isOpen()) {
        m_lastError = QStringLiteral("数据库未打开");
        return false;
    }

    const QString normalizedPeerId = DatabaseCheck::normalizePeerId(peerId);

    const QString normalizedUsername = username.trimmed();

    const QString normalizedIp = ip.trimmed();

    if (normalizedPeerId.isEmpty()) {
        m_lastError = QStringLiteral("peerId不是有效UUID");
        return false;
    }

    if (normalizedUsername.isEmpty()) {
        m_lastError = QStringLiteral("用户名为空");
        return false;
    }

    if (online && normalizedIp.isEmpty()) {
        m_lastError = QStringLiteral("在线用户IP为空");
        return false;
    }

    QSqlQuery query(database);

    const QString sql = R"(
        INSERT INTO peers(
            peer_id,
            username,
            ip,
            online,
            updated_at
        )
        VALUES(
            :peer_id,
            :username,
            :ip,
            :online,
            CURRENT_TIMESTAMP
        )
        ON CONFLICT(peer_id) DO UPDATE SET
            username = excluded.username,
            ip = excluded.ip,
            online = excluded.online,
            updated_at = CURRENT_TIMESTAMP
        WHERE
            peers.username <> excluded.username
            OR peers.ip <> excluded.ip
            OR peers.online <> excluded.online
    )";
    if (!query.prepare(sql)) {
        m_lastError = query.lastError().text();
        return false;
    }

    query.bindValue(QStringLiteral(":peer_id"), normalizedPeerId);

    query.bindValue(QStringLiteral(":username"), normalizedUsername);

    query.bindValue(QStringLiteral(":ip"), normalizedIp);

    query.bindValue(QStringLiteral(":online"), online ? 1 : 0);

    if (!query.exec()) {
        m_lastError = query.lastError().text();
        return false;
    }

    return true;
}

//删除指定peerId对应的用户记录
bool PeerDatabase::deletePeer(
    const QString &peerId)
{
    m_lastError.clear();

    QSqlDatabase database = m_databaseCore.database();

    //isValid()判断QSqlDatabase句柄是否有效，isOpen()判断数据库文件是否已经成功打开
    if (!database.isValid() || !database.isOpen()) {
        m_lastError = QStringLiteral("数据库未打开");

        return false;
    }

    //将输入的peerId转换为统一UUID格式
    const QString normalizedPeerId = DatabaseCheck::normalizePeerId(peerId);

    //返回空字符串表示输入不是有效UUID
    if (normalizedPeerId.isEmpty()) {
        m_lastError = QStringLiteral("peerId不是有效UUID");

        return false;
    }


    //创建使用当前默认连接的查询对象
    QSqlQuery query(database);

    const QString sql = R"(
        DELETE FROM peers
        WHERE peer_id = :peer_id
    )";

    if (!query.prepare(sql)) {
        m_lastError = QStringLiteral("准备删除用户SQL失败：") + query.lastError().text();

        return false;
    }

    //使用参数绑定传入用户ID
    query.bindValue(QStringLiteral(":peer_id"), normalizedPeerId);

    //执行DELETE
    if (!query.exec()) {
        m_lastError = QStringLiteral("删除用户失败：") + query.lastError().text();

        return false;
    }

    return true;
}

//增加在线用户同步接口同时保留历史用户和离线状态
//历史用户不会被删除：先统一标记离线，再将当前列表中的用户更新为在线
bool PeerDatabase::synchronizePeers(const QVariantList &onlinePeers)
{
    m_lastError.clear();

    QSqlDatabase database = m_databaseCore.database();
    if (!database.isValid() || !database.isOpen()) {
        m_lastError = QStringLiteral("数据库未打开");
        return false;
    }

    //“全部标记离线”和“重新标记在线”必须作为一个原子操作
    //全部成功后提交；任何一步失败则回滚，避免数据库处于半更新状态
    //开启一个数据库事务，确保后面的一组数据库操作被视为一个整体执行
    if (!database.transaction()) {
        m_lastError = database.lastError().text();
        return false;
    }

    QSqlQuery markOfflineQuery(database);

    //把数据库中原本在线的用户全部标记为离线
    if (!markOfflineQuery.exec(R"(
        UPDATE peers
        SET online = 0,updated_at = CURRENT_TIMESTAMP
        WHERE online <> 0
    )")) {
        m_lastError = markOfflineQuery.lastError().text();
        database.rollback();
        return false;
    }

    QSqlQuery upsertQuery(database);

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
        database.rollback();
        return false;
    }

    //遍历网络层传入的 QVariantList，每个元素都应包含peerId、username和ip
    for (const QVariant &item : onlinePeers) {
        const QVariantMap peer = item.toMap();
        //对所有字符串字段进行标准化，防止只含空格的数据写入数据库
        const QString peerId = DatabaseCheck::normalizePeerId(peer.value(QStringLiteral("peerId")).toString());

        const QString username = peer.value(QStringLiteral("username")).toString().trimmed();

        const QString ip = peer.value(QStringLiteral("ip")).toString().trimmed();

        //任意必要字段缺失都会使本次同步整体失败并回滚
        if (peerId.isEmpty() || username.isEmpty() || ip.isEmpty()) {
            m_lastError = QStringLiteral("在线用户数据不完整");
            database.rollback();
            return false;
        }

        upsertQuery.bindValue(QStringLiteral(":peer_id"), peerId);
        upsertQuery.bindValue(QStringLiteral(":username"), username);
        upsertQuery.bindValue(QStringLiteral(":ip"), ip);

        //任意一个用户写入失败，都不能保留前面已经写入的数据
        if (!upsertQuery.exec()) {
            m_lastError = upsertQuery.lastError().text();
            database.rollback();
            return false;
        }

        //结束本轮查询状态，使预处理语句可以安全用于下一位用户
        upsertQuery.finish();
    }

    //全部用户同步成功后提交事务，使修改正式生效
    if (!database.commit()) {
        m_lastError = database.lastError().text();
        database.rollback();
        return false;
    }

    return true;
}

//返回PeerDatabase最近一次数据库操作产生的错误信息
QString PeerDatabase::lastError() const
{
    return m_lastError;
}