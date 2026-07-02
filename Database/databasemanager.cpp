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
//           新增：析构函数、增加在线用户事务同步功能
//           修改：用户列表调整为在线优先、用户名排序
//     [v0.1.10] HeZhiyuan    2026-06-14 15:57:04
//         * 新增：删除用户
//     [v0.1.11] HeZhiyuan    2026-06-18 20:23:31
//         * 新增：local_peer_id表，用于保存本机唯一且持久化的peerId
//                loadOrCreateLocalPeerId()，负责读取或创建本机永久peerId
//                normalizePeerId()，负责校验UUID并统一为不带大括号的格式
//     [v0.1.12] HeZhiyuan    2026-06-23 13:43:52
//         * 新增chat_groups、group_members、group_messages表
//           群聊删除时自动级联删除成员关系和群消息
//     [v0.1.13] HeZhiyuan    2026-06-23 14:11:51
//         * 新增：createGroup()，
//           创建群聊和写入全部成员使用同一个数据库事务，任意成员保存失败时自动回滚全部修改
//     [v0.1.14] HeZhiyuan    2026-06-23 14:37:40
//         * 新增：loadGroups()和loadGroupMembers()
//           群成员通过LEFT JOIN读取当前IP和在线状态
//           isSelf通过local_peer_id动态计算
//     [v0.1.15] HeZhiyuan    2026-06-23
//         * 新增：saveGroupMessage()，保存消息和更新chat_groups.updated_at使用同一个事务；
//                loadGroupMessages()，读取聊天记录，并按照group_message_id从旧到新返回
#include "databasemanager.h"

#include <QDir>
#include <QDebug>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QVariantMap>
#include <QVariant>
#include <QUuid>
#include <QSet>

//为当前DatabaseManager生成唯一连接名，QSqlDatabase的连接由全局连接池按名称管理，使用UUID防止多个对象意外共用或覆盖同一连接
DatabaseManager::DatabaseManager(QObject *parent): QObject(parent)
{}

