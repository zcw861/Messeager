//     [v0.1.2] HeZhiyuan    2026-06-13 14:58:31
//         *连接Chat在线用户变化和消息接收信号
//          初始化 SQLite 数据库和网络服务
//          将网络用户同步到数据库
//          发送消息后保存本地聊天记录
//          接收消息后保存对方聊天记录
//          从数据库刷新用户列表和历史消息
//     [v0.1.3] JiangFan    2026-06-14
//          *增加文件发送功能(待完善)
//     [v0.1.4] ZhouChengWei     2026-06-14 21:27:37
//         * 处理了因为给自己发送消息而接收导致显示2次的问题
//     [v0.1.5] HeZhiyuan    2026-06-18 19:23:35
//         * 在初始化数据库后读取或创建本机永久peerId
//           在Chat网络线程启动前，将持久化peerId设置到网络层
//           在线用户同步时使用网络层提供的UUID作为数据库peer_id
//           IP不作为用户唯一标识
//           接收消息时使用发送者的peerId保存用户信息和聊天记录
//           发送消息时向网络层传递目标peerId，由网络层查询目标当前IP
//     [v0.1.6] HeZhiyuan    2026-06-19 12:22:31
//         * 修改初始化函数：在初始化数据库成功后，网络服务启动前加入本机peerId初始化
//           并将peerId设置到网络层，避免每次运行都会给本机生成一个全新的id
//     [v0.1.7] ZhouChengWei    2026-06-22 10:46:32
//         * 把原来的按照IP发送消息改为ID,判断自己发送的消息也改为ID判断
//     [v0.1.8] ZhouChengWei    2026-06-22 11:29:12
//         * 添加了获取本机IP的函数
//     [v0.1.9] HeZhiyuan    2026-06-23 16:34:42
//         * 增加群聊数据库读取与会话状态管理功能
//     [v0.1.10] HeZhiyuan    2026-06-23 17:23:35
//         * 增加群聊创建与数据库持久化流程
//           创建成功后刷新群聊列表并选中新群聊
//     [v0.2.0] HeZhiyuan    2026-06-24 21:33:10
//         * 适配拆分后的数据库访问层
//           将原有DatabaseManager调用替换为对应的数据访问类调用


#include "appcontroller.h"

#include <QVariantMap>
#include <QDebug>
#include <QFileInfo>
#include <QSettings>
#include <QUuid>
#include <chrono>
#include <vector>

AppController::AppController(QObject *parent)
    : QObject(parent),
    m_peerDatabase(m_databaseCore),
    m_privateChatDatabase(m_databaseCore),
    m_groupChatDatabase(m_databaseCore),
    m_chat(this),
    m_groupChat(&m_chat, this)
{
    //将GroupChat对象交给Chat
    m_chat.setGroupChat(&m_groupChat);

    //接收网络层报告的群聊消息
    connect(&m_groupChat, &GroupChat::groupMessageReceived, this, &AppController::handleGroupMessageReceived);

    //原有的私聊连接继续保留
    connect(&m_chat, &Chat::onlineUsersChanged, this, &AppController::synchronizeOnlineUsers);

    connect(&m_chat, &Chat::messageReceived, this, &AppController::handleMessageReceived);


    connect(&m_chat, &Chat::groupInviteReceived, this, &AppController::handleGroupInviteReceived);

    //收到文件请求
    connect(&m_translateFile, &TranslateFile::fileRequestReceived, this, &AppController::fileRequestReceived
    );

    //文件传输进度条
    connect(&m_translateFile, &TranslateFile::fileTransferProgress, this, &AppController::fileTransferProgress
    );

    //文件传输完成
    connect(&m_translateFile, &TranslateFile::fileTransferFinished, this, &AppController::fileTransferFinished
    );
}

AppController::~AppController()
{
    m_translateFile.stop();
}

QVariantList AppController::peers() const
{
    return m_peers;
}
//返回创建群聊时使用的全部候选成员
QVariantList AppController::groupCandidates() const
{
    QVariantList candidates;

    //用于防止本机或数据库成员重复出现。
    QSet<QString> appendedPeerIds;

    const QString localPeerId = m_chat.localId().trimmed();

    const QString localName = m_chat.localName().trimmed();

    const QString localIp = m_chat.localIp().trimmed();


    //加入本机用户
    const QUuid localUuid = QUuid::fromString(localPeerId);

    if (!localUuid.isNull() && !localName.isEmpty() && !localIp.isEmpty()) {
        const QString normalizedLocalPeerId = localUuid.toString(QUuid::WithoutBraces);

        QVariantMap self;

        self.insert(QStringLiteral("peerId"), normalizedLocalPeerId);

        self.insert(QStringLiteral("username"), localName);

        self.insert(QStringLiteral("ip"), localIp);

        //本机程序正在运行，因此本机固定为在线
        self.insert(QStringLiteral("online"), true);

        self.insert(QStringLiteral("updatedAt"), QString());

        //QML通过该字段保证自己默认选中且不能取消
        self.insert(QStringLiteral("isSelf"), true);

        candidates.append(self);
        appendedPeerIds.insert(normalizedLocalPeerId);
    }

    //m_peers来自DatabaseManager::loadPeers()
    for (const QVariant &value : m_peers) {
        QVariantMap candidate = value.toMap();

        const QString originalPeerId = candidate.value(QStringLiteral("peerId")).toString().trimmed();

        const QUuid peerUuid = QUuid::fromString(originalPeerId);

        //数据库中出现异常ID时，不把异常记录交给群聊协议
        if (peerUuid.isNull()) {
            continue;
        }

        const QString peerId = peerUuid.toString(QUuid::WithoutBraces);

        //避免本机在数据库中存在记录时重复显示
        if (appendedPeerIds.contains(peerId)) {
            continue;
        }

        const QString username = candidate.value(QStringLiteral("username")).toString().trimmed();

        //没有用户名的异常记录不能作为群聊候选成员
        if (username.isEmpty()) {
            continue;
        }

        candidate.insert(QStringLiteral("peerId"), peerId);

        candidate.insert(QStringLiteral("isSelf"), false);

        //不修改online字段
        candidates.append(candidate);
        appendedPeerIds.insert(peerId);
    }

    return candidates;
}

