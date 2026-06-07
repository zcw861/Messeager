// Module
// File: InputPanel.qml   Version: 0.1.0   License: AGPLv3
// Created: HeZhiyuan      2026-06-05 20:45:54
// Description:输入框：文本输入+发送按钮，输入框从ChatPanel.qml中拆出，单独负责输入和发送请求。
//
//     [v0.1.1] ZhouChengWei    2026-06-06 21:17:26
//         * 修改了输入框颜色，添加了鼠标在发送按钮悬停样式。
import QtQuick
import QtQuick.Controls

Item {
    id: root

    //MessageWindow.qml传当前聊天对象id
    property string currentPeerId: ""

    //输入框向外通知：请求发送一条消息
    signal sendRequested(string content)

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

    //底部输入栏
    Rectangle {
        id: inputBar

        height: 200
        color: "#FFFFFF"

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        //输入栏顶部分割线
        Rectangle {
            height: 1
            color: "#E5E5E5"

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
        }



        TextArea {
            id: inputArea

            placeholderText: qsTr("请输入消息......")
            font.pixelSize: 14
            wrapMode: TextEdit.Wrap

            anchors.left: parent.left
            anchors.leftMargin: 15
            anchors.right: parent.right  //让他包含发送按钮
            anchors.rightMargin: 10
            anchors.top: parent.top
            anchors.topMargin: 10
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 10

            background: Rectangle {
                radius: 8
                color: "white"
                border.width: 1
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

        //发送信息按钮
        Rectangle {
            id: sendButton

            width: 60
            height: 30
            radius: 10

            //当前是否允许发送
            property bool canSend: root.currentPeerId !== "" && inputArea.text.trim().length > 0

            color: canSend ? "#12B7F5" : "#C9CDD4"

            anchors.right: parent.right
            anchors.rightMargin: 15
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 15

            Text {
                text: qsTr("发送")
                color: "#FFFFFF"
                font.pixelSize: 14
                font.bold: true
                anchors.centerIn: parent
            }

            TapHandler {
                enabled: sendButton.canSend
                gesturePolicy: TapHandler.ReleaseWithinBounds

                onTapped: {
                    root.trySendMessage()
                }
            }

            HoverHandler{
                id :eableSend
                cursorShape: Qt.ArrowCursor
            }
        }
    }
}