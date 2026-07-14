// Module
// File: translatefile.cpp   Version: 0.1.0   License: AGPLv3
// Created: ZhouChengWei     2026-06-13 15:02:51
// Description:
//     用于传输文件
//     [v0.1.1] ZhouChengWei    2026-06-14 15:50:56
//         * 实现传输文件的函数
//     [v0.1.2] ZhouChengWei    2026-06-18 15:12:58
//         * 修改了部分代码格式
//     [v0.1.3] ZhouChengWei    2026-06-23 14:03:51
//         * 修复了因为覆盖压缩包文件导致数据丢失的bug（使用临时文件存储再覆盖解决）
//     [v0.1.4] ZhouChengWei    2026-07-14 17:01:05
//         * 增加了实时速度与剩余时间计算
//         * 增加接收方在完成临时文件重命名后，发送FileResult{success=1}给发送方；失败则发送 success=0
//         * 优化了文件传输时的一些错误检查

#include "translatefile.h"
#include "common.h"

#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QElapsedTimer>
#include <cerrno>

TranslateFile::TranslateFile(QObject *parent) : QObject(parent){}

TranslateFile::~TranslateFile()
{
    stop();
}

void TranslateFile::start()
{
    if(m_running) return;

    m_running = true;
    m_fileServerThread = std::thread(&TranslateFile::fileServerThread, this);

    std::cout << "文件传输服务已启动" << std::endl;
}

void TranslateFile::stop()
{
    m_running = false;

    //关闭socket让阻塞调用返回
    if(m_serverSocket != -1){
        shutdown(m_serverSocket, SHUT_RDWR);
    }

    if(m_fileServerThread.joinable()){
        m_fileServerThread.join();
    }

    //关闭所有待处理的连接
    std::lock_guard<std::mutex> lock(m_mutex);
    for(auto &[ip, req] : m_pendingRequests) {
        if(req.clientFd != -1){
            close(req.clientFd);
        }
    }
    m_pendingRequests.clear();

    if(m_serverSocket != -1){
        close(m_serverSocket);
        m_serverSocket = -1;
    }

    std::cout << "文件传输服务已停止" << std::endl;
}

bool TranslateFile::setNonBlocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if(flags == -1){
        perror("fcntl F_GETFL fail");
        return false;
    }
    if(fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1){
        perror("fcntl F_SETFL fail");
        return false;
    }
    return true;
}

int TranslateFile::createEpoll(int fd, uint32_t events)
{
    int epollfd = epoll_create1(0);
    if(epollfd == -1){
        perror("epoll_create1");
        return -1;
    }

    struct epoll_event ev;
    ev.events = events;
    ev.data.fd = fd;

    if(epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &ev) == -1){
        perror("epoll_ctl");
        close(epollfd);
        return -1;
    }

    return epollfd;
}

