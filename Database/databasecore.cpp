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

bool DatabaseCore::open()
{

    //清除旧错误
    m_lastError.clear();

    //如果当前对象创建的默认连接仍然存在且已经打开，直接返回成功
    if (m_connectionCreated && QSqlDatabase::contains()) {
        QSqlDatabase existingDatabase = QSqlDatabase::database();

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

    QSqlDatabase database;

    if (QSqlDatabase::contains()) {
        //重新取得连接句柄
        database = QSqlDatabase::database();
    } else {
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

    //SQLite默认可能没有启用外键，检查执行该语句，防止ON DELETE CASCADE不生效
    QSqlQuery foreignKeyQuery(database);

    if (!foreignKeyQuery.exec(QStringLiteral("PRAGMA foreign_keys = ON"))) {
        m_lastError = foreignKeyQuery.lastError().text();
        database.close();
        return false;
    }

    return true;
}

bool DatabaseCore::isOpen() const
{
    //没有创建连接或连接池中没有默认连接，就表示数据库不可用
    if (!m_connectionCreated || !QSqlDatabase::contains()) {
        return false;
    }

    const QSqlDatabase database = QSqlDatabase::database();

    return database.isOpen();
}

QSqlDatabase DatabaseCore::database() const
{
    //没有默认连接时返回无效的QSqlDatabase
    if (!m_connectionCreated || !QSqlDatabase::contains()) {
        return {};
    }

    return QSqlDatabase::database();
}

QString DatabaseCore::databasePath() const
{
    return m_databasePath;
}

QString DatabaseCore::lastError() const
{
    return m_lastError;
}

bool DatabaseCore::execute(const QString &sql, QString &errorMessage) const
{
    //防止调用方读取到上一次留下的错误
    errorMessage.clear();

    const QSqlDatabase currentDatabase = database();

    if (!currentDatabase.isValid() || !currentDatabase.isOpen()) {
        errorMessage = QStringLiteral("数据库未打开");

        return false;
    }

    //明确把默认连接传给QSqlQuery
    QSqlQuery query(currentDatabase);

    if (!query.exec(sql)) {
        errorMessage = query.lastError().text();
        return false;
    }

    return true;
}