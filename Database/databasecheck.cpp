// Module
// File: databasecheck.cpp   Version: 0.1.0   License: AGPLv3
// Created: HeZhiyuan      2026-06-24
// Description:     添加数据校验函数
//
#include "databasecheck.h"
#include <QUuid>

//把任意输入peerId规范化为不带大括号的标准UUID字符串，无效输入返回空字符串
QString DatabaseCheck::normalizePeerId(const QString &peerId)
{
    //先删除输入字符串首尾空白
    const QString trimmedPeerId = peerId.trimmed();

    //拒绝空字符串
    if (trimmedPeerId.isEmpty()) {
        return {};
    }

    //统一处理UUID格式并拒绝非法字符
    const QUuid parsedUuid = QUuid::fromString(trimmedPeerId);

    if (parsedUuid.isNull()) {
        return {};
    }

    //数据库统一去除UUID的大括号
    return parsedUuid.toString(QUuid::WithoutBraces);
}

//检查群ID在去除首尾空白后是否为十位数字
bool DatabaseCheck::isValidGroupId(const QString &groupId)
{
    //去除群ID首尾空白
    const QString normalizedGroupId = groupId.trimmed();

    //群ID必须刚好有十个字符
    if (normalizedGroupId.size() != 10) {
        return false;
    }

    //逐个检查字符
    for (const QChar character : normalizedGroupId) {
        if (character < QLatin1Char('0') || character > QLatin1Char('9')) {
            return false;
        }
    }

    return true;
}

//把消息读取数量控制在默认值和最大值之间
int DatabaseCheck::normalizeMessageLimit(int limit)
{
    constexpr int defaultLimit = 5000;
    constexpr int maximumLimit = 9999;

    if (limit <= 0) {
        return defaultLimit;
    }

    if (limit > maximumLimit) {
        return maximumLimit;
    }

    return limit;
}