void TranslateFile::fileServerThread()
{
    m_serverSocket = socket(PF_INET, SOCK_STREAM, 0);
    if(m_serverSocket < 0){
        perror("file server socket create fail");
        return;
    }

    //设置非阻塞
    setNonBlocking(m_serverSocket);

    //设置地址重用
    int reuse = 1;
    setsockopt(m_serverSocket, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    setsockopt(m_serverSocket, SOL_SOCKET, SO_REUSEPORT, &reuse, sizeof(reuse));

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(FILE_PORT);
    address.sin_addr.s_addr = INADDR_ANY;

    if(bind(m_serverSocket, (sockaddr*)&address, sizeof(address)) < 0){
        perror("file server bind failed");
        close(m_serverSocket);
        m_serverSocket = -1;
        return;
    }

    listen(m_serverSocket, 5);

    //创建epoll
    int epollfd = createEpoll(m_serverSocket, EPOLLIN);
    if(epollfd == -1){
        close(m_serverSocket);
        m_serverSocket = -1;
        return;
    }

    std::cout << "文件传输服务器线程已启动（epoll模式）" << std::endl;

    struct epoll_event events[10];

    while(m_running){
        int nfds = epoll_wait(epollfd, events, 10, 1000);  //1秒超时

        if(nfds == -1){
            if (errno == EINTR) continue;   //如果被信号中断，则继续循环
            break;
        }

        for(int i = 0; i < nfds; i++){
            if(events[i].data.fd == m_serverSocket && (events[i].events & EPOLLIN)){
                sockaddr_in clientAddr{};
                socklen_t len = sizeof(clientAddr);

                int clientfd = accept(m_serverSocket, (sockaddr*)&clientAddr, &len);
                if(clientfd < 0){
                    if(errno != EAGAIN && errno != EWOULDBLOCK && m_running){
                        perror("accept failed");
                    }
                    continue;
                }

                //为每个连接创建处理线程
                std::thread([this, clientfd, clientAddr]() {
                    handleFileConnection(clientfd, clientAddr);
                }).detach();
            }
        }
    }

    close(epollfd);
    close(m_serverSocket);
    m_serverSocket = -1;
    std::cout << "文件传输服务器线程已退出" << std::endl;
}

void TranslateFile::handleFileConnection(int clientfd, const sockaddr_in &clientAddr)
{
    std::string ip = inet_ntoa(clientAddr.sin_addr);

    //接收请求类型
    uint8_t type;
    int ret = recv(clientfd, &type, sizeof(type), MSG_WAITALL);
    if(ret != sizeof(type)){
        std::cerr << "接收请求类型失败" << std::endl;
        close(clientfd);
        return;
    }

    if(type == MSG_TYPE_TCP_FILE_REQ){
        //接收文件请求头
        uint32_t nameLen;
        uint64_t fileSize;

        ret = recv(clientfd, &nameLen, sizeof(nameLen), MSG_WAITALL);
        if(ret != sizeof(nameLen)){
            close(clientfd);
            return;
        }

        ret = recv(clientfd, &fileSize, sizeof(fileSize), MSG_WAITALL);
        if(ret != sizeof(fileSize)){
            close(clientfd);
            return;
        }

        nameLen = ntohl(nameLen);
        fileSize = be64toh(fileSize);

        //接收文件名
        std::vector<char> nameBuf(nameLen + 1, 0);
        ret = recv(clientfd, nameBuf.data(), nameLen, MSG_WAITALL);
        if(ret != (int)nameLen){
            close(clientfd);
            return;
        }

        std::string fileName(nameBuf.data(), nameLen);

        std::cout << "收到文件请求: " << fileName << " (" << fileSize << " 字节) 来自 " << ip << std::endl;

        //保存待处理请求
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            //如果之前有来自同一IP的请求，先关闭旧连接
            auto it = m_pendingRequests.find(ip);
            if(it != m_pendingRequests.end()){
                if(it->second.clientFd != -1){
                    close(it->second.clientFd);
                }
            }
            m_pendingRequests[ip] = {clientfd, fileName, fileSize, ip};
        }

        //通知UI
        QMetaObject::invokeMethod(this, [this, ip, fileName, fileSize](){
            emit fileRequestReceived(
                QString::fromStdString(ip),
                QString::fromStdString(fileName),
                static_cast<qint64>(fileSize)
                );
        }, Qt::QueuedConnection);
    }else{
        std::cerr << "未知请求类型: " << (int)type << std::endl;
        close(clientfd);
    }
}

