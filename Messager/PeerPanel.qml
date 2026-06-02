/**
* @file    PeerPanel.qml
* @date    2026-06-02
* @author  JiangFan
* @brief   左侧用户列表
*
* Version: 0.1.0
* License: AGPLv3
* Created:  JiangFan 2026-06-02 22:11:46
*
*
* Change Log:
* [v0.1.0]    2026-06-02
* * Initial creation
*/

import QtQuick
import QtQuick.Controls

//左侧栏
Rectangle {
    id: peerPanel
    color: "#D9D9D9"

    //后续交给PeerModel或AppController处理
    signal searchTextChanged(string keyword)

    //目前先使用测试数据
    ListModel {
        id: testPeerModel

        ListElement {
            peerId: "01"
            username: "张三"
            ip: "192.168.1.1"
            online: true
        }

        ListElement {
            peerId: "02"
            username: "李斯"
            ip: "192.168.1.2"
            online: true
        }

        ListElement {
            peerId: "03"
            username: "王五"
            ip: "192.168.1.3"
            online: false
        }
    }




    Text {
        id: peerTitle
        text: qsTr("局域网用户")
        font.pixelSize: 15
        font.bold: true          //加粗
        color: "#333333"         //灰色

        anchors.left: parent.left
        anchors.leftMargin: 10   //左边留10px空隙差不多
        anchors.top: parent.top  //顶部一样
        anchors.topMargin: 10
        }

    //Search
    TextField {
        id: searchField

        width: parent.width - 20
        height: 30

        placeholderText: qsTr("搜索用户")
        font.pixelSize: 12

        anchors.left: parent.left
        anchors.leftMargin: 10
        anchors.top: peerTitle.bottom       //标题框下方
        anchors.topMargin: 10

        //Search background
        background: Rectangle {
            radius: 10
            color: "#F2F2F2"
            border.color: "#b4b4b4"
            border.width: 1
        }

        //当输入内容发生变化时，向外发出信号
        onTextChanged: {
            peerPanel.searchTextChanged(text)
        }
    }

    ListView {
        id: peerListView

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: searchField.bottom
        anchors.topMargin: 12
        anchors.bottom: parent.bottom

        clip: true
        spacing: 6

        model: testPeerModel

        delegate: Rectangle {
            width: peerListView.width - 20
            height: 50
            radius: 10
            color: mouseArea.containsMouse ? "#888888" : "#adadad"

            anchors.horizontalCenter: parent.horizontalCenter

            //在线状态的原点
            Rectangle {
                id: statusDot

                width: 10
                height: 10
                radius: 10
                color: model.online ? "#00ca00" : "#b8b8b8"

                anchors.left: parent.left
                anchors.leftMargin: 10
                anchors.top: parent.top
                anchors.topMargin: 15
            }

            //用户名
            Text {
                id: usernameText

                text: model.username
                font.pixelSize: 12
                font.bold: true
                color: "black"

                anchors.left: statusDot.right
                anchors.leftMargin: 10
                anchors.top: parent.top
                anchors.topMargin: 10
            }

            // IP 地址
            Text {
                id: ipText

                text:model.ip
                font.pixelSize: 12
                color: "#313131"

                anchors.left: usernameText.left
                anchors.top: usernameText.bottom
                anchors.topMargin: 5
            }

            //鼠标点击区域
            MouseArea {
                id: mouseArea

                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor

                onClicked: {
                    console.log("选择用户:", model.peerId, model.username)
                }
            }

        }
    }

}
