// Module
// File: groupchatdatabase.cpp   Version: 0.1.0   License: AGPLv3
// Created: HeZhiyuan      2026-06-24
// Description:    添加群聊、群成员和群消息相关的函数
//
//     [v0.1.1] ZhouChengWei     2026-06-26 16:42:35
//         * 修改了群成员表的名字获取从用户表里获取，实现了修改名字时同步显示到群成员列表里

#include "groupchatdatabase.h"

#include "databasecore.h"
#include "databasecheck.h"

#include <QSet>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariantMap>

GroupChatDatabase::GroupChatDatabase(DatabaseCore &databaseCore)
    : m_databaseCore(databaseCore)
{}

//创建群聊基本信息表、群成员表、群消息表和相关索引，在同一事务中创建，任何一步失败都会回滚，避免数据库只存在部分群聊结构
bool GroupChatDatabase::initSchema()
{
    m_lastError.clear();
    //取得DatabaseCore管理的连接
    QSqlDatabase database = m_databaseCore.database();

    if (!database.isValid() || !database.isOpen()) {
        m_lastError = QStringLiteral("数据库未打开");

        return false;
    }

    //开启表结构初始化事务
    if (!database.transaction()) {
        m_lastError = database.lastError().text();
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

    if (!m_databaseCore.execute(createChatGroupsSql, m_lastError)) {
        database.rollback();
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

    if (!m_databaseCore.execute(createGroupMembersSql, m_lastError)) {
        database.rollback();
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

    if (!m_databaseCore.execute(createGroupMessagesSql, m_lastError)) {
        database.rollback();
        return false;
    }


    //按照peer_id查询该用户加入了哪些群聊时使用
    const QString createGroupMembersPeerIndexSql = R"(
        CREATE INDEX IF NOT EXISTS idx_group_members_peer_id ON group_members(peer_id)
    )";

    if (!m_databaseCore.execute(createGroupMembersPeerIndexSql, m_lastError)) {
        database.rollback();
        return false;
    }


    //读取群聊历史记录时执行：WHERE group_id = ? ORDER BY group_message_id组合索引可以减少查询时扫描的数据量
    const QString createGroupMessagesIndexSql = R"(
        CREATE INDEX IF NOT EXISTS idx_group_messages_group_message_id
        ON group_messages(group_id, group_message_id)
    )";

    if (!m_databaseCore.execute(createGroupMessagesIndexSql, m_lastError)) {
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

//查询群ID是否存在
bool GroupChatDatabase::groupExists(const QString &groupId, bool &exists)
{
    m_lastError.clear();

    //无论成功失败，先给输出参数确定值
    exists = false;

    QSqlDatabase database = m_databaseCore.database();

    if (!database.isValid() || !database.isOpen()) {
        m_lastError = QStringLiteral("数据库未打开");
        return false;
    }

    //删除群ID首尾空白
    const QString normalizedGroupId = groupId.trimmed();

    if (!DatabaseCheck::isValidGroupId(normalizedGroupId)) {
        m_lastError = QStringLiteral("群聊ID必须是十位数字");

        return false;
    }

    //创建查询对象
    QSqlQuery query(database);

    //只查询常量1并限制一条记录
    const QString sql = R"(
        SELECT 1
        FROM chat_groups
        WHERE group_id = :group_id
        LIMIT 1
    )";

    if (!query.prepare(sql)) {
        m_lastError = query.lastError().text();
        return false;
    }

    query.bindValue(QStringLiteral(":group_id"), normalizedGroupId);

    if (!query.exec()) {
        m_lastError = query.lastError().text();
        return false;
    }

    //true：存在记录，false：不存在记录
    exists = query.next();

    return true;
}

//校验群ID、群名称、创建者和成员列表，并保存群聊及全部成员
bool GroupChatDatabase::createGroup(const QString &groupId,
                                  const QString &groupName,
                                  const QString &creatorId,
                                  const QVariantList &members)
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

    const QString normalizedGroupId = groupId.trimmed();
    const QString normalizedGroupName = groupName.trimmed();
    //校验群主UUID
    const QString normalizedCreatorId = DatabaseCheck::normalizePeerId(creatorId);

    if (!DatabaseCheck::isValidGroupId(normalizedGroupId)) {
        m_lastError = QStringLiteral("群聊ID必须是十位数字");

        return false;
    }

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

    //保存已经出现的成员ID
    QSet<QString> memberIds;
    //保存校验后的成员数据
    QVariantList normalizedMembers;
    //预留与输入列表相同的大小，减少添加过程中的重新分配
    normalizedMembers.reserve(members.size());
    //记录创建者是否包含在成员列表中
    bool creatorIncluded = false;

    //逐个校验群成员
    for (const QVariant &item : members) {
        const QVariantMap member = item.toMap();

        const QString memberId = DatabaseCheck::normalizePeerId(member.value(QStringLiteral("peerId")).toString());

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

        //记录当前成员ID
        memberIds.insert(memberId);
        //判断当前成员是否为创建者
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


    //群ID已经存在时，只允许创建者重复写入
    QSqlQuery existingQuery(database);

    const QString existingSql = R"(
        SELECT creator_id
        FROM chat_groups
        WHERE group_id = :group_id
    )";

    if (!existingQuery.prepare(existingSql)) {
        m_lastError = existingQuery.lastError().text();
        return false;
    }

    existingQuery.bindValue(QStringLiteral(":group_id"), normalizedGroupId);

    if (!existingQuery.exec()) {
        m_lastError = QStringLiteral("检查群聊ID失败：") + existingQuery.lastError().text();
        return false;
    }

    if (existingQuery.next()) {
        const QString existingCreatorId = DatabaseCheck::normalizePeerId(existingQuery.value(0).toString());
        if (existingCreatorId != normalizedCreatorId) {
            m_lastError = QStringLiteral("群聊ID已被其他群占用");
            return false;
        }
    }

    if (!database.transaction()) {
        m_lastError = QStringLiteral("开启群聊保存事务失败：") + database.lastError().text();
        return false;
    }

    //创建保存群聊基本信息的查询对象
    QSqlQuery groupQuery(database);

    //重复邀请时更新群名称和创建者
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
        database.rollback();
        return false;
    }

    groupQuery.bindValue(QStringLiteral(":group_id"), normalizedGroupId);

    groupQuery.bindValue(QStringLiteral(":group_name"), normalizedGroupName);

    groupQuery.bindValue(QStringLiteral(":creator_id"), normalizedCreatorId);

    if (!groupQuery.exec()) {
        m_lastError = QStringLiteral("保存群聊基本信息失败：") + groupQuery.lastError().text();
        database.rollback();
        return false;
    }

    //邀请可能重复到达，创建清理旧成员关系的查询对象，用最新成员列表整体替换旧列表
    QSqlQuery deleteMembersQuery(database);

    const QString deleteMembersSql = R"(
        DELETE FROM group_members
        WHERE group_id = :group_id
    )";

    if (!deleteMembersQuery.prepare(deleteMembersSql)) {
        m_lastError = deleteMembersQuery.lastError().text();
        database.rollback();
        return false;
    }
    deleteMembersQuery.bindValue(QStringLiteral(":group_id"), normalizedGroupId);

    if (!deleteMembersQuery.exec()) {
        m_lastError = QStringLiteral("清理旧群成员失败：") + deleteMembersQuery.lastError().text();
        database.rollback();
        return false;
    }

    //创建可重复执行的成员插入查询对象
    QSqlQuery memberQuery(database);

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
        database.rollback();
        return false;
    }

    //从0开始记录成员显示顺序
    int memberOrder = 0;

    for (const QVariant &item : normalizedMembers) {
        const QVariantMap member = item.toMap();

        memberQuery.bindValue(QStringLiteral(":group_id"), normalizedGroupId);

        memberQuery.bindValue(QStringLiteral(":peer_id"), member.value(QStringLiteral("peerId")));

        memberQuery.bindValue(QStringLiteral(":username"), member.value(QStringLiteral("username")));

        memberQuery.bindValue(QStringLiteral(":member_order"), memberOrder);

        if (!memberQuery.exec()) {
            m_lastError = QStringLiteral("保存群成员失败：") + memberQuery.lastError().text();
            database.rollback();
            return false;
        }
        memberQuery.finish();
        memberOrder++;
    }

    if (!database.commit()) {
        m_lastError = QStringLiteral("提交创建群聊事务失败：") + database.lastError().text();
        database.rollback();
        return false;
    }

    return true;
}

