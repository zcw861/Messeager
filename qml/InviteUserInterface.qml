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
//     [v0.1.3]  JiangFan       2026-06-21
//         * 重构：使用Layout管理主窗口结构

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Window {
    id: inviteUserInterface
    width: 600
    height: 500
    visible: false
    flags: Qt.FramelessWindowHint

    //当前搜索关键字
    property string searchKeyword: ""

    //创建群聊所要求的最少总人数(包含当前用户自己)
    readonly property int minGroupMemberCount: 3

    //当前已经选中的总人数(包含自己)
    property int selectedCount: 0

    //是否已经满足创建群聊的条件。
    readonly property bool canCreateGroup: selectedCount >= minGroupMemberCount

    //距离允许创建群聊还缺少多少人
    readonly property int missingMemberCount: Math.max(0, minGroupMemberCount - selectedCount)

    //判断用户是否匹配关键字(跟PeerPanel一样）
    function matchSearch(username, ip)
    {
        if (searchKeyword.length == 0)
            return true

        var keyword = searchKeyword.toLowerCase()

        return username.toLowerCase().indexOf(keyword) !== -1 || ip.toLowerCase().indexOf(keyword) !== -1
    }

    //根据selected角色，重新计算已选择人数
    function recountSelectedUsers()
    {
        var count = 0

        //遍历候选成员中的每一项
        for (var row = 0; row < testPeerModel.count; row++) {
            //get(row)用于取得指定行的模型数据
            var user = testPeerModel.get(row)

            //只统计selected为true的成员
            if (user.selected)
                count++
        }

        //遍历完成后，一次性更新人数
        selectedCount = count
    }

    //修改指定成员的选中状态
    //row：成员在 testPeerModel 中的行号
    //selected：准备设置的新状态
    function setUserSelected(row, selected)
    {
        //获取当前行对应的用户
        var user = testPeerModel.get(row)
        //自己必须一直存在于群聊中
        if (user.isSelf)
            return
        //状态相同就不需要重复修改模型
        if (user.selected === selected)
            return

        //只修改当前行的selected角色。
        //左侧和右侧ListView都会变化
        testPeerModel.setProperty(row, "selected", selected)

        //模型修改完成后重新统计人数
        recountSelectedUsers()
    }

    //把当前所有已选择的成员整理成一个js数组
    //后续连接AppController时，可以直接把这个数组传给AppController
    function collectSelectedMembers()
    {
        var members = []

        for (var row = 0; row < testPeerModel.count; ++row) {
            var user = testPeerModel.get(row)
            //未选择的用户不加入群成员数组
            if (!user.selected)
                continue

            //不直接把 ListModel 的内部对象放入数组
            //而是重新构造一个js对象
            members.push({
                peerId: user.peerId,
                username: user.username,
                ip: user.ip,
                online: user.online,
                isSelf: user.isSelf
            })
        }

        return members
    }

    //恢复创建群聊窗口的初始状态
    //初始状态是：自己被选中，其他所有用户未选中
    function resetSelection()
    {
        for (var row = 0; row < testPeerModel.count; row++) {
            var user = testPeerModel.get(row)
            //自己设置为true
            //普通成员设置为false
            testPeerModel.setProperty(row, "selected", user.isSelf)
        }

        //重置模型后重新统计人数
        recountSelectedUsers()
    }

    //QML对象创建完成后，进行第一次初始化
    Component.onCompleted: {
        resetSelection()
    }

    //每次创建群聊窗口重新显示时，恢复初始状态
    onVisibleChanged: {
        if (!visible)
            return
        //清空上一次输入的搜索内容
        searchField.text = ""
        searchKeyword = ""

        //只保留自己为选中状态
        resetSelection()
    }

    ListModel {
        id: testPeerModel

        ListElement {
            peerId: "self"
            username: "当前用户"
            ip: "192.168.1.100"
            online: true
            selected: true
            isSelf: true
        }

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

        RowLayout {
            id: mainLayout

            anchors.fill: parent
            spacing: 0

            ColumnLayout {
                id: leftArea

                Layout.fillHeight: true
                Layout.preferredWidth: 220
                spacing: 10

                //搜索框
                TextField {
                    id: searchField

                    Layout.fillWidth: true
                    Layout.preferredHeight: 30
                    Layout.topMargin: 10
                    Layout.leftMargin: 10
                    Layout.rightMargin: 10


                    placeholderText: "搜索用户..."

                    background: Rectangle {
                        id: searchBackGround
                        radius: 10
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

                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    color: "white"
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
                            id: candidateRow

                            //index是当前成员在模型中的行号，点击时需要把它传给setUserSelected()
                            required property int index

                            //以下属性与ListElement中的角色名称一一对应
                            required property string peerId
                            required property string username
                            required property string ip
                            required property bool online
                            required property bool selected
                            required property bool isSelf

                            //当前成员是否符合搜索条件
                            readonly property bool matched:
                                inviteUserInterface.matchSearch(username, ip)

                            width: ListView.view.width - 10
                            height: matched ? 52 : 0
                            visible: matched
                            enabled: matched

                            radius: 8
                            color: candidateHover.hovered && !isSelf ? "#f5f5f5" : "white"

                            border.width: 1
                            border.color: "#eeeeee"


                            RowLayout {
                                width: parent.width
                                height: parent.height
                                spacing: 8

                                // 左侧选中圆圈。
                                Rectangle {
                                    Layout.preferredWidth: 20
                                    Layout.preferredHeight: 20
                                    Layout.leftMargin: 8
                                    Layout.alignment: Qt.AlignVCenter

                                    radius: 10
                                    color: candidateRow.selected ? "#1296db" : "white"
                                    border.width: 1
                                    border.color: candidateRow.selected
                                                  ? "#1296db"
                                                  : "#d0d0d0"

                                    // 选中后显示对勾。
                                    Text {
                                        width: parent.width
                                        height: parent.height

                                        text: "✓"
                                        visible: candidateRow.selected
                                        color: "white"
                                        font.pixelSize: 13

                                        horizontalAlignment: Text.AlignHCenter
                                        verticalAlignment: Text.AlignVCenter
                                    }
                                }

                                // 中间显示用户名和 IP。
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    Layout.alignment: Qt.AlignVCenter
                                    spacing: 1

                                    Text {
                                        Layout.fillWidth: true

                                        // 自己需要有明确标记。
                                        text: candidateRow.isSelf
                                              ? candidateRow.username + "（自己）"
                                              : candidateRow.username

                                        color: "#333333"
                                        font.pixelSize: 14
                                        elide: Text.ElideRight
                                    }

                                    Text {
                                        Layout.fillWidth: true
                                        text: candidateRow.ip
                                        color: "#999999"
                                        font.pixelSize: 11
                                        elide: Text.ElideRight
                                    }
                                }

                                // 右侧显示在线状态。
                                Text {
                                    Layout.rightMargin: 8
                                    Layout.alignment: Qt.AlignVCenter

                                    text: candidateRow.online ? "在线" : "离线"
                                    color: candidateRow.online ? "#43a047" : "#999999"
                                    font.pixelSize: 11
                                }
                            }

                            HoverHandler {
                                id: candidateHover

                                // 自己不可操作，因此使用普通箭头。
                                // 其他成员可以选择，使用手形光标。
                                cursorShape: candidateRow.isSelf
                                             ? Qt.ArrowCursor
                                             : Qt.PointingHandCursor
                            }

                            TapHandler {
                                // 自己不允许取消，因此直接禁用自己的 TapHandler。
                                enabled: !candidateRow.isSelf

                                acceptedButtons: Qt.LeftButton
                                gesturePolicy: TapHandler.ReleaseWithinBounds

                                onTapped: {
                                    // selected 为 true 时改为 false；
                                    // selected 为 false 时改为 true。
                                    inviteUserInterface.setUserSelected(
                                        candidateRow.index,
                                        !candidateRow.selected
                                    )
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
            }

            ColumnLayout {
                id: rightArea

                // rightArea 是 mainLayout 的直接子项，
                // 所以必须使用 Layout 附加属性，让外层 RowLayout 管理尺寸。
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 0

                // 右侧标题栏。
                RowLayout {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 56

                    Text {
                        Layout.leftMargin: 20
                        Layout.alignment: Qt.AlignVCenter

                        text: qsTr("创建群聊")
                        color: "#222222"
                        font.pixelSize: 18
                        font.bold: true
                    }

                    // 弹性占位，把人数文字推向右边。
                    Item {
                        Layout.fillWidth: true
                    }

                    Text {
                        Layout.rightMargin: 20
                        Layout.alignment: Qt.AlignVCenter

                        text: "已选 " + inviteUserInterface.selectedCount + " 人"
                        color: "#888888"
                        font.pixelSize: 13
                    }
                }

                // 标题栏下方分割线。
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 1
                    color: "#eeeeee"
                }

                // 已选择成员列表。
                ListView {
                    id: selectedUserListView

                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.leftMargin: 12
                    Layout.rightMargin: 12

                    clip: true
                    spacing: 0

                    // 与左侧共用同一个模型。
                    model: testPeerModel

                    delegate: Rectangle {
                        id: selectedRow

                        required property int index
                        required property string peerId
                        required property string username
                        required property string ip
                        required property bool selected
                        required property bool isSelf

                        width: ListView.view.width

                        // 未选择成员的高度设置为 0，
                        // 因此它不会在右侧占据可见空间。
                        height: selected ? 54 : 0
                        visible: selected

                        color: "transparent"

                        RowLayout {
                            width: parent.width
                            height: parent.height
                            spacing: 10

                            // 简单模拟头像。
                            Rectangle {
                                Layout.preferredWidth: 34
                                Layout.preferredHeight: 34
                                Layout.leftMargin: 6
                                Layout.alignment: Qt.AlignVCenter

                                radius: 17
                                color: selectedRow.isSelf
                                       ? "#d8ecff"
                                       : "#eeeeee"

                                Text {
                                    width: parent.width
                                    height: parent.height

                                    // 取用户名第一个字符作为模拟头像文字。
                                    text: selectedRow.username.length > 0
                                          ? selectedRow.username.charAt(0)
                                          : "?"

                                    color: "#555555"
                                    font.pixelSize: 14

                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }
                            }

                            // 成员名称和 IP。
                            ColumnLayout {
                                Layout.fillWidth: true
                                Layout.alignment: Qt.AlignVCenter
                                spacing: 1

                                Text {
                                    Layout.fillWidth: true

                                    text: selectedRow.isSelf
                                          ? selectedRow.username + "（自己）"
                                          : selectedRow.username

                                    color: "#333333"
                                    font.pixelSize: 14
                                    elide: Text.ElideRight
                                }

                                Text {
                                    Layout.fillWidth: true
                                    text: selectedRow.ip
                                    color: "#999999"
                                    font.pixelSize: 11
                                    elide: Text.ElideRight
                                }
                            }

                            // 自己显示为固定成员。
                            Text {
                                visible: selectedRow.isSelf

                                Layout.rightMargin: 8
                                Layout.alignment: Qt.AlignVCenter

                                text: "固定成员"
                                color: "#999999"
                                font.pixelSize: 11
                            }

                            // 普通成员右侧显示删除按钮。
                            Rectangle {
                                id: removeMemberButton

                                visible: !selectedRow.isSelf

                                Layout.preferredWidth: 28
                                Layout.preferredHeight: 28
                                Layout.rightMargin: 6
                                Layout.alignment: Qt.AlignVCenter

                                radius: 14
                                color: removeMemberHover.hovered
                                       ? "#eeeeee"
                                       : "transparent"

                                Text {
                                    width: parent.width
                                    height: parent.height

                                    text: "×"
                                    color: "#888888"
                                    font.pixelSize: 18

                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }

                                HoverHandler {
                                    id: removeMemberHover
                                    cursorShape: Qt.PointingHandCursor
                                }

                                TapHandler {
                                    acceptedButtons: Qt.LeftButton
                                    gesturePolicy: TapHandler.ReleaseWithinBounds

                                    onTapped: {
                                        // 将该成员的 selected 改为 false。
                                        // 左侧圆圈和右侧列表会同时更新。
                                        inviteUserInterface.setUserSelected(
                                            selectedRow.index,
                                            false
                                        )
                                    }
                                }
                            }
                        }
                    }
                }

                // 人数不足时显示提示。
                Text {
                    Layout.fillWidth: true
                    Layout.leftMargin: 20
                    Layout.rightMargin: 20
                    Layout.bottomMargin: 10

                    visible: !inviteUserInterface.canCreateGroup

                    text: "至少需要 "
                          + inviteUserInterface.minGroupMemberCount
                          + " 人，还需选择 "
                          + inviteUserInterface.missingMemberCount
                          + " 人"

                    color: "#e67e22"
                    font.pixelSize: 12
                    wrapMode: Text.WordWrap
                }

                RowLayout {
                    id: buttonBar

                    Layout.preferredHeight: 60
                    Layout.fillWidth: true
                    spacing: 5

                    //占位符的作用
                    Item {
                        Layout.fillWidth: true
                    }

                    //确定按钮
                    Rectangle{
                        id: okButton

                        Layout.preferredHeight: 30
                        Layout.preferredWidth: 60
                        Layout.rightMargin: 10

                        radius: 10;

                        //根据是否选择用户和悬停状态改变颜色
                        color: {
                        if (!inviteUserInterface.canCreateGroup) {
                            return "#e0e0e0"  //禁用状态的颜色
                        } else {
                            return okButtonHover.hovered ? "#00ffff" : "#87cefa"
                            }
                        }

                        //透明度变化表示禁用状态
                        opacity: inviteUserInterface.canCreateGroup ? 1.0 : 0.6

                        Text{
                            anchors.centerIn: parent
                            text: "确定"
                            color: inviteUserInterface.canCreateGroup ? "blue" : "#999999"
                        }

                        HoverHandler {
                            id: okButtonHover
                        //根据是否选择用户改变鼠标形状
                            cursorShape: inviteUserInterface.canCreateGroup ? Qt.PointingHandCursor : Qt.ForbiddenCursor
                        }

                        TapHandler {
                            // 人数不足时整个 TapHandler 被禁用，
                            // 因此点击按钮不会执行任何创建操作。
                            enabled: inviteUserInterface.canCreateGroup

                            acceptedButtons: Qt.LeftButton
                            gesturePolicy: TapHandler.ReleaseWithinBounds

                            onTapped: {
                                // 即使 TapHandler 已经通过 enabled 禁用，
                                // 这里仍然再次检查，避免后续修改代码时绕过限制。
                                if (!inviteUserInterface.canCreateGroup)
                                    return

                                // 收集当前所有已选择成员。
                                var members = inviteUserInterface.collectSelectedMembers()

                                // 目前没有连接后端，所以先通过控制台验证。
                                console.log("模拟创建群聊")
                                console.log("群聊成员数量:", members.length)
                                console.log("群聊成员:", JSON.stringify(members))

                                // 模拟创建完成后关闭窗口。
                                inviteUserInterface.close()
                            }
                        }
                    }

                    //取消按钮
                    Rectangle{
                        id: cancelButton

                        Layout.preferredHeight: 30
                        Layout.preferredWidth: 60
                        Layout.rightMargin: 5  //右边不抵住边缘

                        radius: 10
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
                }
            }
        }
    }
}