void TranslateFile::acceptFile(const QString &ip, const QString &savePath)
{
    std::string ipStr = ip.toStdString();
    std::string savePathStr = savePath.toStdString();

    int clientfd = -1;
    std::string fileName;
    uint64_t fileSize = 0;

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_pendingRequests.find(ipStr);
        if(it != m_pendingRequests.end()){
            clientfd = it->second.clientFd;
            fileName = it->second.fileName;
            fileSize = it->second.fileSize;
            m_pendingRequests.erase(it);
        }
    }

    if (clientfd < 0) {
        std::cerr << "没有来自 " << ipStr << " 的待处理文件请求" << std::endl;

        const QString failedFileName =
            QFileInfo(savePath).fileName();

        QMetaObject::invokeMethod(this,[this, ip, failedFileName](){
                emit fileTransferFinished(ip, failedFileName, false);
            },Qt::QueuedConnection);

        return;
    }

    std::cout << "接受文件: " << fileName << " 保存到: " << savePathStr << std::endl;

    //在新线程中接收文件数据
    std::thread([this, clientfd, ipStr, savePathStr, fileName, fileSize]() {
        FileAck ack;
        ack.type = MSG_TYPE_TCP_FILE_ACK;
        ack.accept = 1;

        if (!sendAll(clientfd, &ack, sizeof(ack))) {
            std::cerr << "发送文件接受应答失败，对方可能已断开: " << fileName << std::endl;

            close(clientfd);
            emitFileError(ipStr, fileName, false);
            return;
        }

        receiveFileData(clientfd, ipStr, savePathStr, fileName, fileSize);
    }).detach();
}

void TranslateFile::rejectFile(const QString &ip)
{
    std::string ipStr = ip.toStdString();

    int clientfd = -1;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_pendingRequests.find(ipStr);
        if(it != m_pendingRequests.end()){
            clientfd = it->second.clientFd;
            m_pendingRequests.erase(it);
        }
    }

    if(clientfd < 0){
        std::cerr << "没有来自 " << ipStr << " 的待处理文件请求" << std::endl;
        return;
    }

    std::cout << "拒绝文件: 来自 " << ipStr << std::endl;

    //发送拒绝应答
    FileAck ack;
    ack.type = MSG_TYPE_TCP_FILE_ACK;
    ack.accept = 0;
    sendAll(clientfd, &ack, sizeof(ack));
    close(clientfd);
}