//删除群聊
bool GroupChatDatabase::deleteGroup(const QString &groupId)
{
    m_lastError.clear();

    QSqlDatabase database = m_databaseCore.database();

    if (!database.isValid() || !database.isOpen()) {
        m_lastError = QStringLiteral("数据库未打开");

        return false;
    }

    const QString normalizedGroupId = groupId.trimmed();

    if (!DatabaseCheck::isValidGroupId(normalizedGroupId)) {
        m_lastError = QStringLiteral("群聊ID必须是十位数字");

        return false;
    }

    QSqlQuery query(database);

    const QString sql = R"(
        DELETE FROM chat_groups
        WHERE group_id = :group_id
    )";

    //检查prepare()返回值
    if (!query.prepare(sql)) {
        m_lastError = query.lastError().text();

        return false;
    }

    query.bindValue(QStringLiteral(":group_id"), normalizedGroupId);

    if (!query.exec()) {
        m_lastError = query.lastError().text();

        return false;
    }

    return true;
}

//读取全部群聊基本信息，返回群聊列表显示需要的数据，不会读取群消息，避免程序启动时加载大量历史记录
bool GroupChatDatabase::loadGroups(QVariantList &groups)
{
    m_lastError.clear();

    QSqlDatabase database = m_databaseCore.database();

    if (!database.isValid() || !database.isOpen()) {
        m_lastError = QStringLiteral("数据库未打开");

        return false;
    }

    //群列表汇总查询
    QSqlQuery query(database);

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
        return false;
    }

    if (!query.exec()) {
        m_lastError = QStringLiteral("读取群聊列表失败：") + query.lastError().text();
        qWarning() << m_lastError;
        return false;
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

    return true;
}

