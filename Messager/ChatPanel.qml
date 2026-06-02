/**
* @file    ChatPanel.qml
* @date    2026-06-01
* @author  JiangFan
* @brief   右侧聊天面板
*
* Version: 0.1.0
* License: AGPLv3
* Created:  JiangFan 2026-06-01 22:41:47
*
*
* Change Log:
* [v0.1.0]    2026-06-01
* * Initial creation
*/

import QtQuick


Rectangle{
    id: root

    //Main.qml传入
    property string currentPeerId: ""
    property string currentPeerName: "请选择用户"

    color: "#FFFFFF"


    //顶部栏
    Rectangle {
        id: chatHeader
        height: 50

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top

        color: "#FFFFFF"



        //分割线
        border.color: "#333333"
        border.width: 1

        Text {
            id: chatTitle
            text: currentPeerName

            font.pixelSize: 15
            font.bold: true
            color: "#333333"

            anchors.left: parent.left
            anchors.leftMargin: 15
            anchors.verticalCenter: parent.verticalCenter
        }
    }

    //消息显示
    Rectangle {
        id: messageArea

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: chatHeader.bottom
        anchors.bottom: parent.bottom

        color: "#FAFAFA"

        Text {
            text: qsTr("暂无消息")
            color: "#999999"
            font.pixelSize: 15
            anchors.centerIn: parent
        }
    }
}

