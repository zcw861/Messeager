#include <QGuiApplication>
#include <QQmlApplicationEngine>

#include "databasemanager.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    QCoreApplication::setOrganizationName("Messager");
    QCoreApplication::setApplicationName("Messager");

    DatabaseManager database;

    if (!database.open()) {
        qWarning() << "数据库打开失败:" << database.lastError();
        return -1;
    }

    if (!database.initSchema()) {
        qWarning() << "数据库初始化失败:" << database.lastError();
        return -1;
    }

    qInfo() << "SQLite 数据库路径:" << database.databasePath();

    QQmlApplicationEngine engine;
    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);
    engine.loadFromModule("Messager", "Window");

    return QGuiApplication::exec();
}
