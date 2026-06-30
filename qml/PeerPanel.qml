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
* [v0.1.0]  JiangFan  2026-06-02
* * Initial creation
*
* Change Log:
* [v0.2.0]  JiangFan  2026-06-03
* * 左侧栏实现了选择用户功能（实现鼠标悬停变色，选择变色），并与顶部栏连通（通过Main.qml传递消息）
* * 搜索栏完成消息传递，但具体操作还未完成
*
* Change Log:
* [v0.2.1]  HeZhiYuan  2026-06-04
* * 新增peerClosed信号，用于在再次点击当前聊天用户时通知Main.qml关闭当前会话。
* * 完善用户项点击逻辑：点击当前用户触发peerClosed，点击其他用户触发peerSelected并切换聊天对象。
* * 调整左侧用户列表交互方式，使其更接近QQ的会话选择行为。
*
* Change Log:
* [v0.2.2]  JiangFan  2026-06-05
* * 整理注释结构，完善左侧栏搜索功能,重写鼠标悬停变色逻辑
*
* [v0.2.3]  ZhouChengWei  2026-06-06
* * 添加创建群聊按钮并且添加了相关菜单项
*
* [v0.2.4]  JiangFan  2026-06-10
* * 增加左侧栏搜索框焦点清除功能
*
* [v0.2.5] HeZhiyuan 2026-06-13
* * 删除临时测试ListModel。
* * 新增peerModel外部模型属性。
* * 使用QVariantList/QVariantMap的modelData读取用户数据。
* * 增加左键限制和ReleaseWithinBounds点击策略。
*
* [v0.2.5] HeZhiyuan 2026-06-14
* * 增加删除请求信号
* *[v0.2.6] ZhouChengWei 2026-06-14
* * 修改了搜索栏布局
*
* *[v0.2.7] JiangFan 2026-06-21
* * 重构：使用Layout管理主窗口结构
*
* *[v0.2.8] HeZhiyuan 2026-06-23
* * 新增：左侧列表显示新创建的群聊
* *[v0.2.9] HeZhiyuan 2026-06-23
*   增加群聊创建请求的中转与结果处理流程
* *[v0.3.0] HeZhiyuan 2026-06-23
*   群聊列表改为使用AppController提供的真实群聊模型
*   简化群聊创建请求的转发和创建结果处理流程
* *[v0.3.1] HeZhiyuan 2026-06-26
*   完善群聊卡片边框与用户统一，增加点击、悬停时的颜色变化
* *[v0.3.2] HeZhiyuan 2026-06-28
*   修改在暗色模式下创建群聊以及加好友卡片的显示问题
* *[v0.3.3] HeZhiyuan 2026-06-29
*   新增：已退出群聊的右键删除功能
*        未退出群聊点击删除时的提示弹窗，引导用户先退出群聊
*        群聊删除确认弹窗，防止误删群聊及历史消息
*/

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