//关闭数据库连接并将其从Qt SQL全局连接池中移除
DatabaseManager::~DatabaseManager()
{
    if (m_db.isValid()) {m_db.close();}

    //必须先清空当前QSqlDatabase句柄
    //再从Qt的连接池中删除连接
    m_db = QSqlDatabase();
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
    m_db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"));
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

//初始化数据库表
//1.local_peer_id：保存本机UUID
//2.peers：保存局域网聊天用户
//3.messages：保存私聊聊天消息
//4.chat_groups：保存群聊基本信息
//5.group_members：保存群聊和成员之间的关系
//6.group_messages：保存群聊消息
bool DatabaseManager::initSchema()
{

    //创建local_peer_id表:
    //id固定为1，作用是限制整张表只能保存一条本机身份记录, peer_id保存网络层使用的永久UUID
    const QString createUuidSql = R"(
        CREATE TABLE IF NOT EXISTS local_peer_id(
            id INTEGER PRIMARY KEY CHECK(id = 1),
            peer_id TEXT NOT NULL UNIQUE
        )
    )";

    if (!execSql(createUuidSql)) {
        return false;
    }

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
            content TEXT NOT NULL,
            created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
            FOREIGN KEY(peer_id) REFERENCES peers(peer_id) ON DELETE CASCADE
        )
    )";

    if (!execSql(createMessagesSql)) {
        return false;
    }

    //给messages表建立索引。
    //查询聊天记录时经常使用：WHERE peer_id = ? ORDER BY message_id
    //建立(peer_id, message_id)组合索引，可以减少数据库扫描量
    const QString createMessagesByIdIndexSql = R"(
        CREATE INDEX IF NOT EXISTS idx_messages_peer_message_id
        ON messages(peer_id, message_id)
    )";

    if (!execSql(createMessagesByIdIndexSql)) {
        return false;
    }

    //创建群聊基本信息表
    //group_id使用网络层生成的唯一标识
    //group_name保存界面显示的群名称
    //creator_id保存创建该群聊的用户UUID
    //created_at记录群聊第一次创建的时间
    //updated_at用于后续按最近活动时间排列群聊
    const QString createChatGroupsSql = R"(
        CREATE TABLE IF NOT EXISTS chat_groups(
            group_id TEXT PRIMARY KEY,
            group_name TEXT NOT NULL CHECK(length(trim(group_name)) > 0),
            creator_id TEXT NOT NULL,
            created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
            updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
        )
    )";

    if (!execSql(createChatGroupsSql)) {
        return false;
    }

    //创建群成员关系表
    //一行表示“一名用户属于一个群聊”
    //同一个用户可以属于多个群聊，同一个群聊也可以包含多名用户，
    const QString createGroupMembersSql = R"(
        CREATE TABLE IF NOT EXISTS group_members(
            group_id TEXT NOT NULL,
            peer_id TEXT NOT NULL,
            username TEXT NOT NULL CHECK(length(trim(username)) > 0),
            member_order INTEGER NOT NULL CHECK(member_order >= 0),
            joined_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
            PRIMARY KEY(group_id, peer_id),
            UNIQUE(group_id, member_order),

            FOREIGN KEY(group_id) REFERENCES chat_groups(group_id) ON DELETE CASCADE
        )
    )";

    if (!execSql(createGroupMembersSql)) {
        return false;
    }

    //创建群聊消息表
    //group_id表示消息属于哪个群聊
    //sender_id和sender_name保存发送者身份和发送时的名称
    //from_me表示消息方向：1表示本机发送；0表示其他群成员发送
    //群聊被删除时，其消息由ON DELETE CASCADE自动删除
    const QString createGroupMessagesSql = R"(
        CREATE TABLE IF NOT EXISTS group_messages(
            group_message_id INTEGER PRIMARY KEY AUTOINCREMENT,
            group_id TEXT NOT NULL,
            sender_id TEXT NOT NULL,
            sender_name TEXT NOT NULL CHECK(length(trim(sender_name)) > 0),
            from_me INTEGER NOT NULL CHECK(from_me IN (0, 1)),
            content TEXT NOT NULL CHECK(length(trim(content)) > 0),
            created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,

            FOREIGN KEY(group_id) REFERENCES chat_groups(group_id) ON DELETE CASCADE
        )
    )";

    if (!execSql(createGroupMessagesSql)) {
        return false;
    }

    //按照peer_id查询该用户加入了哪些群聊时使用
    const QString createGroupMembersPeerIndexSql = R"(
        CREATE INDEX IF NOT EXISTS idx_group_members_peer_id ON group_members(peer_id)
    )";

    if (!execSql(createGroupMembersPeerIndexSql)) {
        return false;
    }

    //读取群聊历史记录时执行：WHERE group_id = ? ORDER BY group_message_id组合索引可以减少查询时扫描的数据量
    const QString createGroupMessagesIndexSql = R"(
        CREATE INDEX IF NOT EXISTS idx_group_messages_group_message_id
        ON group_messages(group_id, group_message_id)
    )";

    if (!execSql(createGroupMessagesIndexSql)) {
        return false;
    }

    return true;
}

//当前数据库文件的完整路径，方便运行后检查数据库文件位置
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

//校验并规范化用户UUID
QString DatabaseManager::normalizePeerId(const QString &peerId)
{
    //先去除字符串首尾空白
    const QString trimmedPeerId = peerId.trimmed();

    //空字符串不是有效UUID
    if (trimmedPeerId.isEmpty()) { return {};}

    //QUuid::fromString负责解析UUID,它可以识别带大括号和不带大括号的UUID格式
    const QUuid parsedUuid = QUuid::fromString(trimmedPeerId);

    //解析失败或传入的是全零UUID时，parsedUuid会是null
    if (parsedUuid.isNull()) { return {};}

    //数据库统一保存成不带大括号的标准Uuid格式
    return parsedUuid.toString(QUuid::WithoutBraces);
}

//读取或创建本机永久peer_id
//第一次运行时，local_peer_id表中没有id=1的记录，函数会把网络层生成的candidatePeerId保存进去
//后续运行时，id=1已经存在，INSERT OR IGNORE不会覆盖原来的peer_id，函数最终读取并返回第一次保存的永久peer_id
bool DatabaseManager::loadOrCreateLocalPeerId( const QString &candidatePeerId, QString &persistentPeerId)
{
    //清理上一次错误
    m_lastError.clear();

    //先清空persistentPeerId,这样调用失败后，不会误用之前残留的ID
    persistentPeerId.clear();

    if (!m_db.isOpen()) {
        m_lastError = QStringLiteral("数据库未打开");
        return false;
    }

    //第一次生成的ID也要经过UUID校验
    const QString normalizedCandidateId = normalizePeerId(candidatePeerId);

    if (normalizedCandidateId.isEmpty()) {
        m_lastError = QStringLiteral("网络层生成的本机ID不是有效UUID");
        return false;
    }

    QSqlQuery insertQuery(m_db);

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

    QSqlQuery selectQuery(m_db);

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
    const QString normalizedStoredId = normalizePeerId(selectQuery.value(0).toString());

    if (normalizedStoredId.isEmpty()) {
        m_lastError = QStringLiteral("数据库中保存的本机ID不是有效UUID");
        return false;
    }

    //把数据库确定的ID返回
    persistentPeerId = normalizedStoredId;
    return true;
}

