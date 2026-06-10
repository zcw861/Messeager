#include "peer.h"

#include <iostream>
#include <thread>
#include <cstring>

#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define UDP_PORT 45454
#define TCP_PORT 45455
#define BUF_SIZE 1024

std::unordered_map<std::string, PeerInfo> peers;
std::mutex peer_mutex;

std::string local_name;

void start_udp_broadcast(const std::string& username)
{
    local_name = username;

    std::thread([](){

        int sock = socket(AF_INET, SOCK_DGRAM, 0);

        int broadcastEnable = 1;
        setsockopt(sock, SOL_SOCKET, SO_BROADCAST,&broadcastEnable, sizeof(broadcastEnable));

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(UDP_PORT);
        addr.sin_addr.s_addr = inet_addr("255.255.255.255");

        while(true)
        {
            sendto(sock,local_name.c_str(),local_name.size(),0,(sockaddr*)&addr,sizeof(addr));
            sleep(2);
        }

    }).detach();
}

void start_udp_listener()
{
    std::thread([](){

        int sock = socket(AF_INET, SOCK_DGRAM, 0);

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(UDP_PORT);
        addr.sin_addr.s_addr = INADDR_ANY;

        int reuse = 1;
        setsockopt(sock,SOL_SOCKET,SO_REUSEADDR,&reuse,sizeof(reuse));
        if(bind(sock, (sockaddr*)&addr, sizeof(addr)) < 0)
        {
            perror("udp bind failed");
            return;
        }

        char buf[BUF_SIZE];

        while(true)
        {
            sockaddr_in sender{};
            socklen_t len = sizeof(sender);
            memset(&sender, 0, sizeof(sender));

            int n = recvfrom(sock,buf,BUF_SIZE - 1,0,(sockaddr*)&sender,&len);
            std::string ip = inet_ntoa(sender.sin_addr);
            if (ip == "10.252.183.5") continue;

            if(n <= 0) continue;

            buf[n] = 0;

            {
                std::lock_guard<std::mutex> lock(peer_mutex);

                peers[ip] = {buf,ip};
            }
        }

    }).detach();
}

void start_tcp_server()
{
    std::thread([](){

        int server_fd = socket(AF_INET, SOCK_STREAM, 0);

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(TCP_PORT);
        addr.sin_addr.s_addr = INADDR_ANY;

        int reuse = 1;
        setsockopt(server_fd,SOL_SOCKET,SO_REUSEADDR,&reuse,sizeof(reuse));
        if(bind(server_fd,(sockaddr*)&addr,sizeof(addr)) < 0)
        {
            perror("tcp bind failed");
            return;
        }

        listen(server_fd, 10);

        while(true)
        {
            sockaddr_in client{};
            socklen_t len = sizeof(client);

            int client_fd = accept(server_fd,(sockaddr*)&client,&len);

            std::thread([client_fd, client](){

                char buf[BUF_SIZE];

                int n = recv(client_fd,buf,BUF_SIZE - 1,0);

                if(n > 0)
                {
                    buf[n] = 0;

                    std::cout << "\n====================\n";
                    std::cout << "来自 "<< inet_ntoa(client.sin_addr)<< " 的消息:\n";
                    std::cout << buf << std::endl;
                    std::cout << "====================\n";
                }

                close(client_fd);

            }).detach();
        }

    }).detach();
}

void send_message(const std::string& ip,
                  const std::string& msg)
{
    int sock = socket(AF_INET,SOCK_STREAM,0);

    sockaddr_in peer{};
    peer.sin_family = AF_INET;
    peer.sin_port = htons(TCP_PORT);

    inet_pton(AF_INET,ip.c_str(),&peer.sin_addr);

    if(connect(sock,(sockaddr*)&peer,sizeof(peer)) < 0)
    {
        std::cout << "连接失败\n";
        close(sock);
        return;
    }

    send(sock,msg.c_str(),msg.size(),0);

    close(sock);
}