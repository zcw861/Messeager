// Module
// File: InputPanel.qml   Version: 0.1.0   License: AGPLv3
// Created: HeZhiyuan      2026-06-05 20:45:54
// Description:输入框：文本输入+发送按钮，输入框从ChatPanel.qml中拆出，单独负责输入和发送请求。
//
//     [v0.1.1] ZhouChengWei    2026-06-06 21:17:26
//         * 修改了输入框颜色，添加了鼠标在发送按钮悬停样式。
//     [v0.1.2] HeZhiyuan    2026-06-07 13:57:38
//         * 修改部分ui颜色
//     [v0.1.3] JiangFan    2026-06-14
//         *  增加文件发送按钮、文件选择对话框
//     [v0.1.4] JiangFan    2026-06-15
//         *  更改消息发送框（文件、发送按钮、文本框）的形式，增加ScrollView,支持输入框滑动
//     [v0.1.5] JiangFan    2026-06-20
//         *  重构：使用Layout管理主窗口结构
//     [v0.1.6] HeZhiyuan   2026-06-25
//         * 输入面板支持私聊ID和群聊ID作为统一的当前会话ID
//           统一空消息检查、发送请求和发送后清空输入框的处理流程

import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

Item {
    id: root

    //MessageWindow.qml传当前聊天对象id
    property string currentPeerId: ""

    //群聊当前没有群文件传输协议时，Window.qml传入false。
    property bool fileSendingEnabled: true

    //输入框向外通知：请求发送一条消息
    signal sendRequested(string content)
    //输出框向外通知: 请求发送一个文件
    signal fileSendRequested(url fileUrl)

    //统一处理发送逻辑：检查当前会话和输入内容
    function trySendMessage() {
        var content = inputArea.text.trim()

        if (root.currentPeerId === "") {
            console.log("请先选择聊天对象")
            return
        }

        if (content.length === 0)
            return

        //把内容发给 Window.qml
        root.sendRequested(content)

        //发送后清空输入框
        inputArea.text = ""
    }

    //清空输入框
    function clear() {
        inputArea.text = ""
    }

    //文件选择对话框
    FileDialog {
        id: fileDialog

        title: qsTr("请选择要发送的文件")
        fileMode:  FileDialog.OpenFile
        nameFilters: [qsTr("所有文件(*)")]

        onAccepted:  {
            if (selectedFile.toString().length === 0)
                return

            console.log("inputPanel 已选择文件:", selectedFile)

            //把文件发送请求交给Window.qml
            root.fileSendRequested(selectedFile)
        }
    }

    //底部输入栏
    Rectangle {
        id: inputBar

        color: "#FFFFFF"
        anchors.fill: parent


        ColumnLayout {
            id: inputLayout

            anchors.fill: parent
            spacing: 0

            //输入栏顶部分割线
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 1

                color: "#E5E5E5"
            }

            //输入工具栏：文件、表情、截图等功能入口
            Rectangle {
                id: toolBar

                Layout.fillWidth: true
                Layout.preferredHeight: 38

                color: "#FFFFFF"

                //文件按钮
                Rectangle {
                    id: fileButton

                    width: 50
                    height: 30
                    radius: 10

                    enabled: root.currentPeerId !== "" && root.fileSendingEnabled
                    opacity: enabled ? 1 : 0.5

                    color: fileButtonHover.hovered && fileButton.enabled ? "#F2F3F5" : "#FFFFFF"
                    //border.color: "#DCDCDC"
                    //border.width: 1

                    anchors.left: parent.left
                    anchors.leftMargin: 10
                    anchors.verticalCenter: parent.verticalCenter

                    Image {
                        source: "source/file.svg"
                        width: 20
                        height: 20
                        anchors.centerIn: parent
                        fillMode: Image.PreserveAspectFit
                    }

                    HoverHandler {
                        id: fileButtonHover
                        cursorShape: fileButton.enabled ? Qt.PointingHandCursor : Qt.ForbiddenCursor
                    }

                    TapHandler {
                        enabled: fileButton.enabled
                        acceptedButtons: Qt.LeftButton
                        gesturePolicy: TapHandler.ReleaseWithinBounds

                        onTapped: {
                            fileDialog.open()
                        }
                    }
                }
            }

           Item {
               id: inputAreaBox

               Layout.fillWidth: true
               Layout.fillHeight: true

               ScrollView {
                   id: inputScrollView

                   anchors.fill: parent
                   anchors.leftMargin: 15
                   anchors.rightMargin: 10
                   anchors.topMargin: 2
                   anchors.bottomMargin: 2

                   clip: true

                   ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                   ScrollBar.vertical.policy: ScrollBar.AsNeeded

                   TextArea {
                       id: inputArea

                       placeholderText: qsTr("请输入消息......")
                       font.pixelSize: 14
                       wrapMode: TextEdit.Wrap

                       background: Rectangle {
                           radius: 8
                           color: "white"
                           //border.color: "black"
                           //border.width: 1
                       }

                       //一个处理回车事件的函数
                       // enter or send(press send Button) --> 发送
                       // ctrl + enter--> 换行
                       Keys.onPressed: function(event) {

                           //没有按按钮且没有回车
                           if (!(event.key === Qt.Key_Return || event.key === Qt.Key_Enter))
                               return

                           //ctrl + enter : 换行
                           if ((event.modifiers & Qt.ControlModifier) !== 0)
                           {
                               inputArea.insert(inputArea.cursorPosition, "\n")
                               event.accepted = true
                               return
                           }

                           //enter or send
                           root.trySendMessage()
                           event.accepted = true

                       }
                   }
               }
           }

        Item {
            id: bottomBar

            Layout.fillWidth: true
            Layout.preferredHeight: 40

            //发送信息按钮
            Rectangle {
                id: sendButton

                width: 60
                height: 30
                radius: 10

                //当前是否允许发送
                property bool canSend: root.currentPeerId !== "" && inputArea.text.trim().length > 0

                color: !canSend
                        ? "#C9CDD4"     //不能发送：灰色
                        : sendTap.pressed ? "#9AA4AF" //按下瞬间：变灰
                                          : sendButtonHover.hovered ? "#0E9FE6" //鼠标悬停：深蓝
                                                                    : "#12B7F5" //默认可发送：浅蓝

               anchors.right: parent.right
               anchors.rightMargin: 15
               anchors.verticalCenter: parent.verticalCenter

                Text {
                    text: qsTr("发送")
                    color: "#FFFFFF"
                    font.pixelSize: 14
                    font.bold: true
                    anchors.centerIn: parent
                }

                TapHandler {
                    id: sendTap

                    enabled: sendButton.canSend
                    gesturePolicy: TapHandler.ReleaseWithinBounds

                    onTapped: {
                        root.trySendMessage()
                    }
                }

                HoverHandler{
                    id :sendButtonHover

                    //改变悬停鼠标样式
                    cursorShape: sendButton.canSend ? Qt.PointingHandCursor : Qt.ForbiddenCursor
                }
            }

        }

        }
    }
}