void TranslateFile::receiveFileData(int clientfd, const std::string &ip,
                                    const std::string &savePath,
                                    const std::string &fileName,
                                    uint64_t fileSize)
{
    const QString qSavePath = QString::fromStdString(savePath);

    //确保目录存在
    QFileInfo fileInfo(qSavePath);
    QDir dir = fileInfo.absoluteDir();
    if (!dir.mkpath(".")) {
        std::cerr << "无法创建目录: " << dir.path().toStdString() << std::endl;
        emitFileError(ip, fileName, false);
        close(clientfd);
        return;
    }

    //使用临时文件接收
    const QString tempPath = qSavePath + ".part";
    QFile file(tempPath);
    if (!file.open(QIODevice::WriteOnly)) {
        std::cerr << "无法创建临时文件: " << tempPath.toStdString() << std::endl;
        emitFileError(ip, fileName, false);
        close(clientfd);
        return;
    }

    char buffer[FILE_BUF_SIZE];

    uint64_t totalReceived = 0;

    bool success = false;
    bool errorOccurred = false;

    //记录整个传输经过的时间。
    //这里使用QElapsedTimer，不使用系统时间，避免系统时间改变影响速度计算。
    QElapsedTimer speedTimer;
    speedTimer.start();

    //上一次计算速度时的时间点。
    qint64 lastSampleMs = 0;

    //上一次计算速度时已经接收的字节数。
    uint64_t lastSampleBytes = 0;

    //用于显示的实时速度，这里会经过轻微平滑，避免速度数字剧烈跳动。
    double currentSpeedKBps = 0.0;

    while (!errorOccurred) {      //用标志控制外层循环
        //接收数据块头
        FileDataHeader header;
        int ret;
        do {
            ret = recv(clientfd, &header, sizeof(header), MSG_WAITALL);
        } while (ret < 0 && errno == EINTR);
        if (ret != sizeof(header)) {
            std::cerr << "接收数据头失败" << std::endl;
            errorOccurred = true;
            break;
        }
        if (header.type != MSG_TYPE_TCP_FILE_DATA) {
            std::cerr << "意外消息类型: " << (int)header.type << std::endl;
            errorOccurred = true;
            break;
        }
        uint32_t blockSize = ntohl(header.blockSize);

        //防止错误或恶意数据块超过本地缓冲区大小。
        if (blockSize > FILE_BUF_SIZE) {
            std::cerr << "文件数据块过大: " << blockSize << std::endl;
            errorOccurred = true;
            break;
        }

        //本次数据块不能让总接收字节数超过文件声明大小。
        if (totalReceived + blockSize > fileSize) {
            std::cerr << "文件数据超过声明大小" << std::endl;
            errorOccurred = true;
            break;
        }

        if (blockSize == 0) {
            //收到结束标记不代表一定成功，还必须确认实际接收字节数等于文件请求中的总大小。
            if (totalReceived == fileSize) {
                success = true;
                std::cout << "文件数据接收完成: " << fileName << std::endl;
            } else {
                std::cerr << "文件大小不完整:"
                          << " expected=" << fileSize
                          << " received=" << totalReceived
                          << std::endl;
                errorOccurred = true;
            }
            break;
        }

        //接收数据块
        uint32_t received = 0;
        while (received < blockSize) {
            do {
                ret = recv(clientfd, buffer + received, blockSize - received, MSG_WAITALL);
            } while (ret < 0 && errno == EINTR);
            if (ret <= 0) {
                std::cerr << "接收数据块失败" << std::endl;
                errorOccurred = true;
                break;  //跳出内层循环
            }
            received += ret;
        }
        if (errorOccurred) break;  //错误发生，跳出外层循环

        //写入文件并检查
        qint64 written = file.write(buffer, blockSize);
        if (written != blockSize) {
            std::cerr << "写入文件失败" << std::endl;
            errorOccurred = true;
            break;
        }
        totalReceived += blockSize;

        //获取从传输开始到当前经过的总毫秒数。
        const qint64 nowMs = speedTimer.elapsed();

        //距离上一次速度计算经过的毫秒数。
        const qint64 sampleMs = nowMs - lastSampleMs;

        //每隔约250毫秒更新一次速度。如果文件已经全部接收，也立即更新一次。
        const bool shouldReport = sampleMs >= 250 || totalReceived == fileSize;

        if (shouldReport) {
            //本次采样期间接收的字节数。
            const uint64_t sampleBytes = totalReceived - lastSampleBytes;

            double sampledSpeedKBps = 0.0;

            if (sampleMs > 0) {
                sampledSpeedKBps = (static_cast<double>(sampleBytes) / 1024.0)
                                    / (static_cast<double>(sampleMs) / 1000.0);
            }

            //第一次采样直接使用采样速度，后续使用加权平均，减少速度显示剧烈跳动。
            if (currentSpeedKBps <= 0.0) {
                currentSpeedKBps = sampledSpeedKBps;
            }else{
                currentSpeedKBps = currentSpeedKBps * 0.7 + sampledSpeedKBps * 0.3;
            }

            const uint64_t remainingBytes = fileSize > totalReceived ? fileSize - totalReceived : 0;

            int remainingSeconds = -1;

            if (currentSpeedKBps > 0.01) {
                remainingSeconds = static_cast<int>((static_cast<double>(remainingBytes) / 1024.0)
                                    / currentSpeedKBps);
            }

            const int rawPercent = fileSize > 0 ? static_cast<int>((totalReceived * 100) / fileSize) : 100;

            //接收方尚未完成临时文件重命名，因此数据阶段最高只显示99%。
            const int percent = rawPercent >= 100 ? 99 : rawPercent;

            QMetaObject::invokeMethod(this,[this,ip,fileName,percent,currentSpeedKBps,remainingSeconds](){
                    emit fileTransferProgress(
                        QString::fromStdString(ip),
                        QString::fromStdString(fileName),
                        percent,
                        currentSpeedKBps,
                        remainingSeconds
                        );
                },Qt::QueuedConnection);

            //保存本次采样位置，供下次计算使用。
            lastSampleMs = nowMs;
            lastSampleBytes = totalReceived;
        }
    }

    file.close();

    //默认最终结果为失败，只有本地文件重命名成功后才改成成功。
    FileResult result;
    result.type = MSG_TYPE_TCP_FILE_RESULT;
    result.success = 0;

    if (success && !errorOccurred) {
        //目标路径如果已有旧文件，先删除旧文件。
        if (QFile::exists(qSavePath)) {
            QFile::remove(qSavePath);
        }

        //只有临时文件成功重命名为正式文件，
        //才表示接收方真正保存完成。
        if (QFile::rename(tempPath, qSavePath)) {
            result.success = 1;

            //把最终保存结果发给发送方,发送方收到success=1后才显示100%。
            sendAll(clientfd, &result, sizeof(result));

            QMetaObject::invokeMethod(this,[this, ip, fileName, currentSpeedKBps](){
                    //接收端本地保存完成后，最后发送一次100%。
                    emit fileTransferProgress(
                        QString::fromStdString(ip),
                        QString::fromStdString(fileName),
                        100,
                        currentSpeedKBps,
                        0
                        );

                    emit fileTransferFinished(
                        QString::fromStdString(ip),
                        QString::fromStdString(fileName),
                        true
                        );
                },Qt::QueuedConnection);
        }else{
            std::cerr << "临时文件重命名失败" << std::endl;

            //尽量通知发送方本地保存失败。
            sendAll(clientfd, &result, sizeof(result));

            QFile::remove(tempPath);
            emitFileError(ip, fileName, false);
        }
    }else{
        //接收失败时删除不完整的临时文件。
        QFile::remove(tempPath);

        //如果socket仍然可用，尽量通知发送方失败。
        sendAll(clientfd, &result, sizeof(result));

        emitFileError(ip, fileName, false);
    }

    close(clientfd);
}

