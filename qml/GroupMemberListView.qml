/**
* @file    PeerPanel.qml
* @date    2026-06-30
* @author  JiangFan
* @brief   群聊用户列表
*
* Version: 0.1.0
* License: AGPLv3
* Created:  JiangFan 2026-06-30
*
*
* Change Log:
* [v0.1.0]  JiangFan  2026-06-30
* * Initial creation
*/

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

//成员列表
ListView {
    id: groupMemberListView

    Layout.fillWidth: true
    Layout.fillHeight: true

    clip: true
    model: appController.groupMembers

    delegate: Rectangle {
        required property var modelData

        //从C++群成员模型读取显示所需字段
        readonly property string peerId: String(modelData.peerId)
        readonly property string username: String(modelData.username)
        readonly property string ip: String(modelData.ip)
        readonly property bool online: Boolean(modelData.online)
        readonly property bool isSelf: Boolean(modelData.isSelf)

        id: memberDelegate

        property bool showMemberInfo: false //是否显示小弹窗（成员信息卡片），由悬浮以及悬浮时间决定

        width: groupMemberListView.width
        height: 50

        color: memberHover.hovered? "#F2F3F5" : "transparent"

        HoverHandler {
            id: memberHover

            onHoveredChanged: {
                if (hovered)
                    memberHoverTimer.restart()
                else {
                    memberHoverTimer.stop()
                    memberDelegate.showMemberInfo = false
                }
            }
        }

        Timer {
            id: memberHoverTimer
            interval: 2000
            repeat: false

            onTriggered: {
                if (memberHover.hovered)
                    memberDelegate.showMemberInfo = true
            }
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 10
            anchors.rightMargin: 10
            spacing: 10

            //头像
            Rectangle {
                Layout.preferredHeight: 30
                Layout.preferredWidth: 30
                Layout.alignment: Qt.AlignVCenter

                radius:20
                color: isSelf ? "#D8ECFF" : "#EEEEEE"

                Text {
                    text: username.length > 0 ? username.charAt(0) : "?"
                    font.pixelSize: 15
                    color: "green"

                    anchors.centerIn: parent
                }
            }

            //名字和状态
            ColumnLayout {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignVCenter
                spacing: 1

                Text {
                    Layout.fillWidth: true

                    text: username
                    font.pixelSize: 15
                    color: "#676767"
                    elide: Text.ElideRight
                }

            }
        }

        //小弹窗（群成员信息卡片)
        Popup {
            id: memberInfoPopup

            visible: memberDelegate.showMemberInfo //悬浮 + 悬浮时间共同决定显示

            //在列表左边
            x: -width
            y: 0

            width: 200
            height: 100

            background: Rectangle {
                color: "#FFFFFF"
                radius: 10
            }

            //弹窗卡片布局
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 10
                spacing: 5

                RowLayout {
                    id: memberMessageLayout
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    //头像
                    Rectangle {
                        Layout.preferredHeight: 60
                        Layout.preferredWidth: 60
                        Layout.alignment: Qt.AlignVCenter

                        radius: 999
                        color: isSelf ? "#D8ECFF" : "#EEEEEE"

                        Text {
                            text: username.length > 0 ? username.charAt(0)
                                                      : "?"
                            font.pixelSize: 40
                            color: "green"
                            anchors.centerIn: parent
                        }
                    }

                    ColumnLayout
                    {
                        Text {
                            text: username
                            font.pixelSize: 15
                            font.bold: true
                            color: "black"
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }

                        Text {
                            text: "ID: " + peerId
                            font.pixelSize: 10
                            color: "black"
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }

                        //群成员在线状态信息
                        RowLayout {
                            // id: memberMessageLayout
                            Layout.fillWidth: true
                            Layout.fillHeight: true

                            //在线状态原点
                            Rectangle{

                                width: 10
                                height: 10
                                radius: 20
                                color: online ? "#00CA00" : "#B8B8B8"
                            }

                            Text {
                                Layout.fillWidth: true

                                text: ip
                                font.pixelSize: 10
                                color: online ? "#43A047" : "#999999"
                                elide: Text.ElideRight
                            }
                        }
                    }
                }
            }
        }
    }
}