//检查十位群ID是否已经被数据库中的群聊占用
bool DatabaseManager::groupExists(
    const QString &groupId)
{
    m_lastError.clear();

    if (!m_db.isOpen()) {
        m_lastError =
            QStringLiteral("数据库未打开");

        return false;
    }

    const QString normalizedGroupId =
        groupId.trimmed();

    if (normalizedGroupId.isEmpty()) {
        m_lastError =
            QStringLiteral("群聊ID为空");

        return false;
    }

    QSqlQuery query(m_db);

    const QString sql = R"(
        SELECT 1
        FROM chat_groups
        WHERE group_id = :group_id
        LIMIT 1
    )";

    if (!query.prepare(sql)) {
        m_lastError =
            QStringLiteral("准备检查群聊ID SQL失败：")
            + query.lastError().text();

        return false;
    }

    query.bindValue(
        QStringLiteral(":group_id"),
        normalizedGroupId);

    if (!query.exec()) {
        m_lastError =
            QStringLiteral("检查群聊ID失败：")
            + query.lastError().text();

        return false;
    }

    //查询到一行，表示该群ID已经存在。
    return query.next();
}

//创建一个群聊，并保存全部群成员
bool DatabaseManager::createGroup(const QString &groupId,
                                  const QString &groupName,
                                  const QString &creatorId,
                                  const QVariantList &members)
{
    m_lastError.clear();

    if (!m_db.isOpen()) {
        m_lastError = QStringLiteral("数据库未打开");
        return false;
    }

    const QString normalizedGroupId = groupId.trimmed();
    const QString normalizedGroupName = groupName.trimmed();
    const QString normalizedCreatorId = normalizePeerId(creatorId);

    if (normalizedGroupName.isEmpty()) {
        m_lastError = QStringLiteral("群聊名称不能为空");
        return false;
    }

    if (normalizedCreatorId.isEmpty()) {
        m_lastError = QStringLiteral("群聊创建者ID无效");
        return false;
    }

    //数据库层再次检查，不能只依赖QML状态
    if (members.size() < 3) {
        m_lastError = QStringLiteral("群聊成员不能少于三人");
        return false;
    }

    QSet<QString> memberIds;
    QVariantList normalizedMembers;
    normalizedMembers.reserve(members.size());

    bool creatorIncluded = false;

    for (const QVariant &item : members) {
        const QVariantMap member = item.toMap();

        const QString memberId = normalizePeerId(member.value(QStringLiteral("peerId")).toString());

        const QString username = member.value(QStringLiteral("username")).toString().trimmed();

        if (memberId.isEmpty()) {
            m_lastError = QStringLiteral("群成员peerId无效");
            return false;
        }

        if (username.isEmpty()) {
            m_lastError = QStringLiteral("群成员用户名不能为空");
            return false;
        }

        if (memberIds.contains(memberId)) {
            m_lastError = QStringLiteral("群成员列表中存在重复用户：") + username;
            return false;
        }

        memberIds.insert(memberId);
        creatorIncluded = creatorIncluded || memberId == normalizedCreatorId;

        QVariantMap normalizedMember;
        normalizedMember.insert(QStringLiteral("peerId"), memberId);
        normalizedMember.insert(QStringLiteral("username"),username);

        normalizedMembers.append(normalizedMember);
    }

    if (!creatorIncluded) {
        m_lastError = QStringLiteral("创建者不在群成员列表中");
        return false;
    }


    //群ID已经存在时，只允许同一创建者重复写入
    QSqlQuery existingQuery(m_db);

    existingQuery.prepare(QStringLiteral(
        "SELECT creator_id "
        "FROM chat_groups "
        "WHERE group_id = :group_id"));

    existingQuery.bindValue(QStringLiteral(":group_id"), normalizedGroupId);

    if (!existingQuery.exec()) {
        m_lastError = QStringLiteral("检查群聊ID失败：") + existingQuery.lastError().text();
        return false;
    }

    if (existingQuery.next()) {
        const QString existingCreatorId = normalizePeerId(existingQuery.value(0).toString());
        if (existingCreatorId != normalizedCreatorId) {
            m_lastError = QStringLiteral("群聊ID已被其他群占用");
            return false;
        }
    }

    if (!m_db.transaction()) {
        m_lastError = QStringLiteral("开启群聊保存事务失败：") + m_db.lastError().text();
        return false;
    }

    QSqlQuery groupQuery(m_db);

    //  重复邀请时更新群名称和创建者
    const QString upsertGroupSql = R"(
        INSERT INTO chat_groups(
            group_id,
            group_name,
            creator_id,
            created_at,
            updated_at
        )
        VALUES(
            :group_id,
            :group_name,
            :creator_id,
            CURRENT_TIMESTAMP,
            CURRENT_TIMESTAMP
        )
        ON CONFLICT(group_id) DO UPDATE SET
            group_name = excluded.group_name,
            creator_id = excluded.creator_id
    )";

    if (!groupQuery.prepare(upsertGroupSql)) {
        m_lastError = QStringLiteral("准备保存群聊SQL失败：") + groupQuery.lastError().text();
        m_db.rollback();
        return false;
    }

    groupQuery.bindValue(QStringLiteral(":group_id"), normalizedGroupId);

    groupQuery.bindValue(QStringLiteral(":group_name"), normalizedGroupName);

    groupQuery.bindValue(QStringLiteral(":creator_id"), normalizedCreatorId);

    if (!groupQuery.exec()) {
        m_lastError = QStringLiteral("保存群聊基本信息失败：") + groupQuery.lastError().text();
        m_db.rollback();
        return false;
    }

    //邀请可能重复到达
    QSqlQuery deleteMembersQuery(m_db);

    deleteMembersQuery.prepare(QStringLiteral("DELETE FROM group_members " "WHERE group_id = :group_id"));

    deleteMembersQuery.bindValue(QStringLiteral(":group_id"), normalizedGroupId);

    if (!deleteMembersQuery.exec()) {
        m_lastError = QStringLiteral("清理旧群成员失败：") + deleteMembersQuery.lastError().text();
        m_db.rollback();
        return false;
    }

    QSqlQuery memberQuery(m_db);

    const QString insertMemberSql = R"(
        INSERT INTO group_members(
            group_id,
            peer_id,
            username,
            member_order,
            joined_at
        )
        VALUES(
            :group_id,
            :peer_id,
            :username,
            :member_order,
            CURRENT_TIMESTAMP
        )
    )";

    if (!memberQuery.prepare(insertMemberSql)) {
        m_lastError = QStringLiteral("准备保存群成员SQL失败：") + memberQuery.lastError().text();
        m_db.rollback();
        return false;
    }

    int memberOrder = 0;

    for (const QVariant &item : normalizedMembers) {
        const QVariantMap member = item.toMap();

        memberQuery.bindValue(QStringLiteral(":group_id"), normalizedGroupId);

        memberQuery.bindValue(QStringLiteral(":peer_id"), member.value(QStringLiteral("peerId")));

        memberQuery.bindValue(QStringLiteral(":username"), member.value(QStringLiteral("username")));

        memberQuery.bindValue(QStringLiteral(":member_order"), memberOrder);

        if (!memberQuery.exec()) {
            m_lastError = QStringLiteral("保存群成员失败：") + memberQuery.lastError().text();
            m_db.rollback();
            return false;
        }
        memberQuery.finish();
        memberOrder++;
    }

    if (!m_db.commit()) {
        m_lastError = QStringLiteral("提交创建群聊事务失败：") + m_db.lastError().text();
        m_db.rollback();
        return false;
    }

    return true;
}

