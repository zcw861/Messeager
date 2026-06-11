// Module
// File: main.cpp   Version: 0.1.0   License: AGPLv3
// Created:  ZhouChengWei     2026-06-09 19:28:41
// Description:
//      Messager（信使）项目是一个基于局域网的p2p聊天软件，实现私聊，群聊，传文件等功能
//      使用socket网络编程，c++封装，qml实现界面交互
//      实现轻量化设计
//
//     [v0.1.1]  ZhouChengWei    2026-06-11 21:46:24
//         * 将main.cpp修改为与qml交互

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include "privatechat.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    //创建后端对象
    PrivateChat privateChat;

    QQmlApplicationEngine engine;

    //把c++对象暴露给QML
    engine.rootContext()->setContextProperty("privateChat", &privateChat);

    //加载主QML文件
    const QUrl url = QUrl::fromLocalFile("../../main.qml");
    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreated,
        &app,
        [url](QObject *obj, const QUrl &objUrl) {
            if (!obj && url == objUrl) QCoreApplication::exit(-1);
        },
        Qt::QueuedConnection);

    engine.load(url);

    return app.exec();
}