//返回当前选中私聊对象的消息
QVariantList AppController::messages() const
{
    return m_messages;
}

//返回已经加载到控制器中的群聊列表缓存
QVariantList AppController::groups() const
{
    return m_groups;
}

//返回当前选中群聊的成员缓存
QVariantList AppController::groupMembers() const
{
    return m_groupMembers;
}

//返回当前选中群聊的历史消息缓存
QVariantList AppController::groupMessages() const
{
    return m_groupMessages;
}

QString AppController::lastError() const
{
    return m_lastError;
}

bool AppController::ready() const
{
    return m_ready;
}

bool AppController::containsProtocolSeparator(const QString &text)
{
    return text.contains(QLatin1Char('\n')) || text.contains(QLatin1Char('\r'))||text.contains(QLatin1Char('\t'));
}

//初始化应用后端。顺序为：校验用户名、打开数据库、初始化表结构、读取历史用户、启动网络服务、更新 ready 状态
bool AppController::initialize(const QString &userName)
{
    //防止重复启动网络线程和重复初始化数据库
    if (m_ready) {
        return true;
    }

    //用户名去除首尾空格后再进行校验和传递
    const QString normalizedName = userName.trimmed();

    if (normalizedName.isEmpty()) {
        reportError(QStringLiteral("用户名不能为空"));
        return false;
    }

    if (containsProtocolSeparator(normalizedName)) {
        reportError(QStringLiteral("用户名不能包含换行或制表符"));
        return false;
    }


    //第一步：创建并打开唯一默认连接
    if (!m_databaseCore.open()) {
        reportError(QStringLiteral("数据库打开失败：") + m_databaseCore.lastError());

        return false;
    }


    //第二步：创建用户和本机身份表，必须最先创建，因为私聊messages表外键依赖peers表
    if (!m_peerDatabase.initSchema()) {
        reportError(QStringLiteral("用户数据库初始化失败：") + m_peerDatabase.lastError());
        return false;
    }


    //第三步：创建私聊消息表和索引
    if (!m_privateChatDatabase.initSchema()) {
        reportError(QStringLiteral("私聊数据库初始化失败：") + m_privateChatDatabase.lastError());
        return false;
    }


    //第四步：创建群聊相关表和索引
    if (!m_groupChatDatabase.initSchema()) {
        reportError(QStringLiteral("群聊数据库初始化失败：") + m_groupChatDatabase.lastError());
        return false;
    }

    //数据库存储的peerId
    QString persistentLocalPeerId;

    //本次运行临时生成的候选UUID
    //第一次运行：数据库中没有本机ID，将该候选UUID保存到local_peer_id表
    //后续运行：数据库中已经有本机ID，忽略本次新生成的候选UUID，并把以前保存的id返回到persistentLocalPeerId
    if (!m_peerDatabase.loadOrCreateLocalPeerId(m_chat.localId(), persistentLocalPeerId)) {
        reportError( QStringLiteral("初始化本机永久ID失败：") + m_peerDatabase.lastError());
        return false;
    }

    //须在网络层启动之前，把数据库中的永久ID设置回网络层，
    //如果在start之后设置，UDP广播线程可能已经使用临时UUID，造成同一次运行中出现两个身份
    if (!m_chat.setLocalId(persistentLocalPeerId)) {
        reportError(QStringLiteral("设置网络层本机永久ID失败"));
        return false;
    }

    //程序重启后，上次保留的online=1已经过期
    if (!m_peerDatabase.synchronizePeers({})) {
        reportError(QStringLiteral("重置用户在线状态失败：") + m_peerDatabase.lastError());
        return false;
    }

    //网络服务启动前先读取本地历史用户，使界面可以立即显示以前保存的在线或离线用户
    refreshPeers();

    //网络服务启动前读取数据库中已经持久化的群聊
    refreshGroups();

    //完成本地数据恢复后再启动局域网消息服务
    m_chat.start(normalizedName);
    //通知QML重新读取groupCandidates
    emit groupCandidatesChanged();
    restoreGroupSessions();

    //启动文件传输线程
    m_translateFile.start();

    //所有必要步骤完成后再将控制器标记为可用
    m_ready = true;
    emit readyChanged();

    //初始化成功后，保存本次登录用户名
    QSettings s;
    s.setValue(QStringLiteral("login/userName"), normalizedName);

    return true;
}

//选择私聊对象并加载对应聊天记录
void AppController::selectPeer(const QString &peerId)
{
    //进入私聊前先退出当前群聊，这样界面不会同时保留一份私聊消息和一份群聊消息
    if (!m_currentGroupId.isEmpty()) { clearGroupConversation();}

    //统一去除ID首尾空白
    m_currentPeerId = peerId.trimmed();

    //根据新的peerId重新读取私聊历史记录
    refreshMessages();
}

