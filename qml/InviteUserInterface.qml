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
//     [v0.1.4]  JiangFan       2026-06-21
//         * 重构：使用Layout管理主窗口结构
//     [v0.1.5]  HeZhiyuan       2026-06-22
//         * 重构：1.默认本机用户为群聊成员之一
//                2.左侧点击用户后，在右侧显示
//                3.少于三人无法创建群聊
//           修改部分UI界面
//     [v0.1.6]  HeZhiyuan       2026-06-22
//         * 修改: 确定按钮的颜色与格式
//           新增：生成模拟的群聊id，生成默认群名称，传递信号给PeerPanel
//     [v0.1.7]  HeZhiyuan       2026-06-23
//         * 完善创建群聊请求的前端提交流程
//     [v0.1.8]  HeZhiyuan       2026-06-25
//         * 移除QML生成模拟群聊ID的旧逻辑，群聊ID统一由C++网络层生成

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

    //当前是否正在向C++提交群聊创建请求
    //为true时禁用确定按钮，避免用户连续点击，导致同一组成员创建多个不同的群聊
    property bool creationInProgress: false

    //用户确认创建群聊后，向PeerPanel发送创建请求
    //这里只传递群名称和成员列表，不在QML中生成groupId
    //真实groupId必须由C++网络层GroupChat统一生成
    signal groupCreationRequested(string groupName, var members)

    //人数满足要求，并且当前没有正在执行的创建请求时，才允许再次点击确定按钮
    readonly property bool canCreateGroup: selectedCount >= minGroupMemberCount && !creationInProgress

    //距离允许创建群聊还缺少多少人
    readonly property int missingMemberCount: Math.max(0, minGroupMemberCount - selectedCount)

    //由PeerPanel传入的候选成员。
    //列表内容来自AppController.groupCandidates。
    property var candidateSourceModel: []

    //把undefined、null或其他值转换为安全字符串。
    function normalizedText(value)
    {
        if (value === undefined || value === null)
            return ""

        return String(value).trim()
    }

    //判断用户名或IP是否包含当前搜索关键字
    function matchSearch(username, ip)
    {
        //没有搜索内容时显示全部成员
        if (searchKeyword.length === 0)
            return true
        //统一转换为小写，避免搜索受到大小写影响
        const keyword = searchKeyword.toLowerCase()

        //用户名或IP任意一个匹配就显示该成员
        return username.toLowerCase().includes(keyword) || ip.toLowerCase().includes(keyword)
    }

    //根据selected角色，重新计算已选择人数
    function recountSelectedUsers()
    {
        var count = 0
        //遍历候选成员中的每一项
        for (var row = 0; row < candidateModel.count; row++) {
            //get(row)用于取得指定行的模型数据
            var user = candidateModel.get(row)

            //只统计selected为true的成员
            if (user.selected)
                count++
        }
        //遍历完成后，一次性更新人数
        selectedCount = count
    }

    //修改指定成员的选中状态
    //row：成员在 candidateModel 中的行号
    //selected：准备设置的新状态
    function setUserSelected(row, selected)
    {
        //根据行号取得候选成员
        const user = candidateModel.get(row)

        //本机用户必须始终属于群聊，因此不能取消选中
        if (user.isSelf) return

        //状态没有发生变化时不重复修改模型
        if (user.selected === selected) return

        //只修改当前行的selected角色
        candidateModel.setProperty(row, "selected", selected)

        //模型发生变化后重新统计已选择人数
        recountSelectedUsers()
    }

    //把当前所有已选择的成员整理成一个js数组
    //后续连接AppController时，可以直接把这个数组传给AppController
    function collectSelectedMembers()
    {
        var members = []

        for (var row = 0; row < candidateModel.count; ++row) {
            var user = candidateModel.get(row)
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

    //根据已选择成员生成默认群名称
    //members是collectSelectedMembers()返回的数组
    //每个元素都包含username、peerId、ip等字段
    function buildDefaultGroupName(members)
    {
        var memberNames = []
        //依次取出每名成员的用户名
        for (var index = 0; index < members.length; ++index) {
            memberNames.push(members[index].username)
        }

        //join("、") 会用顿号连接所有用户名
        var joinedNames = memberNames.join("、")

        //在名称后面补充总人数
        return joinedNames + "（" + members.length + "人）"
    }

    //由PeerPanel在C++群聊创建完成后调用，确定是否要恢复按钮状态
    function finishCreation(success)
    {
        creationInProgress = false
        if (success)
            inviteUserInterface.close()
    }

    //将C++提供的候选成员复制到本地ListModel
    //candidateSourceModel负责提供C++候选成员数据，candidateModel在候选成员字段基础上增加selected界面状态
    function rebuildCandidateModel()
    {
        candidateModel.clear()

        if (candidateSourceModel === undefined || candidateSourceModel === null) {
            selectedCount = 0
            return
        }

        var addedPeerIds = {}

        for (var sourceIndex = 0; sourceIndex < candidateSourceModel.length; ++sourceIndex) {
            //从C++提供的候选列表中读取当前成员
            const sourceUser = candidateSourceModel[sourceIndex]

            //规范化成员字段，避免undefined、null和首尾空格
            const peerId = normalizedText(sourceUser.peerId)
            const username = normalizedText(sourceUser.username)
            const ip = normalizedText(sourceUser.ip)

            //把模型中的值明确转换为bool
            const online = Boolean(sourceUser.online)
            const isSelf = Boolean(sourceUser.isSelf)

            //缺少稳定ID或用户名的异常记录不显示。
            if (peerId.length === 0 || username.length === 0) {
                continue
            }

            //同一个peerId只能出现一次。
            if (addedPeerIds[peerId] === true)
                continue

            addedPeerIds[peerId] = true

            //离线成员也保留在候选列表中
            candidateModel.append({
                peerId: peerId,
                username: username,
                ip: ip,
                online: online,
                selected: isSelf,
                isSelf: isSelf
            })
        }

        recountSelectedUsers()
    }

    //恢复创建群聊窗口的初始状态
    //初始状态是：自己被选中，其他所有用户未选中
    function resetSelection()
    {
        for (var row = 0; row < candidateModel.count; row++) {
            var user = candidateModel.get(row)
            //自己设置为true
            //普通成员设置为false
            candidateModel.setProperty(row, "selected", user.isSelf)
        }

        //重置模型后重新统计人数
        recountSelectedUsers()
    }

    //成员变化时重新构造本地选择模型
    onCandidateSourceModelChanged: rebuildCandidateModel()

    //每次重新打开窗口时恢复搜索和选择状态
    onVisibleChanged: {
        //窗口隐藏时不做处理
        if (!visible) return
        //恢复创建按钮状态
        creationInProgress = false
        //清空上一次搜索内容
        searchField.clear()
        searchKeyword = ""
        //候选数据没有变化时只重置选择状态，不重新复制整个模型
        resetSelection()
    }

    ListModel {
        id: candidateModel
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
                Layout.preferredWidth: 200
                spacing: 10

                //搜索框
                TextField {
                    id: searchField

                    color: "black"
                    Layout.fillWidth: true
                    Layout.preferredHeight: 30
                    Layout.topMargin: 10
                    Layout.leftMargin: 10
                    Layout.rightMargin: 10


                    placeholderText: "搜索用户..."

                    background: Rectangle {
                        id: searchBackground
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
                    id:leftInviteUser

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
                        model: candidateModel

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

                                //左侧选中圆圈
                                Rectangle {
                                    Layout.preferredWidth: 20
                                    Layout.preferredHeight: 20
                                    Layout.leftMargin: 8
                                    Layout.alignment: Qt.AlignVCenter

                                    radius: 10
                                    color: candidateRow.selected ? "#029aff" : "white"
                                    border.width: 1
                                    border.color: candidateRow.selected ? "#029aff" : "#d0d0d0"
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

                                //右侧显示在线状态
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

                                //自己不可操作，因此使用普通箭头，其他成员可以选择，使用手形光标
                                cursorShape: candidateRow.isSelf ? Qt.ArrowCursor : Qt.PointingHandCursor
                            }

                            TapHandler {
                                //自己不允许取消，直接禁用TapHandler
                                enabled: !candidateRow.isSelf
                                acceptedButtons: Qt.LeftButton
                                gesturePolicy: TapHandler.ReleaseWithinBounds
                                onTapped: {
                                    //把当前成员的选中状态取反
                                    inviteUserInterface.setUserSelected(candidateRow.index, !candidateRow.selected)
                                }
                            }
                        }

                        //垂直滚动条
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

                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 0

                //右侧标题栏
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

                    //人数文字推向右边
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

                //标题栏下方分割线
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 1
                    color: "#eeeeee"
                }

                //已选择成员列表
                ListView {
                    id: selectedUserListView

                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.leftMargin: 12
                    Layout.rightMargin: 12

                    clip: true
                    spacing: 0

                    model: candidateModel

                    delegate: Rectangle {
                        id: selectedRow

                        required property int index
                        required property string peerId
                        required property string username
                        required property string ip
                        required property bool selected
                        required property bool isSelf

                        width: ListView.view.width

                        //未选择成员的高度设置为0，因此不会在右侧出现
                        height: selected ? 54 : 0
                        visible: selected

                        color: "transparent"

                        RowLayout {
                            width: parent.width
                            height: parent.height
                            spacing: 10

                            //头像
                            Rectangle {
                                Layout.preferredWidth: 34
                                Layout.preferredHeight: 34
                                Layout.leftMargin: 6
                                Layout.alignment: Qt.AlignVCenter

                                radius: 17
                                color: selectedRow.isSelf ? "#d8ecff" : "#eeeeee"

                                Text {
                                    width: parent.width
                                    height: parent.height

                                    //取用户名第一个字符作为头像文字
                                    text: selectedRow.username.length > 0
                                          ? selectedRow.username.charAt(0)
                                          : "?"

                                    color: "#555555"
                                    font.pixelSize: 14

                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }
                            }

                            //成员名称和IP
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

                            //自己为固定成员
                            Text {
                                visible: selectedRow.isSelf

                                Layout.rightMargin: 8
                                Layout.alignment: Qt.AlignVCenter

                                text: "固定成员"
                                color: "#999999"
                                font.pixelSize: 11
                            }

                            //普通成员右侧有删除按钮
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
                                        //将该成员的selected改为false
                                        inviteUserInterface.setUserSelected(selectedRow.index, false)
                                    }
                                }
                            }
                        }
                    }
                }
                //人数不足时显示提示
                Text {
                    Layout.fillWidth: true
                    Layout.leftMargin: 20
                    Layout.rightMargin: 20
                    Layout.bottomMargin: 10

                    //只有人数确实不足时才显示人数提示
                    visible: inviteUserInterface.missingMemberCount > 0

                    text: qsTr("至少需要 %1 人，还需选择 %2 人")
                            .arg(inviteUserInterface.minGroupMemberCount).arg(inviteUserInterface.missingMemberCount)

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

                        radius: 10

                        //根据是否选择用户和悬停状态改变颜色
                        color: {
                        if (!inviteUserInterface.canCreateGroup) {
                            return "#e0e0e0"  //禁用状态的颜色
                        } else {
                            return okButtonHover.hovered ? "#87cefa" : "#029aff"
                            }
                        }

                        //透明度变化表示禁用状态
                        opacity: inviteUserInterface.canCreateGroup ? 1.0 : 0.6

                        Text{
                            anchors.centerIn: parent
                            text: "确定"
                            color: inviteUserInterface.canCreateGroup ? "white" : "#999999"
                        }

                        HoverHandler {
                            id: okButtonHover
                        //根据是否选择用户改变鼠标形状
                            cursorShape: inviteUserInterface.canCreateGroup
                                         ? Qt.PointingHandCursor : Qt.ForbiddenCursor
                        }

                        TapHandler {
                            //人数不足时整个TapHandler禁用
                            enabled: inviteUserInterface.canCreateGroup

                            acceptedButtons: Qt.LeftButton
                            gesturePolicy: TapHandler.ReleaseWithinBounds

                            onTapped: {
                                //人数不足或正在创建时，不重复提交
                                if (!inviteUserInterface.canCreateGroup)
                                    return

                                //收集当前全部已选择成员
                                var members = inviteUserInterface.collectSelectedMembers()

                                //默认群名由前端根据当前显示名称生成
                                var groupName = inviteUserInterface.buildDefaultGroupName(members)

                                //先锁定按钮，避免重复点击
                                inviteUserInterface.creationInProgress = true

                                //只发送群名称和成员
                                inviteUserInterface.groupCreationRequested( groupName, members)
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