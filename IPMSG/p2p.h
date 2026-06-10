// Module
// File: p2p.h   Version: 0.1.0   License: AGPLv3
// Created:  ZhouChengWei     2026-06-9 19:29:14
// Description:
//     部分函数实现基础发现用户user、私聊功能

#ifndef P2P_H
#define P2P_H

#include <string>
#include <unordered_map>
#include <mutex>

//用户信息
struct UserInfo {
    std::string name;
    std::string ip;
};

extern std::unordered_map<std::string, UserInfo> peers; //ip地址与用户名字的映射
extern std::mutex peer_mutex;

void start_udp_broadcast(const std::string& username);  //udp广播
void start_udp_listener();  //接受udp广播
void start_tcp_server();    //tcp服务器，用于收发消息
void send_message(const std::string& ip, const std::string& msg);   //输入用户ip,发送消息

#endif