//退出当前会话，用户列表仍然保留，只清空当前聊天对象和消息缓存
void AppController::clearConversation()
{
    m_currentPeerId.clear();

    //只有消息缓存确实发生变化时才发出信号，避免无效刷新
    if (!m_messages.isEmpty()) {
        m_messages.clear();
        emit messagesChanged();
    }
}

//选择群聊并读取群成员及群聊历史消息
void AppController::selectGroup(const QString &groupId)
{
    const QString normalizedGroupId = groupId.trimmed();

    if (!DatabaseCheck::isValidGroupId(normalizedGroupId)) {
        reportError(QStringLiteral("要选择的群聊ID必须是十位数字"));
        return;
    }

    //空群ID不能对应有效群聊
    if (normalizedGroupId.isEmpty()) {
        reportError(QStringLiteral("要选择的群聊ID为空"));
        return;
    }

    //进入群聊前先退出当前私聊
    if (!m_currentPeerId.isEmpty()) { clearConversation();}

    //记录当前群聊ID，后续刷新成员和消息时都使用这个ID
    m_currentGroupId = normalizedGroupId;

    //成员列表与消息列表必须属于同一个当前群聊
    refreshGroupMembers();
    refreshGroupMessages();
}

//关闭当前群聊
void AppController::clearGroupConversation()
{
    //清除当前群聊选择
    m_currentGroupId.clear();

    //只有数据确实发生变化时才发出信号
    if (!m_groupMembers.isEmpty()) {
        m_groupMembers.clear();
        emit groupMembersChanged();
    }

    if (!m_groupMessages.isEmpty()) {
        m_groupMessages.clear();
        emit groupMessagesChanged();
    }
}

//创建群聊，并同时建立网络和保存本地数据库记录
QString AppController::createGroup(const QString &groupName, const QVariantList &members)
{
    if (!m_ready) {
        reportError(QStringLiteral("程序尚未初始化，无法创建群聊"));
        return {};
    }

    const QString normalizedGroupName = groupName.trimmed();

    if (normalizedGroupName.isEmpty()) {
        reportError(QStringLiteral("群聊名称不能为空"));
        return {};
    }

    if (containsProtocolSeparator(normalizedGroupName)) {
        reportError(QStringLiteral("群聊名称不能包含换行或制表符"));
        return {};
    }

    if (members.size() < 3) {
        reportError(QStringLiteral("群聊成员不能少于三人"));
        return {};
    }

    std::vector<UserInfo> networkMembers;
    networkMembers.reserve(static_cast<std::size_t>(members.size()));

    QVariantList databaseMembers;
    databaseMembers.reserve(members.size());

    QSet<QString> memberIds;
    bool selfIncluded = false;

    const QString localPeerId = m_chat.localId().trimmed();

    const QString localName = m_chat.localName().trimmed();

    const QString localIp = m_chat.localIp().trimmed();

    for (const QVariant &item : members) {
        const QVariantMap sourceMember = item.toMap();

        const bool isSelf = sourceMember.value(QStringLiteral("isSelf")).toBool();

        QString peerId = isSelf ? localPeerId : sourceMember.value(QStringLiteral("peerId")).toString().trimmed();

        QString username = isSelf ? localName : sourceMember.value(QStringLiteral("username")).toString().trimmed();

        QString ip = isSelf ? localIp : sourceMember.value(QStringLiteral("ip")).toString().trimmed();

        const bool online = isSelf || sourceMember.value(QStringLiteral("online")).toBool();

        const QUuid parsedPeerId = QUuid::fromString(peerId);

        if (parsedPeerId.isNull()) {
            reportError(QStringLiteral("群成员peerId无效：%1").arg(username.isEmpty() ? peerId : username));
            return {};
        }

        peerId = parsedPeerId.toString(QUuid::WithoutBraces);

        if (username.isEmpty()) {
            reportError(QStringLiteral("群成员用户名不能为空"));
            return {};
        }

        if (containsProtocolSeparator(username)) {
            reportError(QStringLiteral("群成员名称不能包含换行或制表符：%1").arg(username));
            return {};
        }

        if (!online) {
            reportError(QStringLiteral("用户“%1”当前不在线，无法邀请进入群聊").arg(username));
            return {};
        }

        if (ip.isEmpty()) {
            reportError(QStringLiteral("群成员“%1”的IP地址为空").arg(username));
            return {};
        }

        if (memberIds.contains(peerId)) {
            reportError(QStringLiteral("群成员列表中存在重复用户：%1").arg(username));
            return {};
        }

        memberIds.insert(peerId);

        selfIncluded = selfIncluded || peerId == localPeerId;

        //保存成员当前网络端点。
        if (!m_peerDatabase.upsertPeer(peerId, username, ip, online)) {
            reportError(QStringLiteral("保存群成员信息失败：") + m_peerDatabase.lastError());
            return {};
        }

        UserInfo networkMember;
        networkMember.id = peerId.toStdString();
        networkMember.name = username.toStdString();
        networkMember.ip = ip.toStdString();
        networkMember.lastSeen = std::chrono::steady_clock::now();

        networkMembers.push_back(std::move(networkMember));

        QVariantMap databaseMember;
        databaseMember.insert(QStringLiteral("peerId"), peerId);
        databaseMember.insert(QStringLiteral("username"), username);

        databaseMembers.append(databaseMember);
    }

    if (!selfIncluded) {
        reportError(QStringLiteral("群成员列表中没有包含本机用户"));
        return {};
    }

    //网络层生成候选值
    QString groupId;
    for (int attempt = 0; attempt < 100; attempt++) {
        const QString candidate = m_groupChat.generateGroupId().trimmed();

        if (!DatabaseCheck::isValidGroupId(candidate)) {
            continue;
        }

        bool exists = false;
        //返回false表示SQL查询失败
        if (!m_groupChatDatabase.groupExists(candidate, exists)) {
            reportError(QStringLiteral("检查群聊ID失败：") + m_groupChatDatabase.lastError());
            return {};
        }

        if (!exists) {
            groupId = candidate;
            break;
        }
    }

    if (groupId.isEmpty()) {
        reportError(QStringLiteral("无法生成未占用的十位数字群ID"));
        return {};
    }

    if (!m_groupChatDatabase.createGroup(groupId, normalizedGroupName, localPeerId, databaseMembers)) {
        reportError(QStringLiteral("保存群聊失败：")+ m_groupChatDatabase.lastError());
        return {};
    }

    //数据库成功后才开始发送UDP邀请和建立TCP连接
    if (!m_groupChat.createGroup(groupId, normalizedGroupName,networkMembers)) {
        //创建者本机网络会话建立失败，撤销本次新建群记录
        const bool deleted = m_groupChatDatabase.deleteGroup(groupId);

        const QString rollbackError = m_groupChatDatabase.lastError();

        reportError(deleted ? QStringLiteral("网络层创建群聊失败，已撤销本地群记录")
                            : QStringLiteral("网络层创建群聊失败，且撤销群记录失败：") + rollbackError);
        return {};
    }

    refreshGroups();
    selectGroup(groupId);

    return groupId;
}

