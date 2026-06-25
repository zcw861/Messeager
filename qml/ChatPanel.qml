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
* [v0.1.0] JiangFan   2026-06-01
* * Initial creation
*
* [v0.2.0] JiangFan   2026-06-03
* * 实现聊天框的发送消息，增加enter, ctrl + enter事件处理
*
* [v0.2.2] HeZhiyuan  2026-06-05 22:39:59
* * 增加聊天消息前端显示功能，发送消息后，当前聊天窗口会立即显示自己发送的内容。
*
* [v0.2.3] HeZhiyuan  2026-06-07 13:57:18
* * 修改部分ui颜色
*
* [v0.2.4] HeZhiyuan  2026-06-13 17:47:01
* * 消息列表改为接收AppController提供的QVariantList。
* * delegate使用modelData.fromMe和modelData.content。
*
* [v0.2.5] JiangFan   2026-06-20
* * 重构：使用Layout管理主窗口结构
* * 修复：消息滚动条和消息内容重合的bug
* [v0.2.6] HeZhiyuan   2026-06-25
* * 使用私聊消息模型和群聊消息模型显示当前会话历史
* * 群聊中显示其他成员的发送者名称
*/

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle{
    id: root

    //Window.qml传入
    property string currentPeerId: ""
    property string currentPeerName: "请选择用户"
    property var messageModel: null //MessageWindow.qml 传入的消息模型
    property bool isGroupChat: false

    color: "#FFFFFF"

    ColumnLayout {
        id: chatLayout

        anchors.fill: parent
        spacing: 0

        //顶部栏
        Rectangle {
            id: chatHeader

            Layout.preferredHeight: 50
            Layout.fillWidth: true

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

            Layout.fillWidth: true
            Layout.fillHeight: true

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

                delegate: ColumnLayout {
                    id: messageDelegate

                    required property var modelData

                    readonly property bool fromMe: Boolean(modelData.fromMe)
                    readonly property string content: String(modelData.content)
                    readonly property string senderName: modelData.senderName ? String(modelData.senderName) : ""

                    width: messageList.width
                    spacing: 3

                    //私聊不显示发送者名称
                    //群聊中自己的消息不重复显示自己名称
                    Text {
                        visible: root.isGroupChat && !messageDelegate.fromMe
                        text: messageDelegate.senderName
                        Layout.alignment: Qt.AlignLeft

                        color: "#6B7280"
                        font.pixelSize: 11
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 0

                        //自己发送时，将气泡推向右侧。
                        Item {
                            visible: messageDelegate.fromMe

                            Layout.fillWidth: true
                        }

                        Rectangle {
                            id: messageBubble

                            Layout.maximumWidth: messageList.width * 0.7

                            Layout.preferredWidth: Math.min(messageText.implicitWidth + 24, messageList.width * 0.7)

                            Layout.preferredHeight: messageText.implicitHeight + 18

                            radius: 8

                            color:messageDelegate.fromMe ? "#12B7F5" : "#FFFFFF"

                            Text {
                                id: messageText

                                width: Math.min(implicitWidth, messageList.width * 0.7 - 24)

                                height: implicitHeight

                                x: 12
                                y: 9

                                text: messageDelegate.content

                                font.pixelSize: 14

                                color: messageDelegate.fromMe ? "#FFFFFF" : "#222222"

                                wrapMode: Text.Wrap
                            }
                        }

                        //他人发送时，将气泡保留在左侧。
                        Item {
                            visible: !messageDelegate.fromMe

                            Layout.fillWidth: true
                        }
                    }
                }

                //提供右侧垂直滚动条，用来显示当前滚动位置
                ScrollBar.vertical: ScrollBar {
                    parent: messageArea   //这一句很必要，不加的话，它下面都没用，会被messageList自动管理
                    anchors.top: messageList.top
                    anchors.bottom: messageList.bottom
                    anchors.left: messageList.right
                    anchors.leftMargin: 4 //刚好贴到最边上，不会和消息重合
                }
            }
        }


    }
}

