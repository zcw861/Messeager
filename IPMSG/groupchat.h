// Module
// File: groupchat.h   Version: 0.1.0   License: AGPLv3
// Created: ZhouChengWei      2026-06-22 16:29:25
// Description:
//     群聊相关的函数，在比较了组播与UDP单播之后决定使用单播来实现群聊邀请
//     组播在跨子网时才会有优势，但本项目基于纯局域网p2p,所以现有架构来说UDP单播更有优势

#pragma once

#include <QObject>

#include <mutex>
#include <vector>
#include <unordered_map>
#include <string>

#include "common.h"

class GroupChat : public QObject
{
    Q_OBJECT

public:
    explicit GroupChat(QObject *parent = nullptr);
    ~GroupChat();

private:
    mutable std::mutex m_mutex;
    std::string m_localGroupId; //群ID
    std::string m_localGroupName; //群名称（默认为用户名字拼接，可改）
    std::unordered_map<std::string, std::vector<UserInfo>> m_groupMembers; //群号与群成员的映射
};