//向指定群聊发送一条文本消息
bool AppController::sendGroupMessage(const QString &groupId, const QString &content)
{
    //数据库和网络服务没有初始化时，不能执行群聊发送
    if (!m_ready) {
        reportError(QStringLiteral("程序尚未初始化，无法发送群消息"));
        return false;
    }

    //统一去除首尾空白
    const QString normalizedGroupId = groupId.trimmed();

    const QString normalizedContent = content.trimmed();


    if (!DatabaseCheck::isValidGroupId(normalizedGroupId)) {
        reportError(QStringLiteral("群聊ID必须是十位数字"));
        return false;
    }

    if (normalizedGroupId.isEmpty()) {
        reportError(QStringLiteral("群聊ID不能为空"));
        return false;
    }

    //不允许保存或发送只有空格的消息
    if (normalizedContent.isEmpty()) {
        reportError(QStringLiteral("群消息内容不能为空"));
        return false;
    }

    //把发送任务交给群聊网络层
    const bool submitted = m_groupChat.sendMsgToGroup(normalizedGroupId.toStdString(), normalizedContent.toStdString());

    if (!submitted) {
        reportError(QStringLiteral("群消息发送失败：当前群聊网络会话不存在"));
        return false;
    }

    //这里只表示消息已经提交给网络层
    return true;
}

