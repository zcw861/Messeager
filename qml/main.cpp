//     [v0.1.2] HeZhiyuan    2026-06-13 17:44:06
//         * 移除 main.cpp 对 DatabaseManager 的直接创建和初始化
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <signal.h>

int main(int argc, char *argv[])
{
    signal(SIGPIPE, SIG_IGN);

    QGuiApplication app(argc, argv);

    QCoreApplication::setOrganizationName("se.qt.messager");
    QCoreApplication::setApplicationName("se.qt.messager");

    QQmlApplicationEngine engine;
    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);
    engine.loadFromModule("se.qt.messager", "Window");

    return QGuiApplication::exec();
}
