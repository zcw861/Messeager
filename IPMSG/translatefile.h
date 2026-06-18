// Module
// File: translatefile.h   Version: 0.1.0   License: AGPLv3
// Created: ZhouChengWei     2026-06-13 15:02:51
// Description:
//     用于传输文件
//     [v0.1.1] ZhouChengWei    2026-06-14 15:49:32
//         * 添加了部分函数用于文件传输

#pragma once

#include <QObject>
#include <QString>

#include <mutex>
#include <unordered_map>
#include <string>
#include <thread>
#include <atomic>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

//文件传输端口
#define FILE_PORT 45456

//协议控制字节
#define MSG_TYPE_FILE_REQ  0x01   //文件请求
#define MSG_TYPE_FILE_ACK  0x02   //接受/拒绝
#define MSG_TYPE_FILE_DATA 0x03   //文件数据块

//请求
struct FileRequest {
    uint8_t type;
    uint32_t nameLen;
    uint64_t fileSize;
};

//应答
struct FileAck {
    uint8_t type;
    uint8_t accept;   //1接受，0拒绝
};


struct FileDataHeader {
    uint8_t type;
    uint32_t blockSize;
};

struct PendingFileRequest {
    int clientFd = -1;
    std::string fileName;
    uint64_t fileSize = 0;
    std::string senderIp;
};

class TranslateFile : public QObject
{
    Q_OBJECT

public:
    explicit TranslateFile(QObject *parent = nullptr);
    ~TranslateFile();

    void start();   //启动文件传输线程
    void stop();    //停止文件传输线程

signals:
    //收到文件传输请求
    void fileRequestReceived(const QString &fromIp, const QString &fileName, qint64 fileSize);
    //文件传输进度
    void fileTransferProgress(const QString &ip, const QString &fileName, int percent);
    //文件传输完成
    void fileTransferFinished(const QString &ip, const QString &fileName, bool success);

public slots:
    //QML调用：接受文件
    Q_INVOKABLE void acceptFile(const QString &ip, const QString &savePath);
    //QML调用：拒绝文件
    Q_INVOKABLE void rejectFile(const QString &ip);
    //QML调用：发送文件
    Q_INVOKABLE void sendFile(const QString &ip, const QString &filePath);

private:
    //文件传输线程
    void fileServerThread();
    //处理文件传输
    void handleFileConnection(int clientfd, const sockaddr_in &clientAddr);
    //接受文件
    void receiveFileData(int clientfd, const std::string &ip, const std::string &savePath,
                         const std::string &fileName, uint64_t fileSize);
    //发送文件
    void sendFileData(const std::string &ip, const std::string &filePath);
    //如果文件传输出现错误，发射错误信息
    void emitFileError(const std::string &ip, const std::string &fileName, bool success);

    static bool setNonBlocking(int fd);     //设置文件描述符为非阻塞
    static int createEpoll(int fd, uint32_t events);    //EPOLL模式

    std::atomic<bool> m_running{false};
    std::thread m_fileServerThread;

    int m_serverSocket = -1;

    // 待处理的文件请求
    std::unordered_map<std::string, PendingFileRequest> m_pendingRequests;
    std::mutex m_mutex;
};