void AppController::handleGroupInviteReceived(const QString &groupId,
                                              const QString &groupName,
                                              const QString &inviterId,
                                              const QString &inviterName,
                                              const QString &inviterIp,
                                              const QStringList &memberRecords)
{
    const QString normalizedGroupId = groupId.trimmed();
    const QString normalizedGroupName = groupName.trimmed();
    const QString normalizedInviterName = inviterName.trimmed();
    const QString normalizedInviterIp = inviterIp.trimmed();

    const QUuid creatorUuid = QUuid::fromString(inviterId.trimmed());
    const QString normalizedCreatorId = creatorUuid.isNull() ? QString() : creatorUuid.toString(QUuid::WithoutBraces);

    if (!DatabaseCheck::isValidGroupId(normalizedGroupId)) {
        reportError(QStringLiteral("收到的群邀请包含无效群ID"));
        return;
    }

    if (normalizedGroupName.isEmpty() || containsProtocolSeparator(normalizedGroupName)) {
        reportError(QStringLiteral("收到的群邀请包含无效群名称"));
        return;
    }

    if (normalizedCreatorId.isEmpty() || normalizedInviterName.isEmpty() || normalizedInviterIp.isEmpty()) {
        reportError(QStringLiteral("收到的群邀请缺少创建者信息"));
        return;
    }

    if (memberRecords.size() < 3) {
        reportError(QStringLiteral("收到的群邀请成员数少于三人"));
        return;
    }

    const QString localId = m_chat.localId().trimmed();
    const QString localName = m_chat.localName().trimmed();
    const QString localIp = m_chat.localIp().trimmed();

    QSet<QString> memberIds;

    QVariantList databaseMembers;
    databaseMembers.reserve(memberRecords.size());

    std::vector<UserInfo> networkMembers;
    networkMembers.reserve(static_cast<std::size_t>(memberRecords.size()));

    bool selfIncluded = false;
    bool inviterIncluded = false;

    for (const QString &record : memberRecords) {
        const QStringList fields = record.split(QLatin1Char('\t'), Qt::KeepEmptyParts);

        if (fields.size() != 3) {
            reportError(QStringLiteral("收到的群邀请成员记录格式错误"));
            return;
        }

        const QUuid memberUuid = QUuid::fromString(fields[0].trimmed());
        QString memberId = memberUuid.isNull() ? QString() : memberUuid.toString(QUuid::WithoutBraces);
        QString username = fields[1].trimmed();
        QString ip = fields[2].trimmed();

        //本机资料不能使用邀请报文中的旧值
        if (memberId == localId) {
            username = localName;
            ip = localIp;
            selfIncluded = true;
        }

        //UDP数据报的来源IP比文本字段更可靠
        if (memberId == normalizedCreatorId) {
            username = normalizedInviterName;
            ip = normalizedInviterIp;
            inviterIncluded = true;
        }

        if (memberId.isEmpty() || username.isEmpty() || containsProtocolSeparator(username)) {
            reportError(QStringLiteral("收到的群邀请包含无效成员信息"));
            return;
        }

        if (memberIds.contains(memberId)) {
            reportError(QStringLiteral("收到的群邀请包含重复成员：%1").arg(username));
            return;
        }

        memberIds.insert(memberId);

        if (!ip.isEmpty() && !m_peerDatabase.upsertPeer(memberId, username, ip, true)) {
            reportError(QStringLiteral("保存群邀请成员失败：") + m_peerDatabase.lastError());
            return;
        }

        QVariantMap databaseMember;
        databaseMember.insert(QStringLiteral("peerId"), memberId);
        databaseMember.insert(QStringLiteral("username"), username);
        databaseMembers.append(databaseMember);

        UserInfo networkMember;
        networkMember.id = memberId.toStdString();
        networkMember.name = username.toStdString();
        networkMember.ip = ip.toStdString();
        networkMember.lastSeen = std::chrono::steady_clock::now();

        networkMembers.push_back(std::move(networkMember));
    }

    if (!selfIncluded) {
        reportError(QStringLiteral("收到的群邀请不包含本机用户"));
        return;
    }

    if (!inviterIncluded) {
        reportError(QStringLiteral("收到的群邀请不包含创建者"));
        return;
    }

    //createGroup是幂等事务
    //UDP邀请重复三次也不会生成重复数据
    if (!m_groupChatDatabase.createGroup(normalizedGroupId, normalizedGroupName, normalizedCreatorId, databaseMembers)) {
        reportError(QStringLiteral("保存收到的群邀请失败：") + m_groupChatDatabase.lastError());
        return;
    }

    //先完成数据库保存，再创建GroupSession。
    if (!m_groupChat.restoreGroup(normalizedGroupId, normalizedGroupName, networkMembers)) {
        //不删除数据库记录
        //网络可能只是暂时不可用，后续用户在线同步还会重新恢复
        reportError(QStringLiteral("群邀请已保存，但暂时无法恢复群聊网络会话"));
    }

    refreshGroups();

    if (m_currentGroupId == normalizedGroupId) {
        refreshGroupMembers();
        refreshGroupMessages();
    }
}

void AppController::restoreGroupSessions()
{
    QVariantList savedGroups;

    if (!m_groupChatDatabase.loadGroups(savedGroups)) {
        reportError(QStringLiteral("恢复群聊时读取群列表失败：") + m_groupChatDatabase.lastError());
        return;
    }

    const QString localId = m_chat.localId().trimmed();
    const QString localName = m_chat.localName().trimmed();
    const QString localIp = m_chat.localIp().trimmed();

    for (const QVariant &groupItem : savedGroups) {
        const QVariantMap group = groupItem.toMap();

        const QString groupId = group.value(QStringLiteral("groupId")).toString().trimmed();
        const QString groupName = group.value(QStringLiteral("groupName")).toString().trimmed();

        if (!DatabaseCheck::isValidGroupId(groupId) || groupName.isEmpty()) {
            continue;
        }

        QVariantList savedMembers;

        if (!m_groupChatDatabase.loadGroupMembers(groupId, savedMembers)) {
            reportError(QStringLiteral("恢复群聊时读取群成员失败：") + m_groupChatDatabase.lastError());
            continue;
        }

        std::vector<UserInfo> networkMembers;
        networkMembers.reserve(static_cast<std::size_t>(savedMembers.size()));

        for (const QVariant &memberItem : savedMembers) {
            const QVariantMap member = memberItem.toMap();

            QString memberId = member.value(QStringLiteral("peerId")).toString().trimmed();
            QString username = member.value(QStringLiteral("username")).toString().trimmed();
            QString ip = member.value(QStringLiteral("ip")).toString().trimmed();

            const QUuid memberUuid = QUuid::fromString(memberId);

            if (memberUuid.isNull() || username.isEmpty()) {
                continue;
            }

            memberId = memberUuid.toString(QUuid::WithoutBraces);

            if (memberId == localId) {
                username = localName;
                ip = localIp;
            }

            UserInfo networkMember;
            networkMember.id = memberId.toStdString();
            networkMember.name = username.toStdString();
            networkMember.ip = ip.toStdString();
            networkMember.lastSeen = std::chrono::steady_clock::now();

            networkMembers.push_back(std::move(networkMember));
        }

        if (networkMembers.size() < 3) {
            continue;
        }

        m_groupChat.restoreGroup(groupId, groupName, networkMembers);
    }
}

