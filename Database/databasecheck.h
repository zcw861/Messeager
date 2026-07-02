// Module
// File: databasecheck.h   Version: 0.1.0   License: AGPLv3
// Created: HeZhiyuan      2026-06-24 14:19:53
// Description: 定义数据库层统一使用的数据校验和规范化工具
//
#pragma once
#include <QString>

#include "database_global.h"
class DATABASE_EXPORT DatabaseCheck final
{
public:
    //校验并规范化UUID
    static QString normalizePeerId(const QString &peerId);

    //检查群ID是否严格为十位数字
    static bool isValidGroupId(const QString &groupId);

    //限制一次读取的聊天记录数量
    static int normalizeMessageLimit(int limit);

private:
    //该类只提供静态函数，不需要创建对象
    DatabaseCheck() = delete;
};