//删除群聊
bool DatabaseManager::deleteGroup(const QString &groupId)
{
    m_lastError.clear();

    if (!m_db.isOpen()) {
        m_lastError = QStringLiteral("数据库未打开");
        return false;
    }

    const QString normalizedGroupId = groupId.trimmed();

    QSqlQuery query(m_db);

    query.prepare(QStringLiteral("DELETE FROM chat_groups " "WHERE group_id = :group_id"));

    query.bindValue(QStringLiteral(":group_id"), normalizedGroupId);

    if (!query.exec()) {
        m_lastError = QStringLiteral("删除群聊失败：") + query.lastError().text();
        return false;
    }

    return true;
}

//读取指定群聊中的全部成员
//成员顺序按照创建群聊时保存的member_order恢复
QVariantList DatabaseManager::loadGroupMembers(const QString &groupId)
{
    QVariantList members;

    //开始新的数据库操作前清理旧错误
    m_lastError.clear();

    if (!m_db.isOpen()) {
        m_lastError = QStringLiteral("数据库未打开");
        return members;
    }

    //群ID去除首尾空白
    const QString normalizedGroupId = groupId.trimmed();

    if (normalizedGroupId.isEmpty()) {
        m_lastError = QStringLiteral("群聊ID为空");
        return members;
    }

    QSqlQuery query(m_db);

    const QString sql = R"(
        SELECT
            gm.peer_id,
            gm.username,
            gm.member_order,
            gm.joined_at,
            COALESCE(p.ip, '') AS current_ip,
            COALESCE(p.online, 0) AS current_online,

            CASE
                WHEN gm.peer_id = (
                    SELECT peer_id
                    FROM local_peer_id
                    WHERE id = 1
                )
                THEN 1
                ELSE 0
            END AS is_self

        FROM group_members AS gm

        LEFT JOIN peers AS p ON p.peer_id = gm.peer_id

        WHERE gm.group_id = :group_id

        ORDER BY gm.member_order ASC
    )";

    if (!query.prepare(sql)) {
        m_lastError = QStringLiteral("准备读取群成员SQL失败：") + query.lastError().text();
        qWarning() << m_lastError;
        return members;
    }

    query.bindValue(QStringLiteral(":group_id"), normalizedGroupId);

    if (!query.exec()) {
        m_lastError = QStringLiteral("读取群成员失败：") + query.lastError().text();
        qWarning() << m_lastError;
        return members;
    }

    while (query.next()) {
        QVariantMap member;

        //把当前群ID一起返回，方便检查这些成员属于哪个群
        member.insert(QStringLiteral("groupId"), normalizedGroupId);

        member.insert(QStringLiteral("peerId"), query.value(0).toString());

        member.insert(QStringLiteral("username"), query.value(1).toString());

        member.insert(QStringLiteral("memberOrder"), query.value(2).toInt());

        member.insert(QStringLiteral("joinedAt"), query.value(3).toString());

        //IP和在线状态来自peers表的当前记录
        member.insert(QStringLiteral("ip"), query.value(4).toString());

        member.insert(QStringLiteral("online"), query.value(5).toInt() != 0);

        //isSelf没有保存在group_members中，而是与local_peer_id动态比较后得到
        member.insert(QStringLiteral("isSelf"), query.value(6).toInt() != 0);

        members.append(member);
    }

    return members;
}

