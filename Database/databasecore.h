// Module
// File: databasecore.h   Version: 0.1.0   License: AGPLv3
// Created: HeZhiyuan      2026-06-24 13:20:22
// Description: 只负责创建、打开和访问默认数据库连接
//
#pragma once

#include <QSqlDatabase>
#include <QString>

#include "database_global.h"

class DATABASE_EXPORT DatabaseCore final
{
public:
    //使用编译器生成的默认构造函数
    DatabaseCore() = default;

    //程序退出时关闭默认连接
    ~DatabaseCore();

    //DatabaseCore负责唯一连接，不能被复制，如果允许复制，可能出现两个对象都认为自己负责关闭连接
    DatabaseCore(const DatabaseCore &) = delete;
    DatabaseCore &operator=(const DatabaseCore &) = delete;

    //创建并打开应用默认SQLite连接
    bool open();

    //判断默认连接当前是否已经打开
    bool isOpen() const;

    //取得已经存在的默认连接
    QSqlDatabase database() const;

    //返回messager.db的完整路径
    QString databasePath() const;

    //返回最近一次连接操作产生的错误
    QString lastError() const;

    //执行不包含参数占位符的SQL
    bool execute(const QString &sql, QString &errorMessage) const;

private:
    //保存数据库文件完整路径，不保存QSqlDatabase成员副本
    QString m_databasePath;

    //保存最近一次连接操作错误
    QString m_lastError;

    //标记默认连接是不是由当前对象创建
    bool m_connectionCreated{false};
};