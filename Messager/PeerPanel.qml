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
*
* Change Log:
* [v0.2.0]    2026-06-03
* * 左侧栏实现了选择用户功能（实现鼠标悬停变色，选择变色），并与顶部栏连通（通过Main.qml传递消息）
* * 搜索栏完成消息传递，但具体操作还未完成
*/


import QtQuick
import QtQuick.Controls

//左侧栏
Rectangle {
    id: peerPanel
    color: "#D9D9D9"

    //后续交给 PeerModel 或 AppController 处理
    //Main.qml给，控制左侧用户高亮
    property string currentPeerId: ""

    //后续交给PeerModel或AppController处理
    signal searchTextChanged(string keyword)

    //     [v0.1.2] HeZhiyuan    2026-06-03 16:24:01
    //         *新增点击用户后的操作
    //点击左侧用户后，向 Main.qml 通知当前选中的用户
    signal peerSelected(string peerId, string username, string ip)

    //     [v0.1.2] HeZhiyuan    2026-06-04 20:31:57
    //         * 仿照QQ,再次点击当前用户时，通知 Main.qml 关闭聊天窗口
    signal peerClosed()

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
            height: 56
            radius: 10

            //选中用户后改变背景色。
            color: peerPanel.currentPeerId === model.peerId ? "#E8F3FF" : "#F5F5F5"

            border.color: peerPanel.currentPeerId === model.peerId ? "#9BCBFF" : "#E0E0E0"
            border.width: 1

            anchors.horizontalCenter: parent.horizontalCenter

            //在线状态圆点
            Rectangle {
                id: statusDot

                width: 10
                height: 10
                radius: 5
                color: model.online ? "#00CA00" : "#B8B8B8"

                anchors.left: parent.left
                anchors.leftMargin: 12
                anchors.verticalCenter: parent.verticalCenter
            }

            //用户名
            Text {
                id: usernameText

                text: model.username
                font.pixelSize: 13
                font.bold: true
                color: "#1F2329"

                anchors.left: statusDot.right
                anchors.leftMargin: 10
                anchors.top: parent.top
                anchors.topMargin: 9
            }

            //IP 地址
            Text {
                id: ipText

                text: model.ip
                font.pixelSize: 12
                color: "#6B7280"

                anchors.left: usernameText.left
                anchors.top: usernameText.bottom
                anchors.topMargin: 5
            }

            //     [v0.1.2] HeZhiyuan    2026-06-03 13:36:08
            //         *   点击处理区域。TapHandler不是可视控件，它会处理当前delegate的点击。
            //     [v0.1.2] HeZhiyuan    2026-06-04 20:44:51
            //         * 实现类QQ，根据点击不同的用户，做出不同的操作
            TapHandler {
                id: tapHandler

                onTapped: {
                    //如果点击的是当前正在聊天的用户，则关闭聊天窗口，回到初始界面
                    if (peerPanel.currentPeerId === model.peerId) {
                        peerPanel.peerClosed()
                        console.log("关闭当前聊天窗口:", model.peerId, model.username)
                        return
                    }

                    //否则切换到该用户
                    peerPanel.peerSelected(model.peerId, model.username, model.ip)

                    //调试输出，后续可以删除
                    console.log("选择用户:", model.peerId, model.username, model.ip)
                }
            }
        }
    }
}