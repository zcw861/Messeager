#include "peer.h"

#include <iostream>
#include <thread>
#include <chrono>

void show_peers()
{
    std::lock_guard<std::mutex> lock(peer_mutex);

    std::cout << "\n在线用户:\n";

    int i = 0;

    for(auto& p : peers)
    {
        std::cout << i++ << " : " << p.second.name << " (" << p.second.ip << ")\n";
    }

    std::cout << std::endl;
}

int main()
{
    std::string username;

    std::cout << "输入用户名: ";
    std::getline(std::cin, username);

    start_udp_broadcast(username);

    start_udp_listener();

    start_tcp_server();

    while(true)
    {
        std::cout << "\n";
        std::cout << "1. 查看在线用户\n";
        std::cout << "2. 发送消息\n";
        std::cout << "3. 退出\n";
        std::cout << "选择: ";

        int op;
        std::cin >> op;
        std::cin.ignore();

        if(op == 1)
        {
            show_peers();
        }
        else if(op == 2)
        {
            show_peers();

            std::string ip;
            std::string msg;

            std::cout << "输入对方IP: ";
            std::getline(std::cin, ip);

            std::cout << "输入消息: ";
            std::getline(std::cin, msg);

            send_message(ip, msg);
        }
        else if(op == 3)
        {
            break;
        }
    }

    return 0;
}