//删除本地用户及其聊天记录，并同步清理控制器和QML状态
bool AppController::deletePeer(const QString &peerId)
{
    //未初始化时数据库不可用，禁止执行删除操作
    if (!m_ready) {
        reportError(QStringLiteral("程序尚未初始化"));
        return false;
    }

    //使用去除首尾空白后的 peerId 进行统一处理
    const QString normalizedPeerId = peerId.trimmed();

    if (normalizedPeerId.isEmpty()) {
        reportError(QStringLiteral("要删除的用户ID为空"));
        return false;
    }

    //调用数据库层删除用户,对应聊天记录由数据库外键级联删除
    if (!m_peerDatabase.deletePeer(normalizedPeerId)) {
        reportError(QStringLiteral("删除用户失败：") + m_peerDatabase.lastError());
        return false;
    }

    //如果删除的是当前聊天对象，同时关闭当前会话
    if (m_currentPeerId == normalizedPeerId) {
        clearConversation();
    }

    //删除完成后重新读取用户列表，更新QML中的左侧列表
    refreshPeers();

    //单独通知QML删除成功，使Window.qml清理当前用户名称和IP
    emit peerDeleted(normalizedPeerId);

    return true;
}

QString AppController::localIp()
{
    return m_chat.localIp();
}

//校验聊天对象和消息内容，通过网络层发送消息，并将本机发送记录保存到数据库
void AppController::sendMessage(const QString &peerId,
                                const QString &username,
                                const QString &ip,
                                const QString &content)
{
    if (!m_ready) {
        reportError(QStringLiteral("程序尚未初始化"));
        return;
    }

    //对QML传入的字符串统一去除首尾空白
    const QString normalizedPeerId = peerId.trimmed();
    const QString normalizedName = username.trimmed();
    const QString normalizedIp = ip.trimmed();
    const QString normalizedContent = content.trimmed();

    if (normalizedPeerId.isEmpty() || normalizedName.isEmpty() || normalizedIp.isEmpty()) {
        reportError(QStringLiteral("聊天对象信息不完整"));
        return;
    }

    if (normalizedContent.isEmpty()) {
        reportError(QStringLiteral("消息内容不能为空"));
        return;
    }

    //messages.peer_id是外键,保存消息前先确保该用户已经存在于peers表中
    if (!m_peerDatabase.upsertPeer(normalizedPeerId, normalizedName, normalizedIp, true)) {
        reportError(QStringLiteral("更新用户信息失败：") + m_peerDatabase.lastError());
        return;
    }

    //如果目标是自己，直接本地保存，不经过网络
    if (normalizedPeerId == m_chat.localId()) {
        if (!m_privateChatDatabase.saveMessage(normalizedPeerId, true, normalizedContent)) {
            reportError(QStringLiteral("保存消息失败：") + m_privateChatDatabase.lastError());
            return;
        }
        if (m_currentPeerId == normalizedPeerId) {
            refreshMessages();
        }
        return;
    }

    //当前网络接口是异步发送，调用返回表示消息已交给发送线程，暂时不代表对方一定已经收到。
    m_chat.sendMessageToUser(normalizedPeerId, normalizedContent);

    //网络发送请求提交后保存本地历史记录
    if (!m_privateChatDatabase.saveMessage(normalizedPeerId, true, normalizedContent)) {
        reportError(QStringLiteral("保存发送消息失败：") + m_privateChatDatabase.lastError());
        return;
    }

    //只有消息属于当前打开的会话时才重新读取聊天记录
    if (m_currentPeerId == normalizedPeerId) {
        refreshMessages();
    }
}

void AppController::sendFile(const QString &peerId,
                             const QString &username,
                             const QString &ip,
                             const QUrl &fileUrl)
{
    if (!m_ready) {
        reportError(QStringLiteral("程序尚未初始化"));
        return;
    }

    const QString normalizedPeerId = peerId.trimmed();
    const QString normalizedName = username.trimmed();
    const QString normalizedIp = ip.trimmed();

    if (normalizedPeerId.isEmpty() || normalizedName.isEmpty() || normalizedIp.isEmpty()) {
        reportError(QStringLiteral("文件发送失败：聊天对象信息不完整"));
        return;
    }

    if (!fileUrl.isLocalFile()) {
        reportError(QStringLiteral("文件发送失败：当前只支持本地文件"));
        return;
    }

    const QString localFilePath = fileUrl.toLocalFile();
    const QFileInfo fileInfo(localFilePath);

    if (!fileInfo.exists() || !fileInfo.isFile()) {
        reportError(QStringLiteral("文件发送失败：文件不存在或不是普通文件"));
        return;
    }

    if (!fileInfo.isReadable()) {
        reportError(QStringLiteral("文件发送失败：文件不可读"));
        return;
    }

    // 确保数据库里存在这个聊天对象。
    if (!m_peerDatabase.upsertPeer(normalizedPeerId, normalizedName, normalizedIp, true)) {
        reportError(QStringLiteral("更新文件接收方信息失败：") + m_peerDatabase.lastError());
        return;
    }

    // 记录一条本地文件发送消息，方便聊天窗口立即显示。
    const QString displayMessage =
        QStringLiteral("[发送文件] %1").arg(fileInfo.fileName());

    if (!m_privateChatDatabase.saveMessage(normalizedPeerId, true, displayMessage)) {
        reportError(QStringLiteral("保存文件发送记录失败：") + m_privateChatDatabase.lastError());
        return;
    }

    if (m_currentPeerId == normalizedPeerId) {
        refreshMessages();
    }

    m_translateFile.sendFile(normalizedIp, localFilePath);

    qInfo() << "开始发送文件:"
            << "peerId =" << normalizedPeerId
            << "username =" << normalizedName
            << "ip =" << normalizedIp
            << "file =" << localFilePath
            << "size =" << fileInfo.size();
}

