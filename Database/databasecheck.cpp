#include "databasecheck.h"
#include <QUuid>

QString DatabaseCheck::normalizePeerId(const QString &peerId)
{
    //先删除输入字符串首尾空白
    const QString trimmedPeerId = peerId.trimmed();

    if (trimmedPeerId.isEmpty()) {
        return {};
    }

    const QUuid parsedUuid = QUuid::fromString(trimmedPeerId);

    if (parsedUuid.isNull()) {
        return {};
    }

    //数据库统一去除UUID的大括号
    return parsedUuid.toString(QUuid::WithoutBraces);
}

bool DatabaseCheck::isValidGroupId(const QString &groupId)
{
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