void TranslateFile::sendFile(const QString &ip, const QString &filePath)
{
    std::string ipStr = ip.toStdString();
    std::string filePathStr = filePath.toStdString();

    //在新线程中发送
    std::thread([this, ipStr, filePathStr](){
        sendFileData(ipStr, filePathStr);
    }).detach();
}

void TranslateFile::sendFileData(const std::string &ip, const std::string &filePath)
{
    QFileInfo fileInfo(QString::fromStdString(filePath));
    if (!fileInfo.exists() || !fileInfo.isFile()) {
        std::cerr << "文件不存在: " << filePath << std::endl;
        QMetaObject::invokeMethod(this, [this, ip, filePath](){
            emit fileTransferFinished(
                QString::fromStdString(ip),
                QFileInfo(QString::fromStdString(filePath)).fileName(),
                false
                );
        }, Qt::QueuedConnection);
        return;
    }

    //连接到目标
    int clientfd = socket(PF_INET, SOCK_STREAM, 0);
    if (clientfd < 0) {
        perror("file socket create fail");

        emitFileError(ip, fileInfo.fileName().toStdString(), false);
        return;
    }

    sockaddr_in destAddr{};
    destAddr.sin_family = AF_INET;
    destAddr.sin_port = htons(FILE_PORT);
    if (inet_pton(AF_INET, ip.c_str(), &destAddr.sin_addr) != 1) {
        std::cerr << "无效的IP地址: " << ip << std::endl;
        close(clientfd);
        emitFileError(ip, fileInfo.fileName().toStdString(), false);
        return;
    }

    if(::connect(clientfd, (sockaddr*)&destAddr, sizeof(destAddr)) < 0) {
        std::cerr << "连接文件传输服务器失败: " << ip << std::endl;
        close(clientfd);
        QMetaObject::invokeMethod(this, [this, ip, fileName = fileInfo.fileName()](){
            emit fileTransferFinished(QString::fromStdString(ip), QString(fileName), false);
        }, Qt::QueuedConnection);
        return;
    }

    //发送文件请求
    std::string fileName = fileInfo.fileName().toStdString();
    uint32_t nameLen = fileName.size();
    uint64_t fileSize = fileInfo.size();

    uint8_t type = MSG_TYPE_TCP_FILE_REQ;
    uint32_t nameLenNet = htonl(nameLen);
    uint64_t fileSizeNet = htobe64(fileSize);

    if (!sendAll(clientfd, &type, sizeof(type))
        || !sendAll(clientfd, &nameLenNet, sizeof(nameLenNet))
        || !sendAll(clientfd, &fileSizeNet, sizeof(fileSizeNet))
        || !sendAll(clientfd, fileName.c_str(), nameLen))
    {
        std::cerr << "发送文件请求失败，对方可能已断开: " << ip << std::endl;

        close(clientfd);

        QMetaObject::invokeMethod(this,[this, ip, fileName](){
            emit fileTransferFinished(QString::fromStdString(ip), QString::fromStdString(fileName), false);
        },Qt::QueuedConnection);
        return;
    }

    std::cout << "发送文件请求: " << fileName << " (" << fileSize << " 字节) 到 " << ip << std::endl;

    //等待应答
    FileAck ack;
    int ret = recv(clientfd, &ack, sizeof(ack), MSG_WAITALL);
    if(ret != sizeof(ack) || ack.type != MSG_TYPE_TCP_FILE_ACK || ack.accept != 1){
        std::cout << "文件被拒绝或应答错误" << std::endl;
        close(clientfd);
        QMetaObject::invokeMethod(this, [this, ip, fileName](){
            emit fileTransferFinished(
                QString::fromStdString(ip),
                QString::fromStdString(fileName),
                false
                );
        }, Qt::QueuedConnection);
        return;
    }

    std::cout << "对方已接受，开始传输: " << fileName << std::endl;

    //发送文件数据
    QFile file(QString::fromStdString(filePath));
    if (!file.open(QIODevice::ReadOnly)) {
        std::cerr << "无法打开文件: " << filePath << std::endl;

        close(clientfd);
        emitFileError(ip, fileName, false);

        return;
    }

    char buffer[FILE_BUF_SIZE];

    qint64 totalSent = 0;

    //用于计算实时速度。
    QElapsedTimer speedTimer;
    speedTimer.start();

    //上一次采样时间。
    qint64 lastSampleMs = 0;

    //上一次采样时已发送字节数。
    qint64 lastSampleBytes = 0;

    //平滑后的当前速度。
    double currentSpeedKBps = 0.0;

    while (!file.atEnd()) {
        qint64 bytesRead = file.read(buffer, FILE_BUF_SIZE);
        if (bytesRead <= 0) { break; }

        FileDataHeader header;
        header.type = MSG_TYPE_TCP_FILE_DATA;
        header.blockSize = htonl(static_cast<uint32_t>(bytesRead));

        //发送数据块头失败: 对方已经断开或socket异常
        if (!sendAll(clientfd, &header, sizeof(header))) {
            std::cerr << "发送数据头失败，对方可能已断开: " << fileName << std::endl;

            file.close();
            close(clientfd);

            QMetaObject::invokeMethod(this,[this, ip, fileName](){
                    emit fileTransferFinished(
                        QString::fromStdString(ip),
                        QString::fromStdString(fileName), false);
                },Qt::QueuedConnection);

            return;
        }

        //发送数据块失败: 说明对方已经断开或socket异常
        if (!sendAll(clientfd, buffer, static_cast<size_t>(bytesRead))) {
            std::cerr << "发送数据块失败，对方可能已断开: " << fileName << std::endl;

            file.close();
            close(clientfd);

            QMetaObject::invokeMethod(this,[this, ip, fileName](){
                    emit fileTransferFinished(
                        QString::fromStdString(ip),
                        QString::fromStdString(fileName), false);
                },Qt::QueuedConnection);
            return;
        }

        //只有文件数据真正写入socket后，才增加发送字节数。
        totalSent += bytesRead;

        const qint64 nowMs = speedTimer.elapsed();
        const qint64 sampleMs = nowMs - lastSampleMs;

        //每250毫秒更新一次，如果数据已经全部写入，也立即更新。
        const bool shouldReport = sampleMs >= 250 || totalSent == fileSize;

        if (shouldReport) {
            const qint64 sampleBytes = totalSent - lastSampleBytes;

            double sampledSpeedKBps = 0.0;

            if (sampleMs > 0) {
                sampledSpeedKBps = (static_cast<double>(sampleBytes) / 1024.0)
                                   / (static_cast<double>(sampleMs) / 1000.0);
            }

            //平滑速度。
            if (currentSpeedKBps <= 0.0) {
                currentSpeedKBps = sampledSpeedKBps;
            } else {
                currentSpeedKBps = currentSpeedKBps * 0.7 + sampledSpeedKBps * 0.3;
            }

            const qint64 remainingBytes = fileSize > static_cast<uint64_t>(totalSent)
                                            ? static_cast<qint64>(fileSize) - totalSent
                                            : 0;

            int remainingSeconds = -1;

            if (currentSpeedKBps > 0.01) {
                remainingSeconds = static_cast<int>((static_cast<double>(remainingBytes) / 1024.0)
                                                    / currentSpeedKBps);
            }

            const int rawPercent = fileSize > 0 ? static_cast<int>((totalSent * 100) / fileSize) : 100;

            //还没有收到接收方最终确认，最多只显示99%
            const int percent = rawPercent >= 100 ? 99 : rawPercent;

            QMetaObject::invokeMethod(this,[this,ip,fileName,percent,currentSpeedKBps,remainingSeconds](){
                    emit fileTransferProgress(
                        QString::fromStdString(ip),
                        QString::fromStdString(fileName),
                        percent,
                        currentSpeedKBps,
                        remainingSeconds);
                },Qt::QueuedConnection);

            lastSampleMs = nowMs;
            lastSampleBytes = totalSent;
        }
    }

    file.close();

    //发送结束标志
    FileDataHeader endHeader;
    endHeader.type = MSG_TYPE_TCP_FILE_DATA;
    endHeader.blockSize = 0;  //0表示传输结束

    if (!sendAll(clientfd, &endHeader, sizeof(endHeader))) {
        std::cerr << "发送结束标志失败，对方可能已断开: " << fileName << std::endl;

        close(clientfd);

        QMetaObject::invokeMethod(this,[this, ip, fileName](){
                emit fileTransferFinished(
                    QString::fromStdString(ip),
                    QString::fromStdString(fileName), false);
            },Qt::QueuedConnection);

        return;
    }

    //结束标志只表示“发送方已经没有更多数据”，发送方还必须等待接收方确认文件已经完整保存。
    FileResult result;

    int resultRet;

    do {
        resultRet = recv(clientfd, &result, sizeof(result), MSG_WAITALL);
    } while (resultRet < 0 && errno == EINTR);

    if (resultRet != static_cast<int>(sizeof(result))
        || result.type != MSG_TYPE_TCP_FILE_RESULT
        || result.success != 1) {

        std::cerr << "接收方未确认文件保存成功: "
                  << fileName << std::endl;

        close(clientfd);

        emitFileError(ip, fileName, false);
        return;
    }

    std::cout << "接收方确认文件保存完成: " << fileName << std::endl;

    //收到接收方最终成功确认后，才显示100%。
    QMetaObject::invokeMethod(this,[this,ip,fileName,currentSpeedKBps](){
            emit fileTransferProgress(
                QString::fromStdString(ip),
                QString::fromStdString(fileName),
                100,
                currentSpeedKBps,
                0
                );

            emit fileTransferFinished(
                QString::fromStdString(ip),
                QString::fromStdString(fileName),
                true
                );
        },Qt::QueuedConnection);
    close(clientfd);
}

//发射文件错误信号
void TranslateFile::emitFileError(const std::string &ip, const std::string &fileName, bool success)
{
    QMetaObject::invokeMethod(this, [this, ip, fileName, success](){
        emit fileTransferFinished(
            QString::fromStdString(ip),
            QString::fromStdString(fileName),
            success
            );
    }, Qt::QueuedConnection);
}

bool TranslateFile::sendAll(int fd, const void *buf, size_t len)
{
    size_t total = 0;

    while (total < len) {
        const ssize_t n = send(fd, static_cast<const char *>(buf) + total, len - total, MSG_NOSIGNAL);

        if (n > 0) {
            total += static_cast<size_t>(n);
            continue;
        }

        if (n < 0 && errno == EINTR)
            continue;
        return false;
    }
    return true;
}