//接受该IP发送的文件
void AppController::acceptFile(const QString &ip,
                               const QUrl &saveUrl)
{
    if (!m_ready) {
        reportError(QStringLiteral("程序尚未初始化"));
        return;
    }

    const QString normalizedIp = ip.trimmed();

    if (normalizedIp.isEmpty()) {
        reportError(QStringLiteral("接收文件失败：发送方 IP 为空"));
        return;
    }

    if (!saveUrl.isLocalFile()) {
        reportError(QStringLiteral("接收文件失败：保存路径不是本地路径"));
        return;
    }

    const QString savePath = saveUrl.toLocalFile();

    if (savePath.trimmed().isEmpty()) {
        reportError(QStringLiteral("接收文件失败：保存路径为空"));
        return;
    }

    //通知文件传输后端：接受该IP发来的待处理文件请求，并把文件保存到savePath。
    m_translateFile.acceptFile(normalizedIp, savePath);

    qInfo() << "接受文件请求:"
            << "fromIp =" << normalizedIp
            << "savePath =" << savePath;
}

//拒绝该IP发送的文件
void AppController::rejectFile(const QString &ip)
{
    if (!m_ready) {
        reportError(QStringLiteral("程序尚未初始化"));
        return;
    }

    const QString normalizedIp = ip.trimmed();

    if (normalizedIp.isEmpty()) {
        reportError(QStringLiteral("拒绝文件失败：发送方 IP 为空"));
        return;
    }

    //通知文件传输后端：拒绝该IP发来的待处理文件请求。
    m_translateFile.rejectFile(normalizedIp);

    qInfo() << "拒绝文件请求:"
            << "fromIp =" << normalizedIp;
}

//将Chat的网络用户数据转换成数据库层需要的数据结构，然后调用DatabaseManager完成事务同步
void AppController::synchronizeOnlineUsers()
{
    //使用数据库层约定的字段名保存用户数据
    QVariantList normalizedPeers;

    //onlineUsers的原始字段由网络层提供，主要包含name和ip
    const QVariantList onlineUsers = m_chat.onlineUsers();

    for (const QVariant &item : onlineUsers) {
        //将每一个网络用户转换为QVariantMap
        const QVariantMap networkUser = item.toMap();

        const QString peerId = networkUser.value(QStringLiteral("id")).toString().trimmed();

        //优先将数据库的名字为主，不然旧的广播会覆盖数据库，具体效果可以删掉下面这个if语句查看
        QString name = networkUser.value(QStringLiteral("name")).toString().trimmed();

        if (peerId == m_chat.localId()) {
            const QString savedName = savedUserName().trimmed();

            if (!savedName.isEmpty()) {
                name = savedName;
            }
        }

        const QString ip = networkUser.value(QStringLiteral("ip")).toString().trimmed();

        //name,peerId,ip缺失的记录不能写入数据库
        if (name.isEmpty() || ip.isEmpty()) {continue;}

        QVariantMap peer;

        peer.insert(QStringLiteral("peerId"), peerId);
        peer.insert(QStringLiteral("username"), name);
        peer.insert(QStringLiteral("ip"), ip);
        peer.insert(QStringLiteral("online"), true);

        normalizedPeers.append(peer);
    }

    //数据库同步失败时保留旧的QML用户列表，并报告错误
    if (!m_peerDatabase.synchronizePeers(normalizedPeers)) {
        reportError(QStringLiteral("同步在线用户失败：") + m_peerDatabase.lastError());
        return;
    }

    //同步成功后重新读取排序后的数据库用户列表
    refreshPeers();

    //重新恢复群会话，使用peerId匹配最新IP
    restoreGroupSessions();

    //在线状态变化后需要同步刷新右侧成员栏
    refreshGroupMembers();

}

//处理网络层收到的聊天消息，先保证发送者存在，再保存消息，最后刷新用户列表和当前会话
void AppController::handleMessageReceived( const QString &fromId,
                                           const QString &fromName,
                                           const QString &fromIp,
                                           const QString &message)
{
    //忽略来源IP为本机的消息，避免本机消息被重复保存和显示。
    if (fromIp == m_chat.localIp()) {
        return;
    }

    //对网络层传入的数据进行去除字符串首尾空白
    const QString normalizedPeerId = fromId.trimmed();
    const QString normalizedName = fromName.trimmed();
    const QString normalizedIp = fromIp.trimmed();
    const QString normalizedMessage = message.trimmed();

    //按照ID判断消息是否来自本机
    if (!normalizedPeerId.isEmpty()&& normalizedPeerId == m_chat.localId()) { return;}

    //ip和消息正文是保存消息所必需的数据
    if (normalizedPeerId.isEmpty() || normalizedIp.isEmpty() || normalizedMessage.isEmpty()) { return;}

    //对方没有提供有效用户名时，使用ip作为界面显示名称
    const QString displayName = normalizedName.isEmpty() ? normalizedIp : normalizedName;

    //先保证发送者存在于peers表
    if (!m_peerDatabase.upsertPeer(normalizedPeerId, displayName, normalizedIp, true)) {
        reportError(QStringLiteral("保存消息发送者失败：") + m_peerDatabase.lastError());
        return;
    }

    if (!m_privateChatDatabase.saveMessage(normalizedPeerId, false, normalizedMessage)) {
        reportError(QStringLiteral("保存接收消息失败：") + m_privateChatDatabase.lastError());
        return;
    }

    //收到新消息后，发送者可能是新用户，因此刷新用户列表
    refreshPeers();

    //当消息属于当前会话时刷新聊天记录
    if (m_currentPeerId == normalizedPeerId) { refreshMessages();}
}

