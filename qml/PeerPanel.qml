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

    //选择群聊时只传稳定的群ID和群名称
    //成员由AppController根据groupId从数据库读取
    signal groupSelected(string groupId,string groupName)

    //再次点击当前群聊时关闭群会话
    signal groupClosed()

    //当前右键选中的待删除用户。
    property string pendingDeletePeerId: ""
    property string pendingDeletePeerName: ""

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
        //通知Window.qml打开新创建的群聊
        peerPanel.groupSelected(groupId, groupName)
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

                        MenuItem{
                            text: qsTr("创建群聊")
                            //MenuItem本身就是可以点击的控件 并自带triggered信号,所以不应该用TapHandler
                            onTriggered: {
                                peerPanel.clearSearchFocus()
                                inviteUserInterface.show()
                            }

                        }
                        MenuItem{
                            text: qsTr("加好友")
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

            delegate: RowLayout {
                id: groupDelegate

                required property var modelData

                readonly property string groupId: String(modelData.groupId)
                readonly property string groupName: String(modelData.groupName)
                readonly property int memberCount: Number(modelData.memberCount)
                readonly property string memberSummary: String(modelData.memberSummary)
                readonly property bool matched: peerPanel.matchSearch(groupName, memberSummary)

                width: ListView.view.width
                height: matched ? 56 : 0
                visible: matched
                spacing: 0

                Rectangle {
                    id: groupItem

                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.leftMargin: 10
                    Layout.rightMargin: 10

                    color: peerPanel.currentGroupId === groupDelegate.groupId ? "#E8F3FF" : groupHoverHandler.hovered ? "#EEEEEE" : "#F5F5F5"

                    border.width: 1

                    border.color:
                        peerPanel.currentGroupId === groupDelegate.groupId ? "#9BCBFF" : groupHoverHandler.hovered ? "#D0D0D0" : "#E0E0E0"

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
                            //再次点击当前群聊时关闭会话
                            if (peerPanel.currentGroupId === groupDelegate.groupId) {
                                peerPanel.groupClosed()
                                return
                            }

                            //通知Window.qml打开选中的群聊
                            peerPanel.groupSelected(groupDelegate.groupId, groupDelegate.groupName)
                        }
                    }
                }

                Item {
                    Layout.preferredWidth: 10
                }
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

        MenuItem {
            text: qsTr("删除用户")

            onTriggered: {
                deletePeerDialog.open()
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
            text: qsTr("确定要删除用户“%1”吗?\n聊天记录也会清除")
                  .arg(peerPanel.pendingDeletePeerName)

            wrapMode: Text.Wrap
            color: "#333333"
            font.pixelSize: 14
        }

        onOpened: {
            const deleteButton =
                    deletePeerDialog.standardButton(Dialog.Ok)

            const cancelButton =
                    deletePeerDialog.standardButton(Dialog.Cancel)

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

    DragHandler{
        target: null

        onActiveChanged: {
            if (active)
               root.startSystemMove()
        }
    }
}