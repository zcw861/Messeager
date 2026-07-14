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

    if(clientfd < 0){
        std::cerr << "没有来自 " << ipStr << " 的待处理文件请求" << std::endl;
        return;
    }

    std::cout << "接受文件: " << fileName << " 保存到: " << savePathStr << std::endl;

    //在新线程中接收文件数据
    std::thread([this, clientfd, ipStr, savePathStr, fileName, fileSize](){
        //发送接受应答
        FileAck ack;
        ack.type = MSG_TYPE_TCP_FILE_ACK;
        ack.accept = 1;
        sendAll(clientfd, &ack, sizeof(ack));

        //接收文件数据
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
    bool errorOccurred = false;   //标记是否发生错误

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
        if (blockSize == 0) {
            success = true;
            std::cout << "文件传输完成: " << fileName << std::endl;
            break;  //正常结束，跳出外层循环
        }

        //接收数据块（处理EINTR）
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

        //进度通知
        int percent = fileSize > 0 ? (totalReceived * 100 / fileSize) : 100;
        QMetaObject::invokeMethod(this, [this, ip, fileName, percent]() {
            emit fileTransferProgress(
                QString::fromStdString(ip),
                                      QString::fromStdString(fileName),
                                      percent);
        }, Qt::QueuedConnection);
    }

    file.close();

    if (success && !errorOccurred) {
        //传输成功：删除旧文件，将临时文件重命名为目标文件
        if (QFile::exists(qSavePath)) {
            QFile::remove(qSavePath);
        }
        if (QFile::rename(tempPath, qSavePath)) {
            QMetaObject::invokeMethod(this, [this, ip, fileName]() {
                emit fileTransferFinished(
                    QString::fromStdString(ip),
                                          QString::fromStdString(fileName),
                                          true);
            }, Qt::QueuedConnection);
        } else {
            std::cerr << "重命名失败" << std::endl;
            emitFileError(ip, fileName, false);
            QFile::remove(tempPath);
        }
    } else {
        //失败：删除临时文件
        QFile::remove(tempPath);
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
    if(clientfd < 0){
        perror("sendAll file socket create fail");
        return;
    }

    sockaddr_in destAddr{};
    destAddr.sin_family = AF_INET;
    destAddr.sin_port = htons(FILE_PORT);
    if(inet_pton(AF_INET, ip.c_str(), &destAddr.sin_addr) != 1) {
        std::cerr << "无效的IP地址: " << ip << std::endl;
        close(clientfd);
        return;
    }

    if(::connect(clientfd, (sockaddr*)&destAddr, sizeof(destAddr)) < 0) {
        std::cerr << "连接文件传输服务器失败: " << ip << std::endl;
        close(clientfd);
        QMetaObject::invokeMethod(this, [this, ip, fileName = fileInfo.fileName()](){
            emit fileTransferFinished(
                QString::fromStdString(ip),
                                      QString(fileName),
                                      false
            );
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

    sendAll(clientfd, &type, sizeof(type));
    sendAll(clientfd, &nameLenNet, sizeof(nameLenNet));
    sendAll(clientfd, &fileSizeNet, sizeof(fileSizeNet));
    sendAll(clientfd, fileName.c_str(), nameLen);

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
    if(!file.open(QIODevice::ReadOnly)){
        std::cerr << "无法打开文件: " << filePath << std::endl;
        close(clientfd);
        return;
    }

    char buffer[FILE_BUF_SIZE];
    qint64 totalSent = 0;
    int lastPercent = -1;

    while(!file.atEnd()){
        qint64 bytesRead = file.read(buffer, FILE_BUF_SIZE);
        if (bytesRead <= 0) break;

        //发送数据块头
        FileDataHeader header;
        header.type = MSG_TYPE_TCP_FILE_DATA;
        header.blockSize = htonl(static_cast<uint32_t>(bytesRead));
        sendAll(clientfd, &header, sizeof(header));

        //发送数据
        sendAll(clientfd, buffer, bytesRead);

        totalSent += bytesRead;

        //报告进度（每5%通知一次）
        int percent = fileSize > 0 ? (totalSent * 100 / fileSize) : 100;
        if(percent != lastPercent){
            lastPercent = percent;
            QMetaObject::invokeMethod(this, [this, ip, fileName, percent](){
                emit fileTransferProgress(
                    QString::fromStdString(ip),
                                          QString::fromStdString(fileName),
                                          percent
                );
            }, Qt::QueuedConnection);
        }
    }

    file.close();

    //发送结束标志
    FileDataHeader endHeader;
    endHeader.type = MSG_TYPE_TCP_FILE_DATA;
    endHeader.blockSize = 0;  //0表示传输结束
    sendAll(clientfd, &endHeader, sizeof(endHeader));

    std::cout << "文件发送完成: " << fileName << std::endl;

    QMetaObject::invokeMethod(this, [this, ip, fileName](){
        emit fileTransferFinished(
            QString::fromStdString(ip),
                                  QString::fromStdString(fileName),
                                  true
        );
    }, Qt::QueuedConnection);

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

bool TranslateFile::sendAll(int fd, const void* buf, size_t len){
    size_t total = 0;
    while (total < len) {
        ssize_t n = send(fd, (const char*)buf + total, len - total, 0);
        if (n > 0) {
            total += n;
        } else if (n < 0) {
            if (errno == EINTR) continue;
            return false;
        } else {
            return false;
        }
    }
    return true;
}
