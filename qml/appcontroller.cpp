//     [v0.1.2] HeZhiyuan    2026-06-13 14:58:31
//         *连接PrivateChat在线用户变化和消息接收信号。
//          初始化 SQLite 数据库和网络服务。
//          将网络用户同步到数据库。
//          发送消息后保存本地聊天记录。
//          接收消息后保存对方聊天记录。
//          从数据库刷新用户列表和历史消息。
//
//     [v0.1.3] JiangFan    2026-06-14
//          *增加文件发送功能(待完善)
//     [v0.1.3] ZhouChengWei     2026-06-14 21:27:37
//         * 处理了因为给自己发送消息而接收导致显示2次的问题

#include "appcontroller.h"

#include <QVariantMap>
#include <QDebug>
#include <QFileInfo>


AppController::AppController(QObject *parent)
    : QObject(parent)
{
    connect(
        &m_privateChat,
        &PrivateChat::onlineUsersChanged,
        this,
        &AppController::synchronizeOnlineUsers);

    connect(
        &m_privateChat,
        &PrivateChat::messageReceived,
        this,
        &AppController::handleMessageReceived);

    //收到文件请求
    connect(
        &m_translateFile,
        &TranslateFile::fileRequestReceived,
        this,
        &AppController::fileRequestReceived);

    //文件传输进度条
    connect(
        &m_translateFile,
        &TranslateFile::fileTransferProgress,
        this,
        &AppController::fileTransferProgress);

    //文件传输完成
    connect(
        &m_translateFile,
        &TranslateFile::fileTransferFinished,
        this,
        &AppController::fileTransferFinished);

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

bool AppController::initialize(const QString &userName)
{
    if (m_ready) {
        return true;
    }

    const QString normalizedName = userName.trimmed();

    if (normalizedName.isEmpty()) {
        reportError(QStringLiteral("用户名不能为空"));
        return false;
    }

    if (!m_database.open()) {
        reportError(
            QStringLiteral("数据库打开失败：")
            + m_database.lastError());
        return false;
    }

    if (!m_database.initSchema()) {
        reportError(
            QStringLiteral("数据库初始化失败：")
            + m_database.lastError());
        return false;
    }

    refreshPeers();

    //启动局域网消息服务
    m_privateChat.start(normalizedName);

    //启动文件传输线程
    m_translateFile.start();

    m_ready = true;
    emit readyChanged();

    return true;
}

void AppController::selectPeer(const QString &peerId)
{
    m_currentPeerId = peerId.trimmed();
    refreshMessages();
}

void AppController::clearConversation()
{
    m_currentPeerId.clear();

    if (!m_messages.isEmpty()) {
        m_messages.clear();
        emit messagesChanged();
    }
}

bool AppController::deletePeer(const QString &peerId)
{
    if (!m_ready) {
        reportError(QStringLiteral("程序尚未初始化"));
        return false;
    }

    const QString normalizedPeerId = peerId.trimmed();

    if (normalizedPeerId.isEmpty()) {
        reportError(QStringLiteral("要删除的用户ID为空"));
        return false;
    }

    //删除数据库记录。
    if (!m_database.deletePeer(normalizedPeerId)) {
        reportError(
            QStringLiteral("删除用户失败：")
            + m_database.lastError());
        return false;
    }

    //如果删除的是当前聊天对象，同时清空控制器中的会话状态。
    if (m_currentPeerId == normalizedPeerId) {
        clearConversation();
    }

    //重新从数据库读取用户列表。
    refreshPeers();

    //通知 QML 清理当前选中状态。
    emit peerDeleted(normalizedPeerId);

    return true;
}

void AppController::sendMessage(const QString &peerId,
                                const QString &username,
                                const QString &ip,
                                const QString &content)
{
    if (!m_ready) {
        reportError(QStringLiteral("程序尚未初始化"));
        return;
    }

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

    //确保外键对应的 peer 已经存在。
    if (!m_database.upsertPeer(normalizedPeerId, normalizedName, normalizedIp, true)) {
        reportError(QStringLiteral("更新用户信息失败：") + m_database.lastError());
        return;
    }

    //如果目标是自己，直接本地保存，不经过网络
    if (normalizedIp == m_privateChat.localIp()) {
        if (!m_database.saveMessage(normalizedPeerId, true, normalizedContent)) {
            reportError(QStringLiteral("保存消息失败：") + m_database.lastError());
            return;
        }
        if (m_currentPeerId == normalizedPeerId) {
            refreshMessages();
        }
        return;
    }

    //当前网络接口是异步发送，调用返回表示消息已交给发送线程，
    //暂时不代表对方一定已经收到。
    m_privateChat.sendMessageToUser(normalizedIp, normalizedContent);

    if (!m_database.saveMessage(normalizedPeerId, true, normalizedContent)) {
        reportError(QStringLiteral("保存发送消息失败：") + m_database.lastError());
        return;
    }

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

void AppController::synchronizeOnlineUsers()
{
    QVariantList normalizedPeers;

    const QVariantList onlineUsers = m_privateChat.onlineUsers();

    for (const QVariant &item : onlineUsers) {
        const QVariantMap networkUser = item.toMap();

        const QString name = networkUser.value(QStringLiteral("name")).toString().trimmed();

        const QString ip = networkUser.value(QStringLiteral("ip")).toString().trimmed();

        if (name.isEmpty() || ip.isEmpty()) {continue;}

        QVariantMap peer;

        //当前没有UUID，第一阶段使用IP作为peerId。
        peer.insert(QStringLiteral("peerId"), ip);
        peer.insert(QStringLiteral("username"), name);
        peer.insert(QStringLiteral("ip"), ip);
        peer.insert(QStringLiteral("online"), true);

        normalizedPeers.append(peer);
    }

    if (!m_database.synchronizePeers(normalizedPeers)) {
        reportError(QStringLiteral("同步在线用户失败：") + m_database.lastError());
        return;
    }

    refreshPeers();
}

void AppController::handleMessageReceived(const QString &fromName,
                                          const QString &fromIp,
                                          const QString &message)
{
    //如果是本机Ip直接返回
    if (fromIp == m_privateChat.localIp()) {
        return;
    }

    const QString normalizedName = fromName.trimmed();
    const QString normalizedIp = fromIp.trimmed();
    const QString normalizedMessage = message.trimmed();

    if (normalizedIp.isEmpty() || normalizedMessage.isEmpty()) { return;}

    const QString displayName = normalizedName.isEmpty() ? normalizedIp : normalizedName;

    //当前阶段使用IP作为peerId。
    if (!m_database.upsertPeer(normalizedIp, displayName, normalizedIp, true)) {
        reportError(QStringLiteral("保存消息发送者失败：") + m_database.lastError());
        return;
    }

    if (!m_database.saveMessage(normalizedIp, false, normalizedMessage)) {
        reportError(QStringLiteral("保存接收消息失败：") + m_database.lastError());
        return;
    }

    refreshPeers();

    if (m_currentPeerId == normalizedIp) {refreshMessages();}
}

void AppController::refreshPeers()
{
    const QVariantList loadedPeers = m_database.loadPeers();

    if (loadedPeers == m_peers) { return;}

    m_peers = loadedPeers;
    emit peersChanged();
}

void AppController::refreshMessages()
{
    QVariantList loadedMessages;

    if (!m_currentPeerId.isEmpty()) { loadedMessages = m_database.loadMessages(m_currentPeerId);}

    if (loadedMessages == m_messages) { return;}

    m_messages = loadedMessages;
    emit messagesChanged();
}

void AppController::reportError(const QString &message)
{
    m_lastError = message;

    emit lastErrorChanged();
    emit operationFailed(message);
}
