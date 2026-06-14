// Module
// File: InviteUserInterface.qml   Version: 0.1.0   License: AGPLv3
// Created: ZhouChengWei      2026-06-06 11:25:35
// Description:创建群聊邀请用户的界面
//
//     [v0.1.1]  ZhouChengWei   2026-06-06 12:38:11
//         * 添加取消和确定按钮。
//     [v0.1.2]  ZhouChengWei   2026-06-06 22:31:45
//         * 添加左侧用户列表。
//     [v0.1.3]  JiangFan       2026-06-06 22:31:45
//         * 实现搜索框搜索功能

import QtQuick
import QtQuick.Controls

Window {
    id: inviteUserInterface
    width: 600
    height: 500
    visible: false
    flags: Qt.FramelessWindowHint

    //当前搜索关键字
    property string searchKeyword: ""

    //当前已选择的用户数量，用来控制选择按钮
    property int selectCount: 0
    property bool userSelected: selectCount > 0

    //判断用户是否匹配关键字(跟PeerPanel一样）
    function matchSearch(username, ip)
    {
        if (searchKeyword.length == 0)
            return true

        var keyword = searchKeyword.toLowerCase()

        return username.toLowerCase().indexOf(keyword) !== -1
                || ip.toLowerCase().indexOf(keyword) !== -1
    }

    //修改用户选中状态
    function setUserSelected(row, selected)
    {
        var oldSelected = testPeerModel.get(row).selected

        if (oldSelected === selected)
            return

        testPeerModel.setProperty(row, "selected", selected)

        if (selected)
            selectCount++
        else
            selectCount--
    }


    ListModel {
        id: testPeerModel

        ListElement {
            peerId: "01"
            username: "张三"
            ip: "192.168.1.1"
            online: true
            selected: false
        }

        ListElement {
            peerId: "02"
            username: "李四"
            ip: "192.168.1.2"
            online: true
            selected: false
        }

        ListElement {
            peerId: "03"
            username: "王五"
            ip: "192.168.1.3"
            online: false
            selected: false
        }
    }

    Rectangle {
        anchors.fill: parent
        color: "white"

        Text {
            text: "邀请窗口"
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

        //搜索框
        TextField {
            id: searchField
            anchors.top: parent.top
            anchors.topMargin: 20
            anchors.left: parent.left
            anchors.right: leftInvateUser.right
            anchors.margins: 10
            height: 30
            placeholderText: "搜索用户..."

            background: Rectangle {
                id: searchBackGround
                radius: 5
                color: "#ffffff"
                border.color: searchField.activeFocus ? "#a6ceec" : "#e0e0e0"
            }

            //监控文本变化
            onTextChanged: {
                inviteUserInterface.searchKeyword = text.trim()
            }
        }


        //左侧用户列表
        Rectangle{
            id:leftInvateUser
            width: 200
            color: "white"
            anchors.bottom: parent.bottom
            anchors.top: searchField.bottom
            anchors.topMargin: 10
            // 右边的分割线
            Rectangle {
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                width: 1  //分割线宽度
                color: "#e0e0e0"  //分割线颜色
                opacity: 0.3
            }


            ListView{
                id: userListView
                anchors.fill: parent
                anchors.margins: 5
                clip: true
                model: testPeerModel

                delegate: Rectangle {
                    width: ListView.view.width - 20  //减去左右边距
                    radius: 10
                    anchors.horizontalCenter: parent.horizontalCenter
                    color: isIn.hovered ? "#d3d3d3" : "white"
                    border.color: "#e0e0e0"
                    border.width: 1

                    //当前用户是否匹配搜索关键字
                    property bool matched: inviteUserInterface.matchSearch(model.username, model.ip)

                    //不匹配-->隐藏
                    height: matched ? 45 : 0
                    visible: matched
                    enabled: matched


                    //用户前面的圆圈（表示是否已经被选择)
                    Rectangle{
                        width: 20;  height: 20;  radius: 20
                        anchors.left: parent.left
                        anchors.leftMargin: 5
                        anchors.verticalCenter: parent.verticalCenter
                        color: model.selected ? "blue" : "white"
                        border{
                            width: 1; color: "#eae8e8"
                        }
                    }

                    Text {
                        anchors.centerIn: parent
                        text: model.username
                        font.pointSize: 12
                        color: "#333333"
                    }

                    HoverHandler{
                        id: isIn
                    }

                    TapHandler{
                        id: isSelected
                        onTapped: {
                            inviteUserInterface.setUserSelected(index, !model.selected)
                        }

                    }
                }

                ScrollBar.vertical: ScrollBar {
                    anchors.right: parent.right
                    width: 6
                    policy: ScrollBar.AsNeeded

                    contentItem: Rectangle {
                        implicitWidth: 4
                        radius: 2
                        color: "#cccccc"
                    }
                }
            }
        }

        //右侧已经选择的用户
        Rectangle{
            anchors.left: leftInvateUser.right
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.bottom: okButton.top
            anchors.bottomMargin: 10

            Text {
                id: crateGroup
                text: qsTr("创建群聊")
                color: "black"
                anchors.left:searchField.right
                anchors.leftMargin: 5
            }
        }
    }
}