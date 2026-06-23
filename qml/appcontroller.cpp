//         *连接PrivateChat在线用户变化和消息接收信号。
//          初始化 SQLite 数据库和网络服务。
//          将网络用户同步到数据库。
//          发送消息后保存本地聊天记录。
//          接收消息后保存对方聊天记录。
//          从数据库刷新用户列表和历史消息。
//
//     [v0.1.3] JiangFan    2026-06-14
//          *增加文件发送功能(待完善)
//     [v0.1.4] ZhouChengWei     2026-06-14 21:27:37
//         * 处理了因为给自己发送消息而接收导致显示2次的问题
//     [v0.1.5] HeZhiyuan    2026-06-18 19:23:35
//         * 在初始化数据库后读取或创建本机永久peerId
//           在PrivateChat网络线程启动前，将持久化peerId设置到网络层
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

#include "appcontroller.h"

#include <QVariantMap>
#include <QDebug>
#include <QFileInfo>
#include <QSettings>


AppController::AppController(QObject *parent)
    : QObject(parent)
{
    //在线用户列表发生变化,AppController执行数据库同步
    connect(&m_privateChat, &PrivateChat::onlineUsersChanged, this, &AppController::synchronizeOnlineUsers);

    //收到聊天消息
    connect(&m_privateChat, &PrivateChat::messageReceived, this, &AppController::handleMessageReceived);

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

QVariantList AppController::messages() const
{
    return m_messages;
}

QString AppController::lastError() const
{
    return m_lastError;
}

bool AppController::ready() const
{
    return m_ready;
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

    if (!m_database.open()) {
        reportError(QStringLiteral("数据库打开失败：") + m_database.lastError());
        return false;
    }

    //数据库连接成功后创建必要的数据表和索引
    if (!m_database.initSchema()) {
        reportError(QStringLiteral("数据库初始化失败：") + m_database.lastError());
        return false;
    }


    //数据库存储的peerId。
    QString persistentLocalPeerId;

    //本次运行临时生成的候选UUID
    //第一次运行：数据库中没有本机ID，将该候选UUID保存到local_peer_id表
    //后续运行：数据库中已经有本机ID，忽略本次新生成的候选UUID，并把以前保存的id返回到persistentLocalPeerId
    if (!m_database.loadOrCreateLocalPeerId(m_privateChat.localId(), persistentLocalPeerId)) {
        reportError( QStringLiteral("初始化本机永久ID失败：") + m_database.lastError());
        return false;
    }

    //须在网络层启动之前，把数据库中的永久ID设置回网络层，
    //如果在start之后设置，UDP广播线程可能已经使用临时UUID，造成同一次运行中出现两个身份
    if (!m_privateChat.setLocalId(persistentLocalPeerId)) {
        reportError(QStringLiteral("设置网络层本机永久ID失败"));
        return false;
    }

    //网络服务启动前先读取本地历史用户，使界面可以立即显示离线用户
    refreshPeers();

    //启动局域网消息服务
    m_privateChat.start(normalizedName);

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

//更新当前聊天用户，并从数据库加载其历史消息
void AppController::selectPeer(const QString &peerId)
{
    m_currentPeerId = peerId.trimmed();
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
    if (!m_database.deletePeer(normalizedPeerId)) {
        reportError(QStringLiteral("删除用户失败：") + m_database.lastError());
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
    return m_privateChat.localIp();
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
    if (!m_database.upsertPeer(normalizedPeerId, normalizedName, normalizedIp, true)) {
        reportError(QStringLiteral("更新用户信息失败：") + m_database.lastError());
        return;
    }

    //如果目标是自己，直接本地保存，不经过网络
    if (normalizedPeerId == m_privateChat.localId()) {
        if (!m_database.saveMessage(normalizedPeerId, true, normalizedContent)) {
            reportError(QStringLiteral("保存消息失败：") + m_database.lastError());
            return;
        }
        if (m_currentPeerId == normalizedPeerId) {
            refreshMessages();
        }
        return;
    }

    //当前网络接口是异步发送，调用返回表示消息已交给发送线程，暂时不代表对方一定已经收到。
    m_privateChat.sendMessageToUser(normalizedPeerId, normalizedContent);

    //网络发送请求提交后保存本地历史记录
    if (!m_database.saveMessage(normalizedPeerId, true, normalizedContent)) {
        reportError(QStringLiteral("保存发送消息失败：") + m_database.lastError());
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
    if (!m_database.upsertPeer(normalizedPeerId, normalizedName, normalizedIp, true)) {
        reportError(QStringLiteral("更新文件接收方信息失败：") + m_database.lastError());
        return;
    }

    // 记录一条本地文件发送消息，方便聊天窗口立即显示。
    const QString displayMessage =
        QStringLiteral("[发送文件] %1").arg(fileInfo.fileName());

    if (!m_database.saveMessage(normalizedPeerId, true, displayMessage)) {
        reportError(QStringLiteral("保存文件发送记录失败：") + m_database.lastError());
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

//将PrivateChat的网络用户数据转换成数据库层需要的数据结构，然后调用DatabaseManager完成事务同步
void AppController::synchronizeOnlineUsers()
{
    //使用数据库层约定的字段名保存用户数据
    QVariantList normalizedPeers;

    //onlineUsers的原始字段由网络层提供，主要包含name和ip
    const QVariantList onlineUsers = m_privateChat.onlineUsers();

    for (const QVariant &item : onlineUsers) {
        //将每一个网络用户转换为QVariantMap
        const QVariantMap networkUser = item.toMap();

        const QString peerId = networkUser.value(QStringLiteral("id")).toString().trimmed();

        const QString name = networkUser.value(QStringLiteral("name")).toString().trimmed();

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
    if (!m_database.synchronizePeers(normalizedPeers)) {
        reportError(QStringLiteral("同步在线用户失败：") + m_database.lastError());
        return;
    }

    //同步成功后重新读取排序后的数据库用户列表
    refreshPeers();
}

//处理网络层收到的聊天消息，先保证发送者存在，再保存消息，最后刷新用户列表和当前会话
void AppController::handleMessageReceived( const QString &fromId,
                                           const QString &fromName,
                                           const QString &fromIp,
                                           const QString &message)
{
    //忽略来源IP为本机的消息，避免本机消息被重复保存和显示。
    if (fromIp == m_privateChat.localIp()) {
        return;
    }

    //对网络层传入的数据进行去除字符串首尾空白
    const QString normalizedPeerId = fromId.trimmed();
    const QString normalizedName = fromName.trimmed();
    const QString normalizedIp = fromIp.trimmed();
    const QString normalizedMessage = message.trimmed();

    //按照ID判断消息是否来自本机
    if (!normalizedPeerId.isEmpty()&& normalizedPeerId == m_privateChat.localId()) { return;}

    //ip和消息正文是保存消息所必需的数据
    if (normalizedPeerId.isEmpty() || normalizedIp.isEmpty() || normalizedMessage.isEmpty()) { return;}

    //对方没有提供有效用户名时，使用ip作为界面显示名称
    const QString displayName = normalizedName.isEmpty() ? normalizedIp : normalizedName;

    //先保证发送者存在于peers表
    if (!m_database.upsertPeer(normalizedPeerId, displayName, normalizedIp, true)) {
        reportError(QStringLiteral("保存消息发送者失败：") + m_database.lastError());
        return;
    }

    if (!m_database.saveMessage(normalizedPeerId, false, normalizedMessage)) {
        reportError(QStringLiteral("保存接收消息失败：") + m_database.lastError());
        return;
    }

    //收到新消息后，发送者可能是新用户，因此刷新用户列表
    refreshPeers();

    //当消息属于当前会话时刷新聊天记录
    if (m_currentPeerId == normalizedPeerId) { refreshMessages();}
}

//从数据库重新加载用户列表，只有新数据与缓存不同时才更新属性并通知 QML
void AppController::refreshPeers()
{
    const QVariantList loadedPeers = m_database.loadPeers();

    if (loadedPeers == m_peers) { return;}

    m_peers = loadedPeers;
    emit peersChanged();
}

//从数据库重新加载当前会话消息，没有选择用户时使用空列表清理聊天界面
void AppController::refreshMessages()
{
    QVariantList loadedMessages;
    if (!m_currentPeerId.isEmpty()) { loadedMessages = m_database.loadMessages(m_currentPeerId);}

    if (loadedMessages == m_messages) { return;}

    m_messages = loadedMessages;
    emit messagesChanged();
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