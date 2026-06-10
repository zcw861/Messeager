#ifndef PEER_H
#define PEER_H

#include <string>
#include <unordered_map>
#include <mutex>

struct PeerInfo {
    std::string name;
    std::string ip;
};

extern std::unordered_map<std::string, PeerInfo> peers;
extern std::mutex peer_mutex;

void start_udp_broadcast(const std::string& username);
void start_udp_listener();
void start_tcp_server();
void send_message(const std::string& ip, const std::string& msg);

#endif