//读取指定群聊中的全部成员
//成员顺序按照创建群聊时保存的member_order恢复
bool GroupChatDatabase::loadGroupMembers(const QString &groupId, QVariantList &members)
{
    //开始新的数据库操作前清理旧错误
    m_lastError.clear();

    QSqlDatabase database = m_databaseCore.database();

    if (!database.isValid() || !database.isOpen()) {
        m_lastError = QStringLiteral("数据库未打开");

        return false;
    }

    //群ID去除首尾空白
    const QString normalizedGroupId = groupId.trimmed();

    if (normalizedGroupId.isEmpty()) {
        m_lastError = QStringLiteral("群聊ID为空");
        return false;
    }

    QSqlQuery query(database);

    const QString sql = R"(
        SELECT
            gm.peer_id,
            p.username,
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
        return false;
    }

    query.bindValue(QStringLiteral(":group_id"), normalizedGroupId);

    if (!query.exec()) {
        m_lastError = QStringLiteral("读取群成员失败：") + query.lastError().text();
        qWarning() << m_lastError;
        return false;
    }

    while (query.next()) {
        QVariantMap member;

        //把群ID一起返回，方便检查这些成员属于哪个群
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

    return true;
}

//读取指定群聊最近的历史消息
//先按消息ID倒序取得最新的一批，再升序返回，
bool GroupChatDatabase::loadGroupMessages(const QString &groupId, QVariantList &messages, int limit)
{
    m_lastError.clear();

    QSqlDatabase database = m_databaseCore.database();

    if (!database.isValid() || !database.isOpen()) {
        m_lastError = QStringLiteral("数据库未打开");

        return false;
    }

    //去除首尾空白
    const QString normalizedGroupId = groupId.trimmed();

    if (normalizedGroupId.isEmpty()) {
        m_lastError = QStringLiteral("群聊ID为空");
        return false;
    }


    //限制读取数量。防止传入负数、0或异常大的值，造成一次读取过多历史消息。
    if (limit <= 0) { limit = 5000;}

    if (limit > 9999) { limit = 9999;}

    QSqlQuery query(database);


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
        return false;
    }

    query.bindValue(QStringLiteral(":group_id"), normalizedGroupId);

    query.bindValue(QStringLiteral(":limit"), limit);
    if (!query.exec()) {
        m_lastError = QStringLiteral("读取群聊消息失败：") + query.lastError().text();
        qWarning() << m_lastError;
        return false;
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

    return true;
}