//左侧栏
Rectangle {
    id: peerPanel
    color: "#e8e8e8"
    //color: "#F7F8FA"

    //后续交给 PeerModel 或 AppController 处理
    //Window.qml给，控制左侧用户高亮
    property string currentPeerId: ""

    //当前搜索关键字,搜索框变化时更新，过滤左侧用户列表
    property string searchKeyword : ""

    //当前被选中的群聊ID
    property string currentGroupId: ""

    //后续交给PeerModel或AppController处理
    signal searchTextChanged(string keyword)

    //点击左侧用户后，向 Window.qml 通知当前选中的用户
    signal peerSelected(string peerId, string username, string ip)

    //点击当前用户时，通知 Window.qml 关闭聊天窗口
    signal peerClosed()

    //用户确认删除后，通知 Window.qml。
    signal peerDeleteRequested(string peerId)

    //InviteUserInterface提交创建请求后，PeerPanel转发给AppController
    signal groupCreationRequested(string groupName, var members)

    //增加外部模型属性
    property var peerModel: []
    //创建群聊窗口使用的候选成员
    property var groupCandidateModel: []
    //真实群聊模型来自appController.groups
    property var groupModel: []

    //选择群聊时传递稳定的群ID、群名称和当前活动状态
    //成员由AppController根据groupId从数据库读取
    signal groupSelected(string groupId, string groupName, bool groupActive)

    //用户确认彻底删除已退出群聊后，调用AppController
    signal groupDeleteRequested(string groupId)

    //再次点击当前群聊时关闭群会话
    signal groupClosed()

    //当前右键选中的待删除用户。
    property string pendingDeletePeerId: ""
    property string pendingDeletePeerName: ""

    //保存当前右键点击的群聊信息，供群聊菜单和删除确认窗口使用
    property string pendingDeleteGroupId: ""
    property string pendingDeleteGroupName: ""
    property bool pendingDeleteGroupActive: true

    //判断两个显示字段中是否包含当前搜索关键字
    function matchSearch(firstText, secondText) {
        //没有输入搜索词时，所有项目都显示
        if (searchKeyword.length === 0)
            return true

        //统一使用小写进行不区分大小写的匹配
        const keyword = searchKeyword.toLowerCase()

        //任意一个字段包含关键字即可显示
        return firstText.toLowerCase().includes(keyword) || secondText.toLowerCase().includes(keyword)
    }

    //处理群聊创建结果
    function finishGroupCreation(success, groupId, groupName) {
        //恢复邀请窗口的按钮状态；创建成功时窗口会自行关闭
        inviteUserInterface.finishCreation(success)

        //创建失败时不切换会话
        if (!success)
            return
        //新建群聊一定处于活动状态
        peerPanel.groupSelected(groupId, groupName, true)
    }
    ColumnLayout {
        id: peerLayout

        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 50

            color: "white"

            border.color: "#e5e5e5"
            border.width: 1

            //搜索框 + 加号
            RowLayout {
                id: searchArea

                anchors.fill: parent

                spacing: 5

                //Search
                TextField {
                    id: searchField

                    color: "black"

                    Layout.fillWidth: true
                    Layout.preferredHeight: 30
                    Layout.leftMargin: 10
                    Layout.alignment: Qt.AlignVCenter

                    placeholderText: qsTr("搜索用户")
                    font.pixelSize: 12

                    //Search background
                    background: Rectangle {
                        radius: 10
                        color: "#F2F2F2"
                        border.color: "#b4b4b4"
                        border.width: 1
                    }

                    //当输入内容发生变化时，向外发出信号
                    onTextChanged: {

                        //记录当前搜索关键字
                        peerPanel.searchKeyword = text.trim()

                        // 向外传递关键字变化，后续可交给PeerModel / AppController处理
                        peerPanel.searchTextChanged(peerPanel.searchKeyword)
                    }
                }

                //创建群聊按钮
                Rectangle{
                    id: createGroupChat

                    Layout.preferredHeight: 30
                    Layout.preferredWidth: 30
                    Layout.rightMargin: 10
                    Layout.alignment: Qt.AlignVCenter

                    radius: 10
                    color: typeChange.hovered ? "#a9a9a9" : "#f5f5f5"

                    Text{
                        text: "+"
                        font.pointSize: 20
                        color: "#e6e6fa"
                        anchors.centerIn: parent
                    }

                    HoverHandler {
                        id: typeChange
                        cursorShape: Qt.PointingHandCursor
                    }

                    TapHandler{
                        onTapped: {
                            //清除搜索框焦点
                            peerPanel.clearSearchFocus()

                            featureSet.popup(createGroupChat, 0, createGroupChat.height)
                        }
                    }

                    //创建群聊/添加好友等功能的菜单
                    Menu{
                        id: featureSet
                        width: 100

                        background: Rectangle {
                            implicitWidth: 100
                            color: "#FFFFFF"
                            radius: 6
                            border.color: "#D9D9D9"
                            border.width: 1
                        }

                        MenuItem{
                            id: createGroupMenuItem
                            text: qsTr("创建群聊")

                            contentItem: Text {
                                text: createGroupMenuItem.text
                                color: "#333333"
                                font: createGroupMenuItem.font
                                verticalAlignment: Text.AlignVCenter
                                elide: Text.ElideRight
                            }

                            //MenuItem本身就是可以点击的控件 并自带triggered信号,所以不应该用TapHandler
                            onTriggered: {
                                peerPanel.clearSearchFocus()
                                inviteUserInterface.show()
                            }

                        }
                        MenuItem{
                            id: addFriendMenuItem
                            text: qsTr("加好友")

                            contentItem: Text {
                                text: addFriendMenuItem.text
                                color: "#333333"
                                font: addFriendMenuItem.font
                                verticalAlignment: Text.AlignVCenter
                                elide: Text.ElideRight
                            }
                        }
                    }
                }

            }

        }

        //存在群聊时才显示“群聊”标题
        Text {
            id: groupTitle

            Layout.fillWidth: true
            Layout.leftMargin: 10
            Layout.topMargin: 5

            visible: peerPanel.groupModel && peerPanel.groupModel.length > 0
            text: qsTr("群聊")
            font.pixelSize: 15
            font.bold: true
            color: "#333333"
        }

        //显示群聊列表
        ListView {
            id: groupListView

            Layout.fillWidth: true
            Layout.preferredHeight: Math.min(peerPanel.groupModel.length * 62, 186)
            Layout.topMargin: 4

            visible: peerPanel.groupModel && peerPanel.groupModel.length > 0

            clip: true
            spacing: 6

            model: peerPanel.groupModel

            delegate: Rectangle {
                id: groupDelegate

                //接收当前群聊模型项
                required property var modelData

                readonly property string groupId: String(modelData.groupId)
                readonly property string groupName: String(modelData.groupName)
                readonly property int memberCount: Number(modelData.memberCount)
                readonly property string memberSummary: String(modelData.memberSummary)
                readonly property bool isActive: Boolean(modelData.isActive)
                readonly property bool matched: peerPanel.matchSearch(groupName, memberSummary)

                //判断当前群聊是否处于选中状态
                property bool selected: peerPanel.currentGroupId === groupId

                width: groupListView.width - 20
                height: matched ? 56 : 0
                radius: 10
                //将卡片放置在列表水平方向的中央，使左右各留出10像素
                anchors.horizontalCenter: parent.horizontalCenter
                visible: matched
                //选中的群聊使用浅蓝色背景，鼠标悬停时使用浅灰色背景，其余情况使用默认背景
                color: selected ? "#E8F3FF" : groupHoverHandler.hovered ? "#EEEEEE" : "#F5F5F5"

                //选中的群聊使用浅蓝色边框，鼠标悬停时使用灰色边框，其余情况使用默认边框
                border.color: selected ? "#9BCBFF" : groupHoverHandler.hovered ? "#D0D0D0" : "#E0E0E0"

                //设置一像素边框，使卡片边界更加清晰。
                border.width: 1

                RowLayout {
                    width: parent.width
                    height: parent.height
                    spacing: 10

                    Rectangle {
                        Layout.preferredWidth: 34
                        Layout.preferredHeight: 34
                        Layout.leftMargin: 10
                        Layout.alignment: Qt.AlignVCenter

                        radius: 17
                        color: "#D8ECFF"

                        Text {
                            width: parent.width
                            height: parent.height

                            text: qsTr("群")
                            color: "#357ABD"
                            font.pixelSize: 14
                            font.bold: true

                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.alignment: Qt.AlignVCenter
                        spacing: 2

                        Text {
                            Layout.fillWidth: true

                            text: groupDelegate.groupName

                            color: "#1F2329"
                            font.pixelSize: 13
                            font.bold: true
                            elide: Text.ElideRight
                        }

                        Text {
                            Layout.fillWidth: true

                            //使用arg()组合人数和成员
                            text: qsTr("%1人 · %2").arg(groupDelegate.memberCount).arg(groupDelegate.memberSummary)

                            color: "#6B7280"
                            font.pixelSize: 11
                            elide: Text.ElideRight
                        }
                    }

                    Item {
                        Layout.preferredWidth: 4
                    }
                }

                HoverHandler {
                    id: groupHoverHandler
                    cursorShape:
                        Qt.PointingHandCursor
                }

                TapHandler {
                    acceptedButtons: Qt.LeftButton
                    gesturePolicy: TapHandler.ReleaseWithinBounds

                    onTapped: {
                        //清除搜索框焦点，使点击群聊后的交互行为与点击用户一致
                        peerPanel.clearSearchFocus()

                        //再次点击当前群聊时关闭会话
                        if (peerPanel.currentGroupId === groupDelegate.groupId) {
                            peerPanel.groupClosed()
                            return
                        }

                        //只发送选择信号，currentGroupId由Window.qml统一维护
                        peerPanel.groupSelected(groupDelegate.groupId, groupDelegate.groupName, groupDelegate.isActive)
                    }
                }
                //打开群聊操作菜单
                TapHandler {
                    acceptedButtons: Qt.RightButton
                    gesturePolicy: TapHandler.ReleaseWithinBounds

                    onTapped: function(eventPoint, button) {
                        peerPanel.clearSearchFocus()

                        //记录右键点击的群聊，MenuItem根据isActive决定是否允许彻底删除
                        peerPanel.pendingDeleteGroupId = groupDelegate.groupId
                        peerPanel.pendingDeleteGroupName = groupDelegate.groupName
                        peerPanel.pendingDeleteGroupActive = groupDelegate.isActive

                        //把群聊卡片内的点击坐标转换为PeerPanel坐标，保证菜单出现在右键位置附近
                        const menuPosition = groupDelegate.mapToItem(peerPanel, eventPoint.position.x, eventPoint.position.y)

                        groupContextMenu.x = menuPosition.x
                        groupContextMenu.y = menuPosition.y
                        groupContextMenu.open()
                    }
                }
            }

            Item {
                Layout.preferredWidth: 10
            }

        }

        Text {
            id: peerTitle

            Layout.fillWidth: true
            Layout.leftMargin: 10
            Layout.topMargin: 5

            text: qsTr("局域网用户")
            font.pixelSize: 15
            font.bold: true          //加粗
            color: "#333333"         //灰色

            }

        //用户列表
        ListView {
            id: peerListView

            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.topMargin: 10

            clip: true
            spacing: 6

            model: peerPanel.peerModel

            //使用QVariantList/QVariantMap的modelData读取用户数据
            delegate: Rectangle {
                id: peerItem

                required property var modelData
                readonly property string peerId: modelData.peerId
                readonly property string username: modelData.username
                readonly property string ip: modelData.ip
                readonly property bool online: modelData.online

                width: peerListView.width - 20
                radius: 10

                //当前用户是否匹配关键字
                property bool matched: peerPanel.matchSearch(username, ip)

                //当前用户是否被选中
                // property bool selected: peerPanel.currentPeerId === peerId

                // 只有当前没有选择群聊，并且当前私聊用户 ID 相同时，才显示私聊高亮。
                property bool selected: peerPanel.currentGroupId.length === 0 && peerPanel.currentPeerId === peerId

                //不匹配--> 高度为0 --> 隐藏
                height: matched ? 56 : 0
                visible: matched

                //选中 > 悬停 > 默认
                color: selected ? "#E8F3FF"
                                : hoverHandler.hovered ? "#EEEEEE"
                                : "#F5F5F5"

                border.color: selected ? "#9BCBFF"
                                       : hoverHandler.hovered ? "#D0D0D0"
                                       : "#E0E0E0"

                border.width: 1

                anchors.horizontalCenter: parent.horizontalCenter

                //在线状态圆点
                Rectangle {
                    id: statusDot

                    width: 10
                    height: 10
                    radius: 5
                    color: online ? "#00CA00" : "#B8B8B8"

                    anchors.left: parent.left
                    anchors.leftMargin: 12
                    anchors.verticalCenter: parent.verticalCenter
                }

                //用户名
                Text {
                    id: usernameText

                    text: username
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

                    text: ip
                    font.pixelSize: 12
                    color: "#6B7280"

                    anchors.left: usernameText.left
                    anchors.top: usernameText.bottom
                    anchors.topMargin: 5
                }

               //鼠标悬停处理
                HoverHandler {
                    id: hoverHandler
                    cursorShape: Qt.PointingHandCursor
                }

               //根据点击不同的用户，做出不同的操作
                TapHandler {
                    id: tapHandler
                    acceptedButtons: Qt.LeftButton
                    gesturePolicy: TapHandler.ReleaseWithinBounds

                    onTapped: {
                        //清除搜索框焦点
                        peerPanel.clearSearchFocus()

                        // 点击私聊用户时，取消群聊的本地选中状态。
                        peerPanel.currentGroupId = ""

                        //如果点击的是当前正在聊天的用户，则关闭聊天窗口，回到初始界面
                        if (peerPanel.currentPeerId === peerId) {
                            peerPanel.peerClosed()
                            console.log("关闭当前聊天窗口:", peerId, username)
                            return
                        }

                        //否则切换到该用户
                        peerPanel.peerSelected(peerId, username, ip)

                        //调试输出，后续可以删除
                        console.log("选择用户:", peerId, username, ip)
                    }
                }

                //打开用户操作菜单
                TapHandler {
                    id: rightTapHandler

                    acceptedButtons: Qt.RightButton
                    gesturePolicy: TapHandler.ReleaseWithinBounds

                    onTapped: function(eventPoint, button) {
                        peerPanel.clearSearchFocus()

                        //记录当前右键点击的用户
                        peerPanel.pendingDeletePeerId = peerItem.peerId
                        peerPanel.pendingDeletePeerName = peerItem.username

                        //eventPoint.position 是相对于 peerItem 的坐标，这里转换为相对于 peerPanel 的坐标
                        const menuPosition = peerItem.mapToItem(
                            peerPanel,
                            eventPoint.position.x,
                            eventPoint.position.y
                        )

                        peerContextMenu.x = menuPosition.x
                        peerContextMenu.y = menuPosition.y
                        peerContextMenu.open()
                    }
                }
            }

            TapHandler {
                id: clearSearchHandler

                onTapped: {
                    peerPanel.clearSearchFocus()
                }
            }
        }
    }

    //清楚搜索框焦点
    function clearSearchFocus()
    {
        searchField.focus = false
        peerPanel.forceActiveFocus()
    }

    //判断坐标是否在搜索框内容
    //sourceItem: 坐标来源对象
    function isInSearchField(sourceItem, x, y)
    {
        //坐标转化
        var pointInsearchField = searchField.mapFromItem(sourceItem, x, y)

        return pointInsearchField.x >= 0 && pointInsearchField.x <= searchField.width
                && pointInsearchField.y >= 0 && pointInsearchField.y <= searchField.height
    }

    //用户列表右键菜单。
    Menu {
        id: peerContextMenu

        width: 130

        background: Rectangle {
            implicitWidth: 100
            color: "#FFFFFF"
            radius: 6
            border.color: "#D9D9D9"
            border.width: 1
        }

        MenuItem {
            id: deleteUserMenuItem
            text: qsTr("删除用户")

            contentItem: Text {
                text: deleteUserMenuItem.text
                color: "#333333"
                font: deleteUserMenuItem.font
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideRight
            }
            onTriggered: {
                deletePeerDialog.open()
            }
        }
    }

    //群聊列表右键菜单
    Menu {
        id: groupContextMenu

        width: 180

        //显式设置菜单背景，避免暗色模式下继承系统深色背景
        //这里与用户右键菜单保持相同的浅色界面风格
        background: Rectangle {
            implicitWidth: 180
            color: "#FFFFFF"
            radius: 6
            border.color: "#D9D9D9"
            border.width: 1
        }

        MenuItem {
            id: deleteGroupMenuItem

            text: qsTr("删除群聊")

            //正常群聊触发提示窗口，已退出群聊触发删除确认窗口
            enabled: peerPanel.pendingDeleteGroupId.length > 0

            //显式设置文字颜色，避免受到系统暗色模式影响
            contentItem: Text {
                text: deleteGroupMenuItem.text
                color: deleteGroupMenuItem.enabled ? "#333333" : "#A0A0A0"
                font: deleteGroupMenuItem.font
                leftPadding: 12
                rightPadding: 12
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideRight
            }

            //显式设置菜单项悬停背景，保持与用户右键菜单一致
            background: Rectangle {
                color: deleteGroupMenuItem.highlighted && deleteGroupMenuItem.enabled
                       ? "#F2F3F5" : "transparent"
            }

            //根据群聊活动状态决定打开提示窗口还是删除确认窗口
            onTriggered: {
                //正常群聊不能直接删除，打开提示用户先退出群聊的窗口
                if (peerPanel.pendingDeleteGroupActive) {
                    activeGroupDeleteWarningDialog.open()
                    return
                }

                //已经退出的群聊可以进入彻底删除确认流程
                deleteGroupDialog.open()
            }
        }
    }
    InviteUserInterface {
        id: inviteUserInterface

        //声明式绑定，groupCandidateModel变化时会自动更新
        candidateSourceModel: peerPanel.groupCandidateModel

        //邀请窗口只产生创建请求，PeerPanel继续转发给Window.qml
        onGroupCreationRequested: function(groupName, members) {
            peerPanel.groupCreationRequested(groupName, members)
        }
    }

    Dialog {
        id: deletePeerDialog

        parent: Overlay.overlay
        anchors.centerIn: parent

        implicitWidth: 360
        modal: true
        title: qsTr("删除用户")

        standardButtons: Dialog.Ok | Dialog.Cancel

        contentItem: Text {
            text: qsTr("确定要删除用户“%1”吗?\n聊天记录也会清除").arg(peerPanel.pendingDeletePeerName)

            wrapMode: Text.Wrap
            color: "#333333"
            font.pixelSize: 14
        }

        onOpened: {
            const deleteButton = deletePeerDialog.standardButton(Dialog.Ok)

            const cancelButton = deletePeerDialog.standardButton(Dialog.Cancel)

            if (deleteButton)
                deleteButton.text = qsTr("删除")

            if (cancelButton)
                cancelButton.text = qsTr("取消")
        }

        onAccepted: {
            if (peerPanel.pendingDeletePeerId.length === 0)
                return

            peerPanel.peerDeleteRequested(
                peerPanel.pendingDeletePeerId
            )
        }

        onClosed: {
            peerPanel.pendingDeletePeerId = ""
            peerPanel.pendingDeletePeerName = ""
        }
    }

    //正常群聊点击删除时显示提示窗口
    //该窗口只负责提示用户先退出群聊，不会发出群聊删除请求
    Dialog {
        id: activeGroupDeleteWarningDialog

        parent: Overlay.overlay

        //弹窗显示时阻止用户操作后面的主窗口
        modal: true

        //让弹窗能够接收键盘焦点
        focus: true
        closePolicy: Popup.NoAutoClose

        width: 380
        height: 200

        //将弹窗放置在主窗口中央
        x: Math.round((parent.width - width) / 2)
        y: Math.round((parent.height - height) / 2)

        //取消Dialog默认边距，内部内容统一使用Layout管理
        padding: 0

        //弹窗打开后将活动焦点交给确定按钮
        onOpened: {
            activeGroupDeleteWarningConfirmButton.forceActiveFocus()
        }

        //增加半透明背景遮罩，使提示状态更加清晰
        Overlay.modal: Rectangle {
            color: "#80000000"
        }

        //显式设置浅色弹窗背景，避免暗色模式改变弹窗颜色
        background: Rectangle {
            radius: 10
            color: "#FFFFFF"
            border.width: 1
            border.color: "#DCDCDC"
        }

        contentItem: ColumnLayout {
            spacing: 0

            //弹窗标题区域
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 48

                color: "#F5F5F5"
                radius: 10

                Text {
                    width: parent.width
                    height: parent.height
                    leftPadding: 18

                    text: qsTr("无法删除群聊")
                    color: "#222222"
                    font.pixelSize: 16
                    font.bold: true

                    horizontalAlignment: Text.AlignLeft
                    verticalAlignment: Text.AlignVCenter
                }
            }

            //弹窗提示内容
            Text {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.leftMargin: 20
                Layout.rightMargin: 20
                Layout.topMargin: 16
                Layout.bottomMargin: 12

                text: qsTr("群聊“%1”尚未退出，无法删除。\n请先退出群聊，再删除群聊。")
                      .arg(peerPanel.pendingDeleteGroupName)

                color: "#333333"
                font.pixelSize: 14
                wrapMode: Text.WordWrap
                verticalAlignment: Text.AlignVCenter
            }

            //弹窗底部按钮区域
            RowLayout {
                Layout.fillWidth: true
                Layout.preferredHeight: 54
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                Layout.bottomMargin: 8

                //使用弹性占位项将确定按钮放到右侧
                Item {
                    Layout.fillWidth: true
                }

                Rectangle {
                    id: activeGroupDeleteWarningConfirmButton

                    Layout.preferredWidth: 70
                    Layout.preferredHeight: 30

                    //允许按钮获得键盘焦点
                    activeFocusOnTab: true

                    radius: 8
                    color: activeGroupDeleteWarningConfirmHover.hovered ? "#168FE5" : "#029AFF"

                    Text {
                        width: parent.width
                        height: parent.height

                        text: qsTr("确定")
                        color: "#FFFFFF"
                        font.pixelSize: 13

                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }

                    HoverHandler {
                        id: activeGroupDeleteWarningConfirmHover
                        cursorShape: Qt.PointingHandCursor
                    }

                    TapHandler {
                        acceptedButtons: Qt.LeftButton
                        gesturePolicy: TapHandler.ReleaseWithinBounds

                        //点击确定后只关闭提示窗口，不执行删除
                        onTapped: {
                            activeGroupDeleteWarningDialog.close()
                        }
                    }

                    //按钮获得焦点后，回车关闭窗口
                    Keys.onPressed: function(event) {
                        if (event.key === Qt.Key_Return) {
                            activeGroupDeleteWarningDialog.close()
                            event.accepted = true
                        }
                    }
                }
            }
        }

        //提示窗口关闭后清除右键操作保存的临时群聊信息
        onClosed: {
            peerPanel.pendingDeleteGroupId = ""
            peerPanel.pendingDeleteGroupName = ""
            peerPanel.pendingDeleteGroupActive = true
        }
    }

    Dialog {
        id: deleteGroupDialog

        parent: Overlay.overlay

        //弹窗显示时阻止用户继续操作后面的主窗口
        modal: true

        //让弹窗可以接收键盘焦点
        focus: true

        width: 380
        height: 210

        //将弹窗放置在主窗口中央
        x: Math.round((parent.width - width) / 2)
        y: Math.round((parent.height - height) / 2)

        //取消Dialog默认内容边距，由内部ColumnLayout统一管理
        padding: 0

        //弹窗打开后等待Dialog完成焦点切换，再把活动焦点交给删除按钮
        onOpened: {
            Qt.callLater(function() {
                deleteGroupConfirmButton.forceActiveFocus()
            })
        }

        //统一处理鼠标点击删除和键盘回车删除
        function confirmDeleteGroup() {
            //没有有效群聊ID时不执行删除
            if (peerPanel.pendingDeleteGroupId.length === 0)
                return

            //正常群聊必须先退出，不能直接彻底删除
            if (peerPanel.pendingDeleteGroupActive)
                return

            //进入Dialog的accepted流程，由onAccepted发送删除请求
            deleteGroupDialog.accept()
        }

        //设置弹窗外部半透明遮罩，使删除确认状态更加明显
        Overlay.modal: Rectangle {
            color: "#80000000"
        }

        background: Rectangle {
            radius: 10
            color: "#FFFFFF"
            border.color: "#DCDCDC"
            border.width: 1
        }

        contentItem: ColumnLayout {
            spacing: 0

            //弹窗标题区域
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 48

                color: "#F5F5F5"
                radius: 10

                Text {
                    width: parent.width
                    height: parent.height
                    leftPadding: 18

                    text: qsTr("删除群聊")
                    color: "#222222"
                    font.pixelSize: 16
                    font.bold: true

                    horizontalAlignment: Text.AlignLeft
                    verticalAlignment: Text.AlignVCenter
                }
            }

            //弹窗提示内容
            Text {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.leftMargin: 20
                Layout.rightMargin: 20
                Layout.topMargin: 16
                Layout.bottomMargin: 12

                text: qsTr("确定要删除群聊“%1”吗？\n群成员和聊天记录都会彻底清除")
                      .arg(peerPanel.pendingDeleteGroupName)

                color: "#333333"
                font.pixelSize: 14
                wrapMode: Text.WordWrap
                verticalAlignment: Text.AlignVCenter
            }

            //弹窗底部按钮区域
            RowLayout {
                Layout.fillWidth: true
                Layout.preferredHeight: 58
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                Layout.bottomMargin: 8

                spacing: 10

                //使用弹性占位项将操作按钮放置在右侧
                Item {
                    Layout.fillWidth: true
                }

                //取消按钮
                Rectangle {
                    id: deleteGroupCancelButton

                    Layout.preferredWidth: 76
                    Layout.preferredHeight: 32

                    radius: 8
                    color: deleteGroupCancelHover.hovered ? "#EEEEEE" : "#F8F8F8"
                    border.color: "#C2C2C2"
                    border.width: 1

                    Text {
                        width: parent.width
                        height: parent.height

                        text: qsTr("取消")
                        color: "#333333"
                        font.pixelSize: 14

                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }

                    HoverHandler {
                        id: deleteGroupCancelHover
                        cursorShape: Qt.PointingHandCursor
                    }

                    TapHandler {
                        acceptedButtons: Qt.LeftButton
                        gesturePolicy: TapHandler.ReleaseWithinBounds

                        onTapped: deleteGroupDialog.reject()
                    }
                }

                //删除按钮
                Rectangle {
                    id: deleteGroupConfirmButton

                    Layout.preferredWidth: 76
                    Layout.preferredHeight: 32

                    //通过Tab键获得焦点
                    activeFocusOnTab: true

                    radius: 8
                    color: deleteGroupConfirmHover.hovered ? "#C92F2F" : "#E13B3B"

                    Text {
                        width: parent.width
                        height: parent.height

                        text: qsTr("删除")
                        color: "#FFFFFF"
                        font.pixelSize: 14

                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }

                    HoverHandler {
                        id: deleteGroupConfirmHover
                        cursorShape: Qt.PointingHandCursor
                    }

                    TapHandler {
                        acceptedButtons: Qt.LeftButton
                        gesturePolicy: TapHandler.ReleaseWithinBounds

                        onTapped: deleteGroupDialog.confirmDeleteGroup()
                    }

                    //按钮获得活动焦点后，Enter确认删除
                    Keys.onPressed: function(event) {
                        if (event.key === Qt.Key_Return) {
                            event.accepted = true
                            deleteGroupDialog.confirmDeleteGroup()
                        }
                    }
                }
            }
        }

        //弹窗确认关闭后，将删除请求发送给Window.qml
        onAccepted: {
            peerPanel.groupDeleteRequested(
                peerPanel.pendingDeleteGroupId
            )
        }

        //弹窗关闭后清除临时保存的群聊信息
        onClosed: {
            peerPanel.pendingDeleteGroupId = ""
            peerPanel.pendingDeleteGroupName = ""
            peerPanel.pendingDeleteGroupActive = true
        }
    }

    DragHandler{
        target: null

        onActiveChanged: {
            if (active)
               root.startSystemMove()
        }
    }
}