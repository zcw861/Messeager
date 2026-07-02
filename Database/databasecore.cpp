// Module
// File: databasecore.cpp   Version: 0.1.0   License: AGPLv3
// Created: HeZhiyuan      2026-06-24
// Description: 添加创建、打开和访问默认数据库连接函数
//
#include "databasecore.h"

#include <QDir>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>

DatabaseCore::~DatabaseCore()
{
    //如果当前对象没有创建过连接，就不处理，防止析构函数误关闭其他模块创建的连接
    if (!m_connectionCreated || !QSqlDatabase::contains()) {
        return;
    }

    //取得默认连接
    QSqlDatabase database = QSqlDatabase::database();

    if (database.isOpen()) {
        database.close();
    }
}

//创建或使用SQLite连接
bool DatabaseCore::open()
{

    //清除旧错误
    m_lastError.clear();

    //如果当前对象创建的默认连接仍然存在且已经打开，直接返回成功
    if (m_connectionCreated && QSqlDatabase::contains()) {
        QSqlDatabase existingDatabase = QSqlDatabase::database();

        //确认已有连接是否已经打开
        if (existingDatabase.isOpen()) {
            m_databasePath = existingDatabase.databaseName();
            return true;
        }
    }

    //检查是否有SQLite驱动
    if (!QSqlDatabase::isDriverAvailable(QStringLiteral("QSQLITE"))) {
        m_lastError = QStringLiteral("当前没有可用的QSQLITE驱动");
        return false;
    }

    //AppDataLocation取得自己的数据目录
    const QString dataDirectory = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);

    if (dataDirectory.isEmpty()) {
        m_lastError = QStringLiteral("无法取得应用数据目录");
        return false;
    }


    //mkpath()会递归创建缺失的目录，如果目录已经存在，返回true
    if (!QDir().mkpath(dataDirectory)) {
        m_lastError = QStringLiteral("无法创建数据目录：") + dataDirectory;
        return false;
    }

    //使用filePath()拼接路径
    m_databasePath = QDir(dataDirectory).filePath(QStringLiteral("messager.db"));

    //报告错误，而不是覆盖旧连接
    if (QSqlDatabase::contains() && !m_connectionCreated) {
        m_lastError = QStringLiteral("默认数据库连接已经被其他代码创建；");
        return false;
    }

    //声明数据库连接句柄
    QSqlDatabase database;

    //判断默认连接是否已经存在
    if (QSqlDatabase::contains()) {
        //重新取得连接句柄
        database = QSqlDatabase::database();
    } else {
        //创建使用QSQLITE驱动的默认连接
        database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"));
        m_connectionCreated = true;
    }


    //对SQLite来说，databaseName是数据库文件路径
    database.setDatabaseName(m_databasePath);
    //真正打开SQLite文件
    if (!database.open()) {
        m_lastError = database.lastError().text();
        return false;
    }
    //创建绑定当前连接的查询对象，确保PRAGMA在正确的SQLite连接上执行
    QSqlQuery foreignKeyQuery(database);
    //SQLite默认可能没有启用外键，检查执行该语句，防止ON DELETE CASCADE不生效
    if (!foreignKeyQuery.exec(QStringLiteral("PRAGMA foreign_keys = ON"))) {
        m_lastError = foreignKeyQuery.lastError().text();
        database.close();
        return false;
    }

    return true;
}

//判断当前对象的默认数据库连接是否已经打开
bool DatabaseCore::isOpen() const
{
    //没有创建连接或连接池中没有默认连接，就表示数据库不可用
    if (!m_connectionCreated || !QSqlDatabase::contains()) {
        return false;
    }
    //取得默认连接句柄
    const QSqlDatabase database = QSqlDatabase::database();

    return database.isOpen();
}

//返回当前对象管理的数据库连接句柄
QSqlDatabase DatabaseCore::database() const
{
    //没有默认连接时返回无效的QSqlDatabase
    if (!m_connectionCreated || !QSqlDatabase::contains()) {
        return {};
    }

    return QSqlDatabase::database();
}

//返回当前数据库文件完整路径
QString DatabaseCore::databasePath() const
{
    return m_databasePath;
}

QString DatabaseCore::lastError() const
{
    return m_lastError;
}

//执行一条无需参数绑定的SQL
bool DatabaseCore::execute(const QString &sql, QString &errorMessage) const
{
    //防止调用方读取到上一次留下的错误
    errorMessage.clear();
    //通过database()取得当前连接
    const QSqlDatabase currentDatabase = database();
    //验证连接句柄有效并且数据库已打开
    if (!currentDatabase.isValid() || !currentDatabase.isOpen()) {
        errorMessage = QStringLiteral("数据库未打开");

        return false;
    }

    //绑定当前连接的查询对象
    QSqlQuery query(currentDatabase);

    //执行传入的SQL指令
    if (!query.exec(sql)) {
        errorMessage = query.lastError().text();
        return false;
    }

    return true;
}