//保存一条群聊消息
bool GroupChatDatabase::saveGroupMessage(const QString &groupId,
                                         const QString &senderId,
                                         const QString &senderName,
                                         const QString &content)
{
    m_lastError.clear();

    QSqlDatabase database = m_databaseCore.database();

    if (!database.isValid() || !database.isOpen()) {
        m_lastError = QStringLiteral("数据库未打开");

        return false;
    }

    //去除群id首尾空白
    const QString normalizedGroupId = groupId.trimmed();

    //发送者id是用户的peerId，执行UUID格式校验和标准化
    const QString normalizedSenderId = DatabaseCheck::normalizePeerId(senderId);

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
    QSqlQuery memberQuery(database);

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
    if (!database.transaction()) {
        m_lastError = QStringLiteral("开启群消息保存事务失败：") + database.lastError().text();
        return false;
    }
    //创建群消息插入查询对象
    QSqlQuery insertQuery(database);
    const QString insertSql = R"(
        INSERT INTO group_messages(
            group_id,
            sender_id,
            sender_name,
            from_me,
            content,
            created_at)
        VALUES(
            :group_id,
            :sender_id,
            :sender_name,

            CASE
                WHEN :sender_id = (SELECT peer_id FROM local_peer_id WHERE id = 1)
                THEN 1
                ELSE 0
            END,
            :content,
            CURRENT_TIMESTAMP
        )
    )";

    if (!insertQuery.prepare(insertSql)) {
        m_lastError = QStringLiteral("准备保存群消息SQL失败：") + insertQuery.lastError().text();
        database.rollback();
        return false;
    }

    insertQuery.bindValue(QStringLiteral(":group_id"), normalizedGroupId);

    insertQuery.bindValue(QStringLiteral(":sender_id"), normalizedSenderId);

    insertQuery.bindValue(QStringLiteral(":sender_name"), normalizedSenderName);

    insertQuery.bindValue(QStringLiteral(":content"), normalizedContent);

    if (!insertQuery.exec()) {
        m_lastError = QStringLiteral("保存群消息失败：") + insertQuery.lastError().text();
        database.rollback();
        return false;
    }


    //保存消息后更新群聊最近活动时间
    //loadGroups()当前按照updated_at倒序排列
    //更新后该群就能够移动到群聊列表顶部
    //创建群聊活动时间更新查询
    QSqlQuery updateGroupQuery(database);

    const QString updateGroupSql = R"(
        UPDATE chat_groups
        SET updated_at = CURRENT_TIMESTAMP
        WHERE group_id = :group_id
    )";

    if (!updateGroupQuery.prepare(updateGroupSql)) {
        m_lastError = QStringLiteral("准备更新群聊活动时间SQL失败：") + updateGroupQuery.lastError().text();
        database.rollback();
        return false;
    }

    updateGroupQuery.bindValue(QStringLiteral(":group_id"), normalizedGroupId);

    if (!updateGroupQuery.exec()) {
        m_lastError = QStringLiteral("更新群聊活动时间失败：") + updateGroupQuery.lastError().text();
        database.rollback();
        return false;
    }

    //只有插入消息和更新时间全部成功，才提交事务
    if (!database.commit()) {
        m_lastError = QStringLiteral("提交群消息保存事务失败：") + database.lastError().text();
        database.rollback();
        return false;
    }

    return true;
}

//返回GroupChatDatabase最近一次数据库操作产生的错误信息
QString GroupChatDatabase::lastError() const
{
    return m_lastError;
}