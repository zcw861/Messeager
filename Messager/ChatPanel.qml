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
*
* [v0.2.0]    2026-06-03
* * 实现聊天框的发送消息，增加enter, ctrl + enter事件处理
*/
//     [v0.1.2] HeZhiyuan    2026-06-05 22:39:59
//         * 增加聊天消息前端显示功能，发送消息后，当前聊天窗口会立即显示自己发送的内容。
import QtQuick
import QtQuick.Controls

Rectangle{
    id: root

    //MessageWindow.qml传入
    property string currentPeerId: ""
    property string currentPeerName: "请选择用户"
    property var messageModel: null //MessageWindow.qml 传入的消息模型

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
        Rectangle {
            height: 1
            color: "#E5E5E5"

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
        }

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
        anchors.bottom: parent.bottom //编辑框上方

        color: "#F5F5F5"

        //没有消息时的提示
        Text {
            text: qsTr("暂无消息")
            color: "#999999"
            font.pixelSize: 15
            anchors.centerIn: parent

            visible: messageList.count === 0
        }

        //消息列表
        ListView {
            id: messageList

            anchors.fill: parent
            anchors.margins: 12

            clip: true
            spacing: 8

            model: root.messageModel

            //每新增一条消息，自动滚动到底部
            onCountChanged: {
                Qt.callLater(function() {
                    messageList.positionViewAtEnd()
                })
            }

            delegate: Item {
                width: messageList.width
                height: messageBubble.height + 8

                //使用 required property 接收 ListModel 角色数据
                required property bool fromMe
                required property string content

                Rectangle {
                    id: messageBubble

                    width: Math.min(messageText.implicitWidth + 24, messageList.width * 0.7)
                    height: messageText.implicitHeight + 18

                    //自己发送的消息靠右
                    x: fromMe ? parent.width - width : 0

                    radius: 8
                    color: fromMe ? "#9EEA6A" : "#FFFFFF"

                    Text {
                        id: messageText

                        text: content
                        font.pixelSize: 14
                        color: "#1F2329"

                        width: Math.min(implicitWidth, messageList.width * 0.7 - 24)
                        wrapMode: Text.Wrap

                        anchors.left: parent.left
                        anchors.leftMargin: 12
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }
            }

            ScrollBar.vertical: ScrollBar { }
        }
    }


}

