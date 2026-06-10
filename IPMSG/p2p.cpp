// Module
// File: p2p.cpp   Version: 0.1.0   License: AGPLv3
// Created:  ZhouChengWei     2026-06-09 19:30:06
// Description:
//      见p2p.h文件的描述，本实现的广播，接收广播以及收发消息各一个线程

#include "p2p.h"

#include <iostream>
#include <thread>
#include <cstring>

#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define UDP_PORT 45454  //udp端口
#define TCP_PORT 45455  //tcp端口
#define BUF_SIZE 1024   //缓冲区大小

std::unordered_map<std::string, UserInfo> peers;
std::mutex peer_mutex;

std::string local_name;

void start_udp_broadcast(const std::string& username)
{
    local_name = username;

    //一个守护线程用于udp广播
    std::thread([](){

        int listenfd = socket(PF_INET, SOCK_DGRAM, 0);

        //设置能往广播地址“255.255.255.255”发包
        int broadcastEnable = 1;
        setsockopt(listenfd, SOL_SOCKET, SO_BROADCAST,&broadcastEnable, sizeof(broadcastEnable));

        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_port = htons(UDP_PORT);
        address.sin_addr.s_addr = inet_addr("255.255.255.255");

        while(true)
        {
            sendto(listenfd,local_name.c_str(),local_name.size(),0,(struct sockaddr*)&address,sizeof(address));
            sleep(2);
        }

    }).detach();
}

void start_udp_listener()
{
    //一个守护线程用于监听广播
    std::thread([](){

        int listenfd = socket(PF_INET, SOCK_DGRAM, 0);

        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_port = htons(UDP_PORT);
        address.sin_addr.s_addr = INADDR_ANY;

        //清除bind的超时错误
        int reuse = 1;
        setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

        if(bind(listenfd, (sockaddr*)&address, sizeof(address)) < 0)
        {
            perror("udp bind failed");
            return;
        }

        char userName[BUF_SIZE];

        while(true)
        {
            sockaddr_in sender{};
            socklen_t len = sizeof(sender);
            memset(&sender, 0, sizeof(sender));

            int n = recvfrom(listenfd, userName, BUF_SIZE - 1, 0, (sockaddr*)&sender,&len);
            if(n <= 0) continue;

            userName[n] = 0;
            std::string ip = inet_ntoa(sender.sin_addr);
            {
                std::lock_guard<std::mutex> lock(peer_mutex);
                peers[ip] = {userName,ip};
            }
        }

    }).detach();
}

void start_tcp_server()
{
    //一个守护线程用于tcp收发消息
    std::thread([](){

        int serverfd = socket(PF_INET, SOCK_STREAM, 0);

        //tcp服务器结构体
        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_port = htons(TCP_PORT);
        address.sin_addr.s_addr = INADDR_ANY;

        //清除bind超时错误
        int reuse = 1;
        setsockopt(serverfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

        if(bind(serverfd, (struct sockaddr*)&address, sizeof(address)) < 0)
        {
            perror("tcp bind failed\n");
            return;
        }

        listen(serverfd, 10);

        while(true)
        {
            //tcp客户端结构体
            sockaddr_in client_address{};
            socklen_t len = sizeof(client_address);

            int clientfd = accept(serverfd,(sockaddr*)&client_address,&len);

            //子线程用于处理消息
            std::thread([clientfd, client_address](){

                char buffer[BUF_SIZE];

                int n = recv(clientfd, buffer, BUF_SIZE - 1, 0);

                char* ip = inet_ntoa(client_address.sin_addr);  //对方ip地址
                if(n > 0)
                {
                    buffer[n] = 0;

                    std::cout << "来自 " << peers[ip].name << "(" << ip << ")" << " 的消息:\n";
                    std::cout << buffer << std::endl;
                }

                close(clientfd);

            }).detach();
        }

    }).detach();
}

void send_message(const std::string& ip,const std::string& msg)
{
    int sendfd = socket(PF_INET,SOCK_STREAM,0);

    sockaddr_in user{};
    user.sin_family = AF_INET;
    user.sin_port = htons(TCP_PORT);

    inet_pton(AF_INET,ip.c_str(),&user.sin_addr);

    if(connect(sendfd, (struct sockaddr*)&user, sizeof(user)) < 0)
    {
        std::cout << "连接失败\n";
        close(sendfd);
        return;
    }

    send(sendfd, msg.c_str(), msg.size(), 0);

    close(sendfd);
}