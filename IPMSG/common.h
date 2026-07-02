// Module
// File: common.h   Version: 0.1.0   License: AGPLv3
// Created: ZhouChengWei      2026-06-22 17:22:09
// Description:
//     协议以及共用结构体的集合
//     [v0.1.1] ZhouChengWei    2026-06-22 22:27:27
//         * 添加消息结构体，区别消息类型
//         * 添加序列化/反序列化函数，便于在网络中传输消息
//         * 添加构建TCP/UDP网络包的辅助函数
//     [v0.1.2] HeZhiyuan    2026-06-23 22:06:14
//         * 统一群聊协议常量
//     [v0.1.3] ZhouChengWei    2026-06-25 17:34:15
//         * 完善群聊协议(握手，数据)
//     [v0.1.4] ZhouChengWei    2026-06-27 14:41:07
//         * 添加了解散/退群群聊的消息类型

#pragma once

#include <netinet/in.h>

#include <string>
#include <cstring>
#include <chrono>
#include <cstdint>
#include <vector>

#define UDP_PORT 45454          //UDP端口
#define TCP_PORT 45455          //TCP端口
#define FILE_PORT 45456         //文件传输端口
#define MESSAGE_BUF_SIZE 1024   //消息缓冲区大小
#define FILE_BUF_SIZE 8192      //文件缓冲区大小


//UDP消息类型(0x10 ~ 0x2F)
#define MSG_TYPE_UDP_BROADCAST      0x10   //广播发现用户
#define MSG_TYPE_UDP_UNICAST        0x11   //单播邀请（群聊）
#define MSG_TYPE_UDP_ACK            0x12   //邀请应答

//TCP消息类型(0x30 ~ 0x4F)
#define MSG_TYPE_TCP_PRIVATE        0x30   //普通私聊消息
#define MSG_TYPE_TCP_GROUP          0x31   //群聊消息
#define MSG_TYPE_TCP_FILE_REQ       0x32   //文件请求
#define MSG_TYPE_TCP_FILE_ACK       0x33   //文件接受/拒绝
#define MSG_TYPE_TCP_FILE_DATA      0x34   //文件数据块
#define MSG_TYPE_TCP_GROUP_HELLO    0x35   //群聊连接握手
#define MSG_TYPE_TCP_GROUP_DATA     0x36   //群聊消息数据
#define MSG_TYPE_TCP_GROUP_ACK      0x37   //群聊TCP握手确认
#define MSG_TYPE_TCP_GROUP_LEAVE    0x38   //成员退出群聊
#define MSG_TYPE_TCP_GROUP_DISMISS  0x39   //群主解散群聊

//单个TCP载荷允许的最大字节数（1MB）
#define MAX_TCP_PAYLOAD_SIZE (1024 * 1024)

//用户结构体
struct UserInfo
{
    std::string name;
    std::string ip;
    std::chrono::steady_clock::time_point lastSeen; //最后活跃时刻
    std::string id;     //用户唯一标识ID
};

//消息结构体
struct MsgData{
    uint8_t type;           //消息类型
    std::string senderId;   //发送者ID
    std::string senderName; //发送者名字
    std::string targetId;   //私聊时为对方ID，群聊时为群ID
    std::string message;    //消息内容
};

//序列化MsgData为二进制载荷
//格式：依次写入senderId、senderName、targetId、message，
//每个字段先写入4字节长度（网络字节序），再写入实际字符串内容。
//msg：要序列化的消息结构体
//返回连续的二进制字符串，直接作为TCP包的载荷
inline std::string serializeMsgData(const MsgData& msg){
    std::string out;
    auto appendStr = [&](const std::string& s){
        uint32_t len = htonl(s.size());     //转为网络字节序
        out.append(reinterpret_cast<const char*>(&len), 4); //写入4字节长度
        out.append(s);      //写入字符串数据
    };
    appendStr(msg.senderId);
    appendStr(msg.senderName);
    appendStr(msg.targetId);
    appendStr(msg.message);
    return out;
}

//从二进制载荷反序列化MsgData
//data：二进制数据起始地址
//len：数据总长度（字节）
//返回反序列化后的MsgData结构体
inline MsgData deserializeMsgData(const char* data, size_t len) {
    MsgData msg;
    size_t offset = 0;
    auto readStr = [&]() -> std::string{
        if(offset + 4 > len) return "";     //不够读长度字段
        uint32_t slen;
        memcpy(&slen, data + offset, 4);    //读取4字节长度
        slen = ntohl(slen);                 //转为主机字节序
        offset += 4;
        if(offset + slen > len) return "";  //数据不够
        std::string s(data + offset, slen); //读取字符串
        offset += slen;
        return s;
    };
    msg.senderId = readStr();
    msg.senderName = readStr();
    msg.targetId = readStr();
    msg.message = readStr();
    return msg;
}

//构建UDP网络包
//格式：1字节类型 + 变长载荷字符串
inline std::vector<uint8_t> buildUdpPacket(uint8_t type, const std::string& payload) {
    std::vector<uint8_t> pkt;
    pkt.push_back(type);    //写入类型字节
    pkt.insert(pkt.end(), payload.begin(), payload.end()); //写入载荷
    return pkt;
}

//构建TCP网络包
//格式：1字节类型 + 4字节载荷长度（网络字节序） + 载荷数据
inline std::vector<uint8_t> buildTcpPacket(uint8_t type, const std::string& payload) {
    std::vector<uint8_t> pkt;
    pkt.push_back(type);    //写入类型字节
    uint32_t len = htonl(static_cast<uint32_t>(payload.size())); //载荷长度转网络序
    const auto* lenBytes = reinterpret_cast<const uint8_t*>(&len);
    pkt.insert(pkt.end(), lenBytes, lenBytes + 4);    //写入4字节长度
    pkt.insert(pkt.end(), payload.begin(), payload.end()); //写入载荷数据
    return pkt;
}
