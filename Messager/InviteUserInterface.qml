// Module
// File: InviteUserInterface.qml   Version: 0.1.0   License: AGPLv3
// Created: ZhouChengWei      2026-06-06 11:25:35
// Description:创建群聊邀请用户的界面
//
//     [v0.1.1]  ZhouChengWei   2026-06-06 12:38:11
//         * 添加取消和确定按钮。

import QtQuick
import QtQuick.Controls

Window {
    id: inviteUserInterface
    width: 600
    height: 500
    visible: false
    flags: Qt.FramelessWindowHint

    property bool userSelected: false

    Rectangle {
        anchors.fill: parent
        color: "white"

        Text {
            text: "这是新窗口"
            anchors.centerIn: parent
            font.pointSize: 16
        }

        //确定按钮
        Rectangle{
            id: okButton
            anchors.bottom: parent.bottom
            anchors.right: parent.right
            anchors.rightMargin: 20
            anchors.bottomMargin: 20
            width: 60;  height: 30;  radius: 10;
            //根据是否选择用户和悬停状态改变颜色
            color: {
            if (!inviteUserInterface.userSelected) {
                return "#e0e0e0"  //禁用状态的颜色
            } else {
                return okButtonHover.hovered ? "#00ffff" : "#87cefa"
                }
            }

            //透明度变化表示禁用状态
            opacity: inviteUserInterface.userSelected ? 1.0 : 0.6

            Text{
                anchors.centerIn: parent
                text: "确定"
                color: inviteUserInterface.userSelected ? "blue" : "#999999"
            }

            HoverHandler {
                id: okButtonHover
            //根据是否选择用户改变鼠标形状
                cursorShape: inviteUserInterface.userSelected ? Qt.PointingHandCursor : Qt.ForbiddenCursor
            }

        }

        //取消按钮
        Rectangle{
            id: cancelButton
            anchors.bottom: parent.bottom
            anchors.right: okButton.left
            anchors.rightMargin: 20
            anchors.bottomMargin: 20
            width: 60;  height: 30;  radius: 10;
            color: cancelButtonHover.hovered ? "#d3d3d3" : "white"
            border.color: "#d3d3d3";  border.width: 1
            Text{
                anchors.centerIn: parent
                text: "取消"
                color: "black"
            }

            TapHandler{
                onTapped: inviteUserInterface.close()
            }

            HoverHandler {
                id: cancelButtonHover
            }
        }
    }
}