//读取全部群聊基本信息，返回群聊列表显示需要的数据，
//不会读取群消息，避免程序启动时加载大量历史记录
QVariantList DatabaseManager::loadGroups()
{
    QVariantList groups;

    m_lastError.clear();

    if (!m_db.isOpen()) {
        m_lastError = QStringLiteral("数据库未打开");
        return groups;
    }

    QSqlQuery query(m_db);

    const QString sql = R"(
        SELECT
            g.group_id,
            g.group_name,
            g.creator_id,
            g.created_at,
            g.updated_at,
            (
                SELECT COUNT(*)
                FROM group_members AS count_member
                WHERE count_member.group_id = g.group_id
            ) AS member_count,

            COALESCE(
                (
                    SELECT GROUP_CONCAT(ordered_member.username, '、')
                    FROM (
                        SELECT username
                        FROM group_members
                        WHERE group_id = g.group_id
                        ORDER BY member_order ASC
                    ) AS ordered_member
                ),

                ''
            ) AS member_summary

        FROM chat_groups AS g
        ORDER BY g.updated_at DESC, g.created_at DESC, g.group_id ASC
    )";

    if (!query.prepare(sql)) {
        m_lastError = QStringLiteral("准备读取群聊列表SQL失败：") + query.lastError().text();
        qWarning() << m_lastError;
        return groups;
    }

    if (!query.exec()) {
        m_lastError = QStringLiteral("读取群聊列表失败：") + query.lastError().text();
        qWarning() << m_lastError;
        return groups;
    }

    while (query.next()) {
        QVariantMap group;

        group.insert(QStringLiteral("groupId"), query.value(0).toString());

        group.insert(QStringLiteral("groupName"), query.value(1).toString());

        group.insert(QStringLiteral("creatorId"), query.value(2).toString());

        group.insert(QStringLiteral("createdAt"), query.value(3).toString());

        group.insert(QStringLiteral("updatedAt"), query.value(4).toString());

        group.insert(QStringLiteral("memberCount"), query.value(5).toInt());

        group.insert(QStringLiteral("memberSummary"), query.value(6).toString());

        groups.append(group);
    }

    return groups;
}

