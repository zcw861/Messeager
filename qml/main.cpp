//     [v0.1.2] HeZhiyuan    2026-06-13 17:44:06
//         * 移除 main.cpp 对 DatabaseManager 的直接创建和初始化
//     [v0.1.3] ZhouChengWei    2026-07-02 23:36:00
//         * 添加了信号忽略，防止传文件时对面中途退出导致本机因为信号闪退

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <signal.h>

int main(int argc, char *argv[])
{
    signal(SIGPIPE,SIG_IGN);    //防止传文件时对面中途退出导致本机因为信号闪退

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
