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

import QtQuick
import QtQuick.Controls

Rectangle{
    id: root

    //Main.qml传入
    property string currentPeerId: ""
    property string currentPeerName: "请选择用户"

    color: "#FFFFFF"

    //发送消息
    function trysendMessage() {
        var content = inputArea.text.trim()

        if (root.currentPeerId === "")
        {
            console.log("请先选择聊天对象")
            return
        }

        if (content.length === 0)
            return

        //目前先这样，后面再改
        console.log("发送给", root.currentPeerId, "内容", content)

        inputArea.text = ""
    }




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
        anchors.bottom: inputBar.top //编辑框上方

        color: "#F5F5F5"

        Text {
            text: qsTr("暂无消息")
            color: "#999999"
            font.pixelSize: 15
            anchors.centerIn: parent
        }
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
                color: "#D0D0D0"
                border.color: "red"
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
                root.trysendMessage()
                event.accepted = true

            }
        }

        //发送信息按钮
        Button {
            id: sendButton

            width:60
            height: 30
            text: qsTr("发送")

            anchors.right: parent.right
            anchors.rightMargin: 15
            //anchors.verticalCenter: parent.verticalCenter
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 15


            enabled: root.currentPeerId !== "" && inputArea.text.trim().length > 0

            background: Rectangle{
                radius: 10
                color: sendButton.enabled ? "#12B7F5" : "#C9CDD4"
            }


            contentItem: Text {
                text: sendButton.text
                color: "#FFFFFF"
                font.pixelSize: 14
                font.bold: true
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }



            onClicked: {
                //console.log("发送给:", currentPeerId, "内容", inputArea.text)
                //inputArea.text = ""
                //改成直接调用函数
                root.trysendMessage()

            }
        }

    }


}