//统一处理网络层报告的群聊消息
void AppController::handleGroupMessageReceived(const QString &groupId,
                                               const QString &fromId,
                                               const QString &fromName,
                                               const QString &content)
{
    //对网络层传入的数据做统一规范化处理
    const QString normalizedGroupId = groupId.trimmed();
    const QString normalizedFromId = fromId.trimmed();
    const QString normalizedFromName = fromName.trimmed();
    const QString normalizedContent = content.trimmed();

    if (normalizedGroupId.isEmpty() || normalizedFromId.isEmpty() || normalizedContent.isEmpty()) {
        reportError(QStringLiteral("收到的群消息数据不完整"));
        return;
    }

    //GroupChatDatabase会根据senderId和local_peer_id自动计算消息来源
    if (!m_groupChatDatabase.saveGroupMessage(normalizedGroupId, normalizedFromId, normalizedFromName, normalizedContent)) {
        reportError(QStringLiteral("保存群消息失败：") + m_groupChatDatabase.lastError());

        return;
    }

    //保存群消息时会更新chat_groups.updated_at
    refreshGroups();

    //只有当前正在查看这个群聊时，才重新读取该群的消息记录
    if (m_currentGroupId == normalizedGroupId) {
        refreshGroupMessages();
    }
}

//从数据库重新加载用户列表，只有新数据与缓存不同时才更新属性并通知 QML
void AppController::refreshPeers()
{
    QVariantList loadedPeers;

    //loadPeers返回false表示SQL失败，loadedPeers为空不一定是错误
    if (!m_peerDatabase.loadPeers(loadedPeers)) {
        reportError(QStringLiteral("读取数据库用户列表失败：") + m_peerDatabase.lastError());
        return;
    }

    if (loadedPeers == m_peers) {
        return;
    }

    m_peers = loadedPeers;

    emit peersChanged();
    emit groupCandidatesChanged();
}

//从数据库重新加载当前会话消息，没有选择用户时使用空列表清理聊天界面
void AppController::refreshMessages()
{
    QVariantList loadedMessages;
    if (!m_currentPeerId.isEmpty()) {
        if (!m_privateChatDatabase.loadMessages(m_currentPeerId, loadedMessages)) {
            reportError(QStringLiteral("读取私聊记录失败：") + m_privateChatDatabase.lastError());
            return;
        }
    }

    if (loadedMessages == m_messages) {
        return;
    }
    m_messages = loadedMessages;
    emit messagesChanged();
}
//从数据库读取全部群聊
void AppController::refreshGroups()
{
    QVariantList loadedGroups;
    if (!m_groupChatDatabase.loadGroups(loadedGroups)) {
        reportError(QStringLiteral("读取群聊列表失败：") + m_groupChatDatabase.lastError());

        return;
    }

    if (loadedGroups == m_groups) {
        return;
    }

    m_groups = loadedGroups;
    emit groupsChanged();
}

//读取当前选中群聊的成员列表
void AppController::refreshGroupMembers()
{
    QVariantList loadedMembers;

    if (!m_currentGroupId.isEmpty()) {
        if (!m_groupChatDatabase.loadGroupMembers(m_currentGroupId, loadedMembers)) {
            reportError(QStringLiteral("读取群成员失败：") + m_groupChatDatabase.lastError());
            return;
        }
    }

    if (loadedMembers == m_groupMembers) {
        return;
    }

    m_groupMembers = loadedMembers;
    emit groupMembersChanged();
}

//读取当前选中群聊的历史消息
void AppController::refreshGroupMessages()
{
    QVariantList loadedMessages;

    if (!m_currentGroupId.isEmpty()) {
        if (!m_groupChatDatabase.loadGroupMessages(m_currentGroupId, loadedMessages)) {
            reportError(QStringLiteral("读取群聊历史消息失败：") + m_groupChatDatabase.lastError());

            return;
        }
    }

    if (loadedMessages == m_groupMessages) {
        return;
    }

    m_groupMessages = loadedMessages;
    emit groupMessagesChanged();
}

//统一保存错误并发出属性变化和业务失败信号
void AppController::reportError(const QString &message)
{
    m_lastError = message;

    emit lastErrorChanged();
    emit operationFailed(message);
}

//读取上次保存的用户名
QString AppController::savedUserName() const
{
    QSettings s;
    return s.value(QStringLiteral("login/userName")).toString().trimmed();
}

//清除上次登录用户名
void AppController::clearSavedUserName()
{
    QSettings s;
    s.remove(QStringLiteral("login/userName"));
}

bool AppController::updateMyName(const QString &newName)
{
    if (!m_ready) {
        reportError(QStringLiteral("程序尚未初始化！"));
        return false;
    }

    const QString name = newName.trimmed();

    if (name.isEmpty()) {
        reportError(QStringLiteral("用户名不能为空"));
        return false;
    }

    const QString localId = m_chat.localId().trimmed();
    const QString localIp = m_chat.localIp().trimmed();
    
    if (localId.isEmpty() || localIp.isEmpty())
    {
        reportError(QStringLiteral("信息不完整，用户名修改失败"));
        return false;
    }

    //更新数据库本机用户信息
    if (!m_peerDatabase.upsertPeer(localId, name, localIp, true)) {
        reportError(QStringLiteral("更新本机用户名失败：") + m_peerDatabase.lastError());
        return false;
    }

    //更新自动登录用户名
    QSettings s;
    s.setValue(QStringLiteral("login/userName"), name);

    //更新前端用户列表
    refreshPeers();

    return true;
}