//保存一条群聊消息
bool DatabaseManager::saveGroupMessage(const QString &groupId,
                                       const QString &senderId,
                                       const QString &senderName,
                                       bool fromMe,
                                       const QString &content)
{
    m_lastError.clear();

    if (!m_db.isOpen()) {
        m_lastError = QStringLiteral("数据库未打开");
        return false;
    }

    //这里去除群id首尾空白
    const QString normalizedGroupId = groupId.trimmed();

    //发送者id是用户的peerId，执行UUID格式校验和标准化
    const QString normalizedSenderId = normalizePeerId(senderId);

    //消息正文保存前去除首尾空白
    const QString normalizedContent = content.trimmed();

    //发送者名称优先使用网络层或调用者传入的当前名称
    QString normalizedSenderName = senderName.trimmed();

    if (normalizedGroupId.isEmpty()) {
        m_lastError = QStringLiteral("群聊ID为空");
        return false;
    }

    if (normalizedSenderId.isEmpty()) {
        m_lastError = QStringLiteral("群消息发送者peerId无效");
        return false;
    }

    if (normalizedContent.isEmpty()) {
        m_lastError = QStringLiteral("群消息内容为空");
        return false;
    }

    //在保存消息前检查
    //1.群聊是否存在
    //2.发送者是否属于这个群聊
    //3.在senderName为空时，可以使用成员表中的名称作为后备值
    //不能让一个不属于该群的peerId向该群的历史记录中写入消息
    QSqlQuery memberQuery(m_db);

    const QString selectMemberSql = R"(
        SELECT username FROM group_members
        WHERE group_id = :group_id AND peer_id = :peer_id
        LIMIT 1
    )";

    if (!memberQuery.prepare(selectMemberSql)) {
        m_lastError = QStringLiteral("准备检查群成员SQL失败：") + memberQuery.lastError().text();
        qWarning() << m_lastError;
        return false;
    }

    memberQuery.bindValue(QStringLiteral(":group_id"), normalizedGroupId);

    memberQuery.bindValue(QStringLiteral(":peer_id"), normalizedSenderId);

    if (!memberQuery.exec()) {
        m_lastError = QStringLiteral("检查群消息发送者失败：") + memberQuery.lastError().text();
        qWarning() << m_lastError;
        return false;
    }

    //查询不到记录，可能是群不存在，也可能是发送者不属于该群
    if (!memberQuery.next()) {
        m_lastError = QStringLiteral("群聊不存在或消息发送者不是该群成员");
        return false;
    }

    //网络层没有提供有效用户名时，使用创建群聊时保存在group_members中的用户名
    if (normalizedSenderName.isEmpty()) {
        normalizedSenderName = memberQuery.value(0).toString().trimmed();
    }

    if (normalizedSenderName.isEmpty()) {
        m_lastError = QStringLiteral("群消息发送者名称为空");
        return false;
    }


    //开启事务
    //1.插入群聊消息
    //2.更新群聊最近活动时间
    if (!m_db.transaction()) {
        m_lastError = QStringLiteral("开启群消息保存事务失败：") + m_db.lastError().text();
        return false;
    }
    QSqlQuery insertQuery(m_db);
    const QString insertMessageSql = R"(
        INSERT INTO group_messages(
            group_id,
            sender_id,
            sender_name,
            from_me,
            content,
            created_at
        )
        VALUES(
            :group_id,
            :sender_id,
            :sender_name,
            :from_me,
            :content,
            CURRENT_TIMESTAMP
        )
    )";

    if (!insertQuery.prepare(insertMessageSql)) {
        m_lastError = QStringLiteral("准备保存群消息SQL失败：") + insertQuery.lastError().text();
        m_db.rollback();
        return false;
    }

    insertQuery.bindValue(QStringLiteral(":group_id"), normalizedGroupId);

    insertQuery.bindValue(QStringLiteral(":sender_id"), normalizedSenderId);

    insertQuery.bindValue(QStringLiteral(":sender_name"), normalizedSenderName);

    insertQuery.bindValue(QStringLiteral(":from_me"), fromMe ? 1 : 0);

    insertQuery.bindValue(QStringLiteral(":content"), normalizedContent);

    if (!insertQuery.exec()) {
        m_lastError = QStringLiteral("保存群消息失败：") + insertQuery.lastError().text();
        m_db.rollback();
        return false;
    }


    //保存消息后更新群聊最近活动时间
    //loadGroups()当前按照updated_at倒序排列
    //更新后该群就能够移动到群聊列表顶部
    QSqlQuery updateGroupQuery(m_db);

    const QString updateGroupSql = R"(
        UPDATE chat_groups
        SET updated_at = CURRENT_TIMESTAMP
        WHERE group_id = :group_id
    )";

    if (!updateGroupQuery.prepare(updateGroupSql)) {
        m_lastError = QStringLiteral("准备更新群聊活动时间SQL失败：") + updateGroupQuery.lastError().text();
        m_db.rollback();
        return false;
    }

    updateGroupQuery.bindValue(QStringLiteral(":group_id"), normalizedGroupId);

    if (!updateGroupQuery.exec()) {
        m_lastError = QStringLiteral("更新群聊活动时间失败：") + updateGroupQuery.lastError().text();
        m_db.rollback();
        return false;
    }

    //只有插入消息和更新时间全部成功，才提交事务
    if (!m_db.commit()) {
        m_lastError = QStringLiteral("提交群消息保存事务失败：") + m_db.lastError().text();
        m_db.rollback();
        return false;
    }

    return true;
}

