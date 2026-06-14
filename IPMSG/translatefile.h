// Module
// File: translatefile.h   Version: 0.1.0   License: AGPLv3
// Created: ZhouChengWei     2026-06-13 15:02:51
// Description:
//     用于传输文件

#pragma once

#include <QObject>
#include <QString>

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