//读取指定群聊最近的历史消息
//先按消息ID倒序取得最新的一批，再升序返回，
QVariantList DatabaseManager::loadGroupMessages(const QString &groupId,
                                                int limit)
{
    QVariantList messages;

    m_lastError.clear();

    if (!m_db.isOpen()) {
        m_lastError = QStringLiteral("数据库未打开");
        return messages;
    }

    //去除首尾空白
    const QString normalizedGroupId = groupId.trimmed();

    if (normalizedGroupId.isEmpty()) {
        m_lastError = QStringLiteral("群聊ID为空");
        return messages;
    }


    //限制读取数量。防止传入负数、0或异常大的值，造成一次读取过多历史消息。
    if (limit <= 0) { limit = 5000;}

    if (limit > 9999) { limit = 9999;}

    QSqlQuery query(m_db);


    //内层查询按group_message_id倒序取得最新limit条记录
    //外层查询再按照group_message_id升序排列
    //最终返回从旧到新的消息列表
    const QString sql = R"(
        SELECT
            group_message_id,
            group_id,
            sender_id,
            sender_name,
            from_me,
            content,
            created_at
        FROM (
            SELECT
                group_message_id,
                group_id,
                sender_id,
                sender_name,
                from_me,
                content,
                created_at
            FROM group_messages
            WHERE group_id = :group_id
            ORDER BY group_message_id DESC
            LIMIT :limit
        )
        ORDER BY group_message_id ASC
    )";

    if (!query.prepare(sql)) {
        m_lastError = QStringLiteral("准备读取群聊消息SQL失败：") + query.lastError().text();
        qWarning() << m_lastError;
        return messages;
    }

    query.bindValue(QStringLiteral(":group_id"), normalizedGroupId);

    query.bindValue(QStringLiteral(":limit"), limit);
    if (!query.exec()) {
        m_lastError = QStringLiteral("读取群聊消息失败：") + query.lastError().text();
        qWarning() << m_lastError;
        return messages;
    }
    while (query.next()) {
        QVariantMap message;

        message.insert(QStringLiteral("groupMessageId"), query.value(0).toLongLong());

        message.insert(QStringLiteral("groupId"), query.value(1).toString());

        message.insert(QStringLiteral("senderId"), query.value(2).toString());

        message.insert(QStringLiteral("senderName"), query.value(3).toString());

        message.insert(QStringLiteral("fromMe"), query.value(4).toInt() != 0);

        message.insert(QStringLiteral("content"), query.value(5).toString());

        message.insert(QStringLiteral("createdAt"),query.value(6).toString());

        messages.append(message);
    }

    return messages;
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