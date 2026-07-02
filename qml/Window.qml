// Module
// File: Window.qml   Version: 0.1.0   License: AGPLv3
// Created: HeZhiyuan      2026-06-05 19:33:19
// Description:存放真正的窗口内容，负责整体布局和状态
//
//     [v0.1.2] HeZhiyuan    2026-06-05 21:22:32
//         * 加上底部输入框组件。
//     [v0.1.3] HeZhiyuan    2026-06-13 16:45:05
//         * 新增AppController QML对象。
//           用户模型改为appController.peers。
//           消息模型改为appController.messages。
//           发送消息改为调用appController.sendMessage()。
//           选择用户时调用appController.selectPeer()读取历史消息。
//           关闭聊天时调用appController.clearConversation()。
//           删除已移除的chatMessageModel引用。
//     [v0.1.4] HeZhiyuan    2026-06-14 15:06:58
//         * 修改模块注册
//     [v0.1.5] JiangFan    2026-06-14
//         * 修复全局焦点处理bug
//     [v0.1.5] HeZhiyuan    2026-06-14 16:14:29
//         * 删除成功后，清理QML的当前用户状态
//     [v0.1.6] JiangFan    2026-06-14 23:28
//         * 增加处理文件传输的弹窗及其功能
//     [v0.1.7] JiangFan    2026-06-18
//         * 重构：使用Layout管理主窗口结构
//     [v0.1.8] ZhouChengWei    2026-06-22 10.17
//         * 实现了判断文件大小（B/KB/MB/GB），替代了接收文件时一直显示字节
//         * 添加了接受文件时显示对方名字
//     [v0.1.9] JiangFan    2026-06-22
//         * 增加顶部的、自己的菜单栏
//     [v0.2.0] JiangFan    2026-06-22
//         * 对整个窗口添加拖拽、放大、缩小，拉伸等功能，增加登录界面，可以一开始自定义名字
//     [v0.2.1] JiangFan    2026-06-23
//         * 增加群聊的右侧用户列表
//     [v0.2.2] HeZhiyuan    2026-06-23
//         * 接通群聊创建界面与AppController的调用流程
//     [v0.2.3] JiangFan   2026-06-24
//         *增加群聊成员悬停2s显示成员资料卡的功能
//         *修复更改名字左边用户列表名字不同步的问题
//     [v0.2.4] HeZhiyuan    2026-06-25
//         * 分离当前私聊状态和当前群聊状态，不再将群聊ID保存到currentPeerId
//     [v0.2.5] HeZhiyuan    2026-06-26
//         * 将登录功能分离出去，用Login.qml单独管理
//     [v0.2.6] HeZhiyuan    2026-06-26
//         * 调整程序启动时的登录窗口显示流程
//           主窗口在登录成功前保持隐藏，登录成功后通过isLogin显示
//     [v0.2.7] ZhouChengWei    2026-06-26
//         * 修改了群聊界面群成员收起/展开按钮的样貌
//         * 不是群聊界面取消了文件按钮
//     [v0.2.8] ZhouChengWei    2026-06-27
//         * 添加了群聊详情界面打开，实现退出群聊
//         * 添加了退出群聊时提示是否要退出的对话框
//         * 对退出群聊确定按钮做了默认焦点处理，现在按enter就能退出
//     [v0.2.9] HeZhiyuan    2026-06-27 23:48:52
//         * 优化退出群聊时焦点选择以及按键处理
//     [v0.3.0] HeZhiyuan    2026-06-28 20:45:17
//         * 修改在暗色模式下，退出群聊的显示
//     [v0.3.1] HeZhiyuan    2026-06-29 23:53:46
//         * 增加当前群聊活动状态管理
//           退出群聊后保留历史消息并切换为只读显示状态
//           退出群聊后隐藏消息输入框
//     [v0.3.2] JiangFan    2026-06-30
//         * 将用户列表做成单独文件，成为可以复用的控件，并在群聊详情界面复用
//         * 修复：详情界面按钮点击收缩时无法收缩的bug（收缩一点后又展开）
<<<<<<< HEAD
//     [v0.3.3] JiangFan    2026-06-30
//         * 完成文件传输进度条显示在聊天栏
//

=======
//     [v0.3.3] HeZhiyuan    2026-07-02 00:55:32
//         * 接收群聊状态变化，退群或解散后保留当前历史消息并切换为只读状态
//           解散群聊会隐藏退出按钮，同时关闭群聊详情界面并清空消息输入框
>>>>>>> 4c34a4ba0b04faea4c11f9a2bd0919b355840023
import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import se.qt.messager
import QtQuick.Layouts
import QtQuick.Shapes

ApplicationWindow {
    id: root

    //启动登录流程
    Component.onCompleted: {
        loginWindow.beginLogin()
    }

    width: 1000
    height: 700
    minimumWidth: 800
    minimumHeight: 500

    visible: isLogin

    title: "Messager 信使"
    flags: Qt.Window | Qt.FramelessWindowHint //隐藏顶部默认菜单栏，但是需要自己实现窗口缩放（暂时还没有实现）

    property bool isLogin: false //登录状态

    //自己的信息
    property string myName: ""
    property string myIp: ""

    //当前正在聊天的局域网用户信息
    property string currentPeerId: ""
    property string currentPeerName: ""
    property string currentPeerIp: ""

    //群聊信息
    property string currentGroupId: ""
    property string currentGroupName: ""
    property bool currentGroupActive: false
    property bool currentIsGroup: false //当前会话是否为群聊
    property bool currentIsShrink: false //当前右侧用户列表是否收缩
    //当前界面是否存在活动会话
    readonly property bool hasActiveConversation: currentIsGroup ? currentGroupId.length > 0
                                                                 : currentPeerId.length > 0
    //当前会话ID
    readonly property string currentConversationId: currentIsGroup ? currentGroupId : currentPeerId
    //当前会话标题，私聊显示用户名，群聊显示群名
    readonly property string currentConversationName: currentIsGroup ? currentGroupName : currentPeerName

    //当前待处理的文件接收请求
    property string pendingFileIp: ""
    property string pendingFileName: ""
    property double pendingFileSize: 0

    //文件传输状态显示
    property int fileTransferPercent: 0
    property string fileTransferStatusText: ""
    property string fileTransferName: "" //传输的文件名
    property bool fileTransferVisible: false //进度条的可见性
    property bool fileTransferFromMe: true  //进度条的归属行

    //C++应用控制器：负责数据库、用户发现和消息收发，QML只处理界面状态
    AppController {
        id: appController

        //删除成功后，再清理QML的当前用户状态。
        onPeerDeleted: function(peerId) {
            if (root.currentPeerId !== peerId)
                return

            root.currentPeerId = ""
            root.currentPeerName = ""
            root.currentPeerIp = ""

            inputPanel.clear()

            console.log("已删除当前聊天用户:", peerId)
        }

        //彻底删除群聊成功后，再清理QML保存的当前群聊状态
        onGroupDeleted: function(groupId) {
            if (root.currentGroupId !== groupId)
                return

            root.closeGroupChat()
            groupDetailDrawer.close()
        }

        //群聊退出或解散后仍保留历史记录，只把当前会话切换为只读状态
        onGroupActivityChanged: function(groupId, active) {
            if (root.currentGroupId !== groupId)
                return

            root.currentGroupActive = active

            if (!active) {
                inputPanel.clear()
                groupDetailDrawer.close()
            }
        }

        //收到对方文件发送请求
        onFileRequestReceived: function(fromIp, fileName, fileSize)
        {
            //如果文件请求来自本机，直接忽略。
            //自己给自己发文件时，本地sendFile已经保存了聊天记录，不需要再接收一次。
            if (fromIp === appController.localIp()) {
                console.log("忽略本机文件请求:", fromIp, fileName)
                return
            }

            root.pendingFileIp = fromIp
            root.pendingFileName = fileName
            root.pendingFileSize = fileSize
            root.fileTransferPercent = 0

            //图片文件自动接收，不弹出接收/拒绝面板
            if (root.isImageFileName(fileName)) {
                root.fileTransferStatusText = qsTr("正在自动接收图片")
                receiveFilePanel.visible = false

                console.log("收到图片文件请求，自动接收:", fromIp, fileName, fileSize)
                appController.acceptImageFile(fromIp, fileName)

                return
            }

            //普通文件才弹出接收/拒绝面板
            root.fileTransferStatusText = qsTr("收到文件请求")
            receiveFilePanel.visible = true

            console.log("收到普通文件请求", fromIp, fileName, fileSize)
        }

        //文件传输进度
        onFileTransferProgress: function(ip, fileName, percent) {
            root.fileTransferPercent = percent
            root.fileTransferName = fileName
            root.fileTransferVisible = true
            root.fileTransferStatusText = qsTr("文件传输中: ") + percent + "%"
        }

        //文件传输完成
        onFileTransferFinished: function(ip, fileName, success) {
            root.fileTransferPercent = success ? 100 : root.fileTransferPercent
            root.fileTransferStatusText = success
                ? qsTr("文件传输完成: ") + fileName
                : qsTr("文件传输失败: ") + fileName

            //传输结束后，隐藏进度条，其他初始化
            root.fileTransferVisible = false
            root.fileTransferName = ""
            root.fileTransferPercent = 0
        }

        //错误提示
        onOperationFailed: function(message)
        {
            console.log("操作失败: ", message)
            root.fileTransferStatusText = message
        }
    }

    //判断文件大小，用于转换B/KB/MB/GB
    function fileSizeJudgement(fileSize){
        var size = 0
        //B
        if(fileSize < 1024){
            size = fileSize
            return size.toFixed(1) + " B"
        }

        //KB
        if(fileSize < 1024 * 1024){
            size = fileSize / 1024
            return size.toFixed(1) + " KB"
        }
        //MB
        if(fileSize < 1024 * 1024 * 1024){
            size = fileSize / (1024 * 1024)
            return size.toFixed(1) + " MB"
        }
        //GB
        if(fileSize < 1024 * 1024 * 1024 * 1024){
            size = fileSize / (1024 * 1024 * 1024)
            return size.toFixed(1) + " GB"
        }
    }

    //判断图片的函数
    function isImageFileName(fileName)
    {
        var name = String(fileName).toLowerCase()

        return name.endsWith(".png")
            || name.endsWith(".jpg")
            || name.endsWith(".jpeg")
            || name.endsWith(".bmp")
            || name.endsWith(".gif")
            || name.endsWith(".webp")
    }

    //自己给自己发送普通文件时使用的本地保存进度
    Timer {
        id: localFileProgressTimer

        interval: 20
        repeat: true

        onTriggered: {
            if (root.fileTransferPercent < 100) {
               root.fileTransferPercent += 10
               return
           }

           stop()

           //文件保存完毕 —-> 隐藏进度条。
           root.fileTransferVisible = false
           root.fileTransferPercent = 0
           root.fileTransferName = ""
       }
    }

    //从fileUrl中提取文件名
    function fileNameFromUrl(fileUrl)
    {
        var text = fileUrl.toString()

        if (text.length === 0)
            return ""

        var index = text.lastIndexOf("/")

        if (index < 0)
            return decodeURIComponent(text)

        return decodeURIComponent(text.substring(index + 1))
    }

    //校验消息并根据当前会话类型交给C++发送
    function trySendMessage(content)
    {
        const normalizedContent = content.trim()  //去掉消息首尾空白

        //空消息不发送
        if (normalizedContent.length === 0)
            return

        //当前是群聊时使用群ID发送
        if (root.currentIsGroup) {
            //群ID为空说明当前群会话无效
            if (root.currentGroupId.length === 0)
                return

            //已经退出的群聊只用于查看历史记录，不能再从输入框发送消息
            if (!root.currentGroupActive)
                return

            //网络发送和数据库保存交给AppController
            appController.sendGroupMessage(root.currentGroupId, normalizedContent)
            return
        }
        //私聊必须存在有效用户ID
        if (root.currentPeerId.length === 0)
            return

        //把私聊发送请求交给AppController
        appController.sendMessage(root.currentPeerId, root.currentPeerName, root.currentPeerIp, normalizedContent)
    }

    //发送文件
    function trySendFile(fileUrl) {
        if (root.currentIsGroup) {
            console.log("当前群聊还不支持发文件")
            return
        }

        if (root.currentPeerId === "") {
            console.log("请先选择聊天对象")
            return
        }

        if (fileUrl.toString().length === 0) {
            console.log("文件路径为空")
            return
        }

        //从fileUrl中取出文件名。
        var fileName = root.fileNameFromUrl(fileUrl)

        //判断当前选择的文件是否为图片。
        var isImageFile = root.isImageFileName(fileName)

        //判断是否是自己给自己发送。
        var isSendToSelf = root.currentPeerIp === appController.localIp()

        //进度条。
        if (!isImageFile) {
            root.fileTransferName = fileName
            root.fileTransferFromMe = true
            root.fileTransferPercent = 0
            root.fileTransferVisible = true
        } else {
            root.fileTransferName = ""
            root.fileTransferPercent = 0
            root.fileTransferVisible = false
        }

        appController.sendFile(root.currentPeerId, root.currentPeerName, root.currentPeerIp, fileUrl)

        if (!isImageFile && isSendToSelf) {
            root.startLocalFileProgress()
        }
    }

    //自己给自己发送文件的进度
    function startLocalFileProgress()
    {
        localFileProgressTimer.stop()

        root.fileTransferPercent = 0
        root.fileTransferVisible = true

        localFileProgressTimer.start()
    }

    //窗口缩放函数
    function toggleMaxinized()
    {
        if(root.visibility === Window.Maximized)
            root.showNormal()
        else
            root.showMaximized()
    }

    //打开指定群聊并加载群成员和群消息
    function openGroupChat(groupId, groupName, groupActive)
    {
        //没有有效群ID时不切换界面
        if (groupId.length === 0)
            return

        //进入群聊前清理私聊状态
        root.currentPeerId = ""
        root.currentPeerName = ""
        root.currentPeerIp = ""
        appController.clearConversation()

        //保存当前群聊状态
        root.currentGroupId = groupId
        root.currentGroupName = groupName
        root.currentGroupActive = groupActive
        root.currentIsGroup = true
        root.currentIsShrink = false

        //清空上一个会话的输入内容
        inputPanel.clear()

        //通知C++读取群成员和群聊历史消息
        appController.selectGroup(groupId)
    }

    //关闭当前群聊并清理群聊界面状态
    function closeGroupChat() {
        root.currentGroupId = ""
        root.currentGroupName = ""
        root.currentGroupActive = false
        root.currentIsGroup = false
        root.currentIsShrink = false

        inputPanel.clear()
        appController.clearGroupConversation()
    }
    //修改本机用户名
    function changeMyName(newName)
    {
        var name = newName.trim()

        if (name.length === 0)
        {
            myNameEdit.text = root.myName
            return
        }

        if (name === root.myName)
        {
            myNameEdit.text = root.myName
            return
        }

        if (!appController.updateMyName(name))
        {
            myNameEdit.text = root.myName
            return
        }

        root.myName = name
        myNameEdit.text = name

        console.log("用户名写入数据库成功：", name)
    }

    //登录窗口
    Login {
        id: loginWindow

        //登录组件不会再次创建数据库和网络控制器
        controller: appController

        //接收Login.qml登录成功后返回的用户名和本机IP
        onLoginSucceeded: function(userName, localIp) {
            root.myName = userName

            root.myIp = localIp

            //标记已经登录
            root.isLogin = true

            console.log("登录成功！用户名：", userName, "本机IP：", localIp)
        }
    }

    Rectangle {
        id: background
        anchors.fill: parent
        color: "white"

        visible: isLogin

        ColumnLayout {
            id: mainLayout

            anchors.fill: parent
            spacing: 0

            //顶部个人栏
            Rectangle {

                border.color: "#e5e5e5"
                border.width: 1

                Layout.preferredHeight: 40
                //Layout.height: 30
                Layout.minimumHeight: 40
                Layout.maximumHeight: 40
                Layout.fillWidth: true

                RowLayout {
                    id: personLayout

                    anchors.fill: parent

                    spacing: 0

                    //头像（类似于inviteUserInterface.qml 500)
                    Rectangle {
                        id: headPortrait

                        Layout.preferredHeight: 28
                        Layout.preferredWidth: 28
                        Layout.leftMargin: 5
                        Layout.alignment: Qt.AlignVCenter

                        color: "#b2e4ff"
                        radius: 5

                        Text {
                            width:parent.width
                            height: parent.height

                            text: myName.length > 0 ? myName.charAt(0) : "?"
                            color: "#1c82ff"
                            font.pixelSize: 23

                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                    }

                    TextField {
                        id:myNameEdit

                        color: "black"

                        text: myName
                        font.pixelSize: 20
                        Layout.leftMargin: 10
                        Layout.preferredHeight: 30

                        Layout.preferredWidth: myNameEdit.contentWidth + 20 //加上20留点空
                        Layout.minimumWidth: 30
                        Layout.maximumWidth: 300

                        //单行
                        selectByMouse: true

                        //外观处理：看起来更像普通文字
                        background: Rectangle {
                            color: myNameEdit.activeFocus ?  "#F5F7FA" : "transparent"
                            radius: 4
                            border.color: myNameEdit.activeFocus ? "#12B7F5" : "transparent"
                            border.width: 1
                        }

                        //回车确认修改(这里不用onAccepted了，用更高级的，可以对失去焦点响应)
                        onEditingFinished: {
                            root.changeMyName(myNameEdit.text)
                        }
                    }


                    // Text {
                    //    id: myNametext
                    //    text: myName
                    //    font.pixelSize: 20
                    //    Layout.leftMargin: 10
                    // }

                    Text {
                        id: myIpText
                        text: "ip:" + myIp
                        font.pixelSize: 15
                        Layout.leftMargin: 10
                        Layout.topMargin: 5
                    }

                    //标题栏空白拖拽区域
                    Rectangle {
                        id: titleDragArea

                        Layout.fillHeight: true
                        Layout.fillWidth: true

                        DragHandler{
                            target: null

                            onActiveChanged: {
                                if (active)
                                    root.startSystemMove()
                            }
                        }
                    }

                    //功能栏（最小化，最大化，关闭）
                    //最小化
                    Rectangle{
                        id: smallerButton

                        Layout.fillHeight: true
                        Layout.preferredWidth: 30
                        Layout.rightMargin: 5

                        color: smallerButtonHover.hovered ? "#F2F3F5" : "#FFFFFF"

                        Image {
                            source: "source/smaller.svg"

                            width: 20
                            height: 20
                            anchors.centerIn: parent
                        }

                        HoverHandler {
                            id: smallerButtonHover
                            cursorShape: Qt.PointingHandCursor
                        }

                        TapHandler {
                            onTapped: {
                                root.showMinimized()
                            }
                        }
                    }

                    //最大化
                    Rectangle{
                        id: biggerButton

                        Layout.fillHeight: true
                        Layout.preferredWidth: 30
                        Layout.rightMargin: 5

                        color: biggerButtonHover.hovered ? "#F2F3F5" : "#FFFFFF"

                        Image {
                            source: "source/bigger.svg"

                            visible: root.visibility !== Window.Maximized
                            width: 20
                            height: 20
                            anchors.centerIn: parent
                        }

                        Image {
                            source: "source/bigger2.svg"

                            visible: root.visibility === Window.Maximized
                            width: 20
                            height: 20
                            anchors.centerIn: parent
                        }

                        HoverHandler {
                            id: biggerButtonHover
                            cursorShape: Qt.PointingHandCursor

                        }

                        TapHandler {
                            onTapped: {
                                root.toggleMaxinized()
                            }
                        }
                    }

                    //关闭
                    Rectangle{
                        id: closeButton

                        Layout.fillHeight: true
                        Layout.preferredWidth: 30
                        Layout.rightMargin: 5

                        color: closeButtonHover.hovered ? "#e60a0a" : "#FFFFFF"

                        Image {
                            source: "source/close.svg"

                            width: 20
                            height: 20
                            anchors.centerIn: parent
                        }

                        HoverHandler {
                            id: closeButtonHover
                            cursorShape: Qt.PointingHandCursor
                        }

                        TapHandler {

                            onTapped: {
                                root.close()
                            }
                        }
                    }
                }
            }

            RowLayout {
                id: activeLayout //凑合叫活动窗口

                Layout.fillWidth: true
                Layout.fillHeight: true

                spacing: 0

                PeerPanel {
                    id: peerPanel

                    Layout.preferredWidth: 200
                    Layout.fillHeight: true

                    //Window.qml当前选中的用户id传给PeerPanel，用于左侧高亮
                    currentPeerId: root.currentPeerId

                    currentGroupId: root.currentGroupId
                    groupCandidateModel: appController.groupCandidates
                    groupModel: appController.groups
                    peerModel: appController.peers

                    //接收创建群聊窗口提交的群名称和成员列表
                    onGroupCreationRequested: function(groupName, members)
                    {
                        //群聊ID生成、数据库保存和网络通知统一交给C++
                        const groupId = appController.createGroup(groupName, members)

                        //返回空ID表示创建失败
                        if (groupId.length === 0) {
                            peerPanel.finishGroupCreation(false, "", groupName)
                            return
                        }

                        //前面已经确认ID有效，因此成功状态直接传入true
                        peerPanel.finishGroupCreation(true, groupId, groupName)
                    }

                    //     [v0.1.2] HeZhiyuan    2026-06-03 16:37:40
                    //         * 用户点击左侧列表项后，这里会被调用
                    //并通知AppController从数据库读取该用户的历史消息
                    onPeerSelected: function(peerId, username, ip)
                    {
                        //从群聊切换到私聊时先清理群聊状态
                        if (root.currentIsGroup){
                            root.closeGroupChat()
                        }
                        //切换到另一个用户时，先清空当前前端消息
                        if (root.currentPeerId !== peerId){
                            inputPanel.clear()
                        }

                        //保存当前会话所需的用户信息
                        root.currentPeerId = peerId
                        root.currentPeerName = username
                        root.currentPeerIp = ip
                        //通知控制器读取该用户的历史消息。
                        appController.selectPeer(peerId)
                        console.log("Main.qml 当前聊天对象:", peerId, username, ip)
                    }

                    //     [v0.1.2] HeZhiyuan    2026-06-04 20:47:45
                    //         * 再次点击当前用户会关闭聊天窗口
                    onPeerClosed: {
                        root.currentPeerId = ""
                        root.currentPeerName = ""
                        root.currentPeerIp = ""

                        //关闭聊天窗口时清空前端消息
                        inputPanel.clear()
                        //清除控制器当前聊天对象和前端消息属性。
                        appController.clearConversation()
                        console.log("Main.qml 已回到初始界面")
                    }

                    onPeerDeleteRequested: function(peerId) {
                        appController.deletePeer(peerId)
                    }

                    //接收PeerPanel发送的  搜索框改变  的信号
                    onSearchTextChanged: function(keyword) {
                        //待改
                        console.log("搜索用户", keyword)
                    }

                    //接收PeerPanel发出的群聊选择信号
                    onGroupSelected: function(groupId, groupName, groupActive) {
                        root.openGroupChat(groupId, groupName, groupActive)
                    }
                    //接收PeerPanel发出的群聊彻底删除请求
                    onGroupDeleteRequested: function(groupId) {
                        appController.deleteExitedGroup(groupId)
                    }
                    onGroupClosed: {
                        root.closeGroupChat()
                    }


                }

                Rectangle {
                    id: rightPanel

                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    color: "#FFFFFF"

                    ColumnLayout {
                        id: rightLayout

                        anchors.fill: parent
                        spacing: 0

                        Item {
                            id: rightArea

                            Layout.fillHeight: true
                            Layout.fillWidth: true

                            //没有选择用户时显示提示区域
                            Rectangle {
                                id: emptyPanel

                                anchors.fill: parent

                                color: "#FAFAFA"
                                visible: !root.hasActiveConversation

                                Text {
                                    text: qsTr("点击左侧用户开始聊天")
                                    font.pixelSize: 16
                                    color: "#999999"
                                    anchors.centerIn: parent
                                }
                            }

                            RowLayout {
                                id: chatContentLayout

                                anchors.fill:parent
                                spacing: 0

                                visible: root.hasActiveConversation

                                //右侧聊天窗口：点击左侧用户后才显示
                                ChatPanel {
                                    id: chatPanel

                                    Layout.fillWidth: true
                                    Layout.fillHeight: true

                                    currentPeerId: root.currentConversationId
                                    currentPeerName: root.currentConversationName
                                    isGroupChat: root.currentIsGroup
                                    messageModel: root.currentIsGroup
                                                  ? appController.groupMessages
                                                  : appController.messages
                                    appController: appController

                                    //传递文件传输信息
                                    fileTransferVisible: root.fileTransferVisible
                                    fileTransferName: root.fileTransferName
                                    fileTransferPercent: root.fileTransferPercent
                                    fileTransferFromMe: root.fileTransferFromMe
                                }

                                //群成员列表
                                Rectangle {
                                    id: groupMemberPanel

                                    Layout.preferredWidth: 150
                                    Layout.topMargin: 50 //这个数值跟ChatPanel.qml的顶部栏高度一致
                                    Layout.fillHeight: true

                                    visible: root.currentIsGroup && !root.currentIsShrink

                                    color: "#FAFAFA"
                                    border.color: "#e0e0e0"
                                    border.width: 1

                                    //目前用来隐藏、显示收缩按钮
                                    HoverHandler {
                                        id: groupMemberHover
                                    }

                                    ColumnLayout {
                                        anchors.fill: parent
                                        spacing: 0

                                        //顶部标题
                                        Rectangle {
                                            Layout.fillWidth: true
                                            Layout.preferredHeight: 40

                                            color: "#FAFAFA"

                                            Text {
                                                text: qsTr("群聊成员: ") + appController.groupMembers.length
                                                font.pixelSize: 13
                                                color: "black"

                                                anchors.left: parent.left
                                                anchors.leftMargin: 10
                                                anchors.verticalCenter: parent.verticalCenter
                                            }
                                        }

                                        //成员列表
                                        GroupMemberListView {
                                            id: groupMemberListView

                                            Layout.fillHeight: true
                                            Layout.fillWidth: true
                                        }
                                    }
                                }
                            }

                            //群聊右侧用户列表收缩按钮
                            Shape {
                                id: shrinkButton
                                opacity: shrinkButtonHover.hovered ? 0.8 : 0.5
                                height: 40; width: 10; z:2

                                visible: currentIsGroup && root.currentGroupId !== ""
                                    && (groupMemberHover.hovered || shrinkButtonHover.hovered ||
                                        root.currentIsShrink)
                                    //不加按钮的这个悬浮的话，准备点击的时候会瞬间消失 OvO
                                anchors.verticalCenter: parent.verticalCenter
                                anchors.right: parent.right
                                anchors.rightMargin: root.currentIsShrink ? 2 : 150

                                ShapePath {
                                    strokeWidth: 1
                                    strokeColor: "#e2e2e2"
                                    fillColor: "#e2e2e2"
                                    startX: 0; startY: 10      // 左底上端点（窄端上角）
                                    PathLine { x: 10; y: 0 }   // 右底上端点（宽端上角）— 上腰
                                    PathLine { x: 10; y: 40 }  // 右底下端点（宽端下角）— 右底边
                                    PathLine { x: 0; y: 30 }   // 左底下端点（窄端下角）— 下腰
                                    PathLine { x: 0; y: 10 }
                                }

                                Text {
                                    anchors.centerIn: parent
                                    text: currentIsShrink ? ">" : "<"
                                    font.pixelSize: 11
                                }

                                HoverHandler {
                                    id: shrinkButtonHover
                                    cursorShape: Qt.PointingHandCursor
                                }

                                TapHandler {
                                    onTapped: {
                                        root.currentIsShrink = !currentIsShrink
                                    }
                                }
                            }

                        }

                        //底部输入框：点击左侧用户后才显示
                        InputPanel {
                            id: inputPanel

                            Layout.fillWidth: true
                            Layout.preferredHeight: 200

                            //私聊和活动群聊显示输入框，已经退出的群聊只显示历史消息
                            visible: root.hasActiveConversation && (!root.currentIsGroup || root.currentGroupActive)
                            currentPeerId: root.currentConversationId
                            fileSendingEnabled: !root.currentIsGroup
                            isVisibleFileButton: !root.currentIsGroup


                            //接收InputPanel发出的文本消息发送请求。
                            onSendRequested: function(content) {
                                //由Window统一判断当前会话是私聊还是群聊，再调用对应的C++接口。
                                root.trySendMessage(content)
                            }

                            //接收InputPanel发出的文件发送请求。
                            onFileSendRequested: function(fileUrl) {
                                //由Window检查当前是否为私聊，再将文件发送请求交给C++。
                                root.trySendFile(fileUrl)
                            }
                        }
                        //退出群聊后用只读提示替代输入框，避免用户误以为仍然可以发送消息
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 50

                            visible: root.currentIsGroup
                                     && root.currentGroupId.length > 0
                                     && !root.currentGroupActive

                            color: "#F5F5F5"
                            border.color: "#E5E5E5"
                            border.width: 1

                            Text {
                                width: parent.width
                                height: parent.height

                                text: qsTr("该群聊已退出或解散，只能查看历史消息")
                                color: "#6B7280"
                                font.pixelSize: 14

                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                        }
                    }
                }
            }
        }
    }


    //全局焦点处理：点击搜索框以外的位置
    TapHandler {
        id: focus

        gesturePolicy: TapHandler.DragThreshold

        onTapped: function(eventPoint)
        {
            if (!peerPanel.isInSearchField(background, eventPoint.position.x, eventPoint.position.y))
            {
                peerPanel.clearSearchFocus()
            }
        }
    }

    //接收文件时选择保存路径
    FileDialog {
        id: saveFileDialog

        title: qsTr("选择文件保存位置")
        fileMode: FileDialog.SaveFile

        //默认打开 /root 目录，并预填收到的文件名。
        //SaveFile需要“完整文件路径”
        currentFolder: "file:///root"

        nameFilters: [qsTr("所有文件 (*)")]
        acceptLabel: qsTr("保存")

        onAccepted: {
            var saveUrl = selectedFile

            if (saveUrl.toString().length === 0) {
                console.log("保存路径为空")
                root.fileTransferStatusText = qsTr("保存路径为空")
                return
            }

            if (saveUrl.toString().endsWith("/")) {
                console.log("请选择具体文件名，不能只选择文件夹")
                root.fileTransferStatusText = qsTr("请选择具体文件名，不能只选择文件夹")
                return
            }

            root.fileTransferName = root.pendingFileName
            root.fileTransferFromMe = false
            root.fileTransferPercent = 0
            root.fileTransferVisible = true

            appController.acceptFile(root.pendingFileIp, saveUrl)

            receiveFilePanel.visible = false
            root.fileTransferStatusText = qsTr("对方已接受文件，等待传输")

            console.log("接受文件:", root.pendingFileName, "保存到:", saveUrl)
        }
    }

    //文件接受提示面板
    Rectangle {
        id: receiveFilePanel

        width: 300
        height: 150
        radius: 10
        visible: false
        color: "#FFFFFF"
        border.color: "#D0D0D0"
        border.width: 1
        z: 10

        anchors.centerIn: parent

        Text {
            id: receiveTitle

            text: qsTr("收到文件")
            font.pixelSize: 10
            font.bold: true
            color: "black"

            anchors.left: parent.left
            anchors.leftMargin: 20
            anchors.top: parent.top
            anchors.topMargin: 20
        }


        Text {
            id: receiveInfo

            text: root.pendingFileName
                + "\n大小：" + root.fileSizeJudgement(root.pendingFileSize)
                + "\n来自：" + root.pendingFileIp
            font.pixelSize: 15
            color: "#4E5969"
            wrapMode: Text.Wrap

            anchors.left: receiveTitle.left
            anchors.right: parent.right
            anchors.rightMargin: 20
            anchors.top: receiveTitle.bottom
            anchors.topMargin: 10
        }

        Text {
            id: transferStatusText

            text: root.fileTransferStatusText
            font.pixelSize: 12
            color: "#86909C"
            elide: Text.ElideRight

            anchors.left: receiveTitle.left
            anchors.right: parent.right
            anchors.rightMargin: 20
            anchors.top: receiveInfo.bottom
            anchors.topMargin: 10
        }


        Rectangle {
            id: rejectFileButton

            width: 70
            height: 30
            radius: 10
            color: rejectFileHover.hovered ? "#F2F3F5" : "#FFFFFF"
            border.color: "#D0D0D0"
            border.width: 1

            anchors.right: acceptFileButton.left
            anchors.rightMargin: 10
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 20

            Text {
                text: qsTr("拒绝")
                color: "#333333"
                font.pixelSize: 15
                anchors.centerIn: parent
            }

            HoverHandler {
                id: rejectFileHover
                cursorShape: Qt.PointingHandCursor
            }

            TapHandler {
                acceptedButtons: Qt.LeftButton
                gesturePolicy: TapHandler.ReleaseWithinBounds

                onTapped: {
                    if (root.pendingFileName.length === 0) {
                        console.log("待接收文件名为空")
                        root.fileTransferStatusText = qsTr("待接收文件名为空")
                        return
                    }

                    //接收普通文件
                    root.fileTransferName = root.pendingFileName
                    root.fileTransferFromMe = false
                    root.fileTransferPercent = 0
                    root.fileTransferVisible = true

                    //保存文件到data/download
                    appController.acceptFileToDownload(root.pendingFileIp, root.pendingFileName)

                    receiveFilePanel.visible = false
                    root.fileTransferStatusText = qsTr("已接受文件，等待传输")
                }
            }
        }

        Rectangle {
            id: acceptFileButton

            width: 70
            height: 30
            radius: 10
            color: acceptFileHover.hovered ? "#0E9FE6" : "#12B7F5"

            anchors.right: parent.right
            anchors.rightMargin: 20
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 15

            Text {
                text: qsTr("接收")
                color: "#FFFFFF"
                font.pixelSize: 15
                anchors.centerIn: parent
            }

            HoverHandler {
                id: acceptFileHover
                cursorShape: Qt.PointingHandCursor
            }

            TapHandler {
                acceptedButtons: Qt.LeftButton
                gesturePolicy: TapHandler.ReleaseWithinBounds

                onTapped: {
                    if (root.pendingFileName.length === 0) {
                        console.log("待接收文件名为空")
                        root.fileTransferStatusText = qsTr("待接收文件名为空")
                        return
                    }

                    //打开保存框前，先给它一个默认保存文件名。
                    //这样用户不用只选目录，而是直接得到 /root/原文件名。
                    saveFileDialog.selectedFile = "file:///root/" + encodeURIComponent(root.pendingFileName)
                    saveFileDialog.open()
                }
            }
        }
    }
    //边缘拉伸
    Item {
        id: resizeArea

        anchors.fill: parent
        z:10

        //边缘缩放区域宽度
        property int edgeWidth: 5

        //左边缘
        Rectangle {
            width: resizeArea.edgeWidth
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom

            HoverHandler {
                cursorShape: Qt.SizeHorCursor
            }

            DragHandler {
                target: null
                onActiveChanged: {
                    if (active)
                        root.startSystemResize(Qt.LeftEdge)
                }
            }
        }

        //右边缘
        Rectangle {
            width:resizeArea.edgeWidth
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.bottom: parent.bottom

            HoverHandler {
                cursorShape: Qt.SizeHorCursor
            }

            DragHandler {
                target: null
                onActiveChanged: {
                    if (active)
                        root.startSystemResize(Qt.RightEdge)
                }
            }
        }

        //上边缘
        Rectangle {
            height:resizeArea.edgeWidth
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top

            HoverHandler {
                cursorShape: Qt.SizeVerCursor
            }

            DragHandler {
                target: null
                onActiveChanged: {
                    if (active)
                        root.startSystemResize(Qt.TopEdge)
                }
            }
        }

        //下边缘
        Rectangle {
            height:resizeArea.edgeWidth
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom

            HoverHandler {
                cursorShape: Qt.SizeVerCursor
            }

            DragHandler {
                target: null
                onActiveChanged: {
                    if (active)
                        root.startSystemResize(Qt.BottomEdge)
                }
            }
        }

        //左上边缘
        Rectangle {
            height:resizeArea.edgeWidth
            width: resizeArea.edgeWidth
            z: 2

            anchors.left: parent.left
            anchors.top: parent.top

            HoverHandler {
                cursorShape: Qt.SizeFDiagCursor
            }

            DragHandler {
                target: null
                onActiveChanged: {
                    if (active)
                        root.startSystemResize(Qt.TopEdge | Qt.LeftEdge)
                }
            }
        }

        //右上边缘
        Rectangle {
            height:resizeArea.edgeWidth
            width: resizeArea.edgeWidth
            z: 2

            anchors.right: parent.right
            anchors.top: parent.top

            HoverHandler {
                cursorShape: Qt.SizeBDiagCursor
            }

            DragHandler {
                target: null
                onActiveChanged: {
                    if (active)
                        root.startSystemResize(Qt.TopEdge | Qt.RightEdge)
                }
            }
        }

        //左下边缘
        Rectangle {
            height:resizeArea.edgeWidth
            width: resizeArea.edgeWidth
            z: 2

            anchors.left: parent.left
            anchors.bottom: parent.bottom

            HoverHandler {
                cursorShape: Qt.SizeBDiagCursor
            }

            DragHandler {
                target: null
                onActiveChanged: {
                    if (active)
                        root.startSystemResize(Qt.BottomEdge | Qt.LeftEdge)
                }
            }
        }

        //右下边缘
        Rectangle {
            height:resizeArea.edgeWidth
            width: resizeArea.edgeWidth
            z: 2

            anchors.right: parent.right
            anchors.bottom: parent.bottom

            HoverHandler {
                cursorShape: Qt.SizeFDiagCursor
            }

            DragHandler {
                target: null
                onActiveChanged: {
                    if (active)
                        root.startSystemResize(Qt.BottomEdge | Qt.RightEdge)
                }
            }
        }
    }

    //群聊详情界面的展开
    Drawer {
        id: groupDetailDrawer

        edge: Qt.RightEdge
        y: 90
        width: 280
        height: parent.height - y
        modal: true //Drawer打开时启用一个modal overlay(为了修复再次点击按钮不收缩的情况：因为按钮在外侧，点击按钮会触发两次收缩，导致点击按钮无法收缩)

        //点击外部区域关闭, 增加允许Esc关闭
        closePolicy: Popup.CloseOnPressOutside | Popup.CloseOnEscape

        enter: Transition {
            NumberAnimation {
                property: "position"
                from: 0
                to: 1
                duration: 150
                easing.type: Easing.OutCubic
            }
        }

        exit: Transition {
            NumberAnimation {
                property: "position"
                from: 1
                to: 0
                duration: 150
                easing.type: Easing.OutCubic
            }
        }

        background: Rectangle {
            color: "#FFFFFF"
            //左侧加一条分割线
            Rectangle {
                width: 1
                height: parent.height
                color: "#E5E5E5"
                anchors.left: parent.left
            }
        }

        //抽屉内容
        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 20
            spacing: 12

            Text {
                text: "群聊信息"
                font.pixelSize: 18
                font.bold: true
                color: "#333333"
            }

            Text {
                text: "群名称: " + root.currentGroupName
                font.pixelSize: 14
                color: "#555555"
                Layout.fillWidth: true
                elide: Text.ElideRight
            }

            Text {
                text: "群ID: " + root.currentGroupId
                font.pixelSize: 14
                color: "#555555"
                Layout.fillWidth: true
                elide: Text.ElideRight
            }

            Text {
                text: "成员数: " + appController.groupMembers.length
                font.pixelSize: 14
                color: "#555555"
            }

            //分割线
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 1
                color: "#F0F0F0"
            }

            //成员列表
            GroupMemberListView {
                Layout.fillHeight: true
                Layout.fillWidth: true
            }

            //占位
            Item {
                Layout.fillHeight: true
            }

            //退出群聊按钮
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 36
                visible: root.currentGroupActive
                radius: 5
                color: leaveGroupButtonHovered.hovered ? "#e17a7a" : "#e13b3b"

                Text {
                    text: "退出群聊"
                    color: "#FFFFFF"
                    font.pixelSize: 15
                    anchors.centerIn: parent
                }

                TapHandler{
                    id: groupDetailTapped
                    onTapped: {
                        exitGroupConfirmDialog.open()
                    }
                }

                HoverHandler{
                    id: leaveGroupButtonHovered
                }
            }
        }
    }

    //打开群聊详情界面的函数
    function openGroupDrawer() {
        if (!root.currentIsGroup || root.currentGroupId.length === 0)
            return

        if (groupDetailDrawer.opened) {
            groupDetailDrawer.close()
        } else {
            groupDetailDrawer.open()
        }
    }

    //提示用户是否要退出该群聊
    Dialog {
        id: exitGroupConfirmDialog
        modal: true
        focus: true

        closePolicy: Popup.CloseOnPressOutside
        width: 350
        height: 170
        anchors.centerIn: parent

        //设置其他背景变暗
        Overlay.modal: Rectangle {
            color: "#80000000"
        }

        function confirmLeaveGroup() {
            if (appController.leaveGroup(root.currentGroupId))
                exitGroupConfirmDialog.accept()
        }

        onOpened: {
            //等待Dialog自身完成打开和焦点切换后，再把活动焦点交给确定按钮。
            Qt.callLater(function() {
                confirmLeaveButton.forceActiveFocus()
            })
        }

        Rectangle {
            width: 24
            height: 24
            anchors.right: parent.right
            anchors.rightMargin: 2
            anchors.top: parent.top
            anchors.topMargin: 2
            color: "transparent"

            Text {
                text: "✕"
                font.pixelSize: 20
                color: "black"
                anchors.centerIn: parent
            }

            TapHandler {
                onTapped: exitGroupConfirmDialog.reject()
            }

            HoverHandler{
                id:quitGroupConfirmHovered
                cursorShape: Qt.PointingHandCursor
            }
        }

        background: Rectangle {
            color: "white"
            radius: 8
            border.color: "#E0E0E0"
            border.width: 1
        }

        contentItem: Item {
            anchors.fill: parent

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 20
                spacing: 10

                Label{
                    text: "退出群聊"
                    font.pixelSize: 16
                    font.bold: true
                    color: "black"
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }

                Label {
                    text: "确定要退出该群聊吗？"
                    font.pixelSize: 16
                    color: "black"
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }

                RowLayout {
                    Layout.alignment: Qt.AlignRight
                    //确定按钮
                    Rectangle{
                        id: confirmLeaveButton

                        width: 80; height: 35; radius: 10;
                        color: okHovered.hovered ? "#008af3" : "#00a6ff"
                        focus: true
                        Text {
                            anchors.centerIn: parent
                            text: "确定"
                            color: "white"
                            font.pixelSize: 16
                        }

                        HoverHandler{
                            id: okHovered
                        }

                        TapHandler{
                            onTapped: exitGroupConfirmDialog.confirmLeaveGroup()
                        }
                        //键盘事件必须写在实际获得activeFocus的确定按钮上。
                        Keys.onPressed: function(event) {
                            if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                                //阻止回车事件继续传递给其他对象
                                event.accepted = true

                                exitGroupConfirmDialog.confirmLeaveGroup()
                            }
                        }
                    }
                    //取消按钮
                    Rectangle{
                        width: 80; height: 35; radius: 10;
                        color: cancelHovered.hovered ? "#efefef" : "#f8f8f8"
                        border{
                            color: "#c2c2c2"
                            width: 1
                        }

                        Text {
                            anchors.centerIn: parent
                            text: "取消"
                            color: "black"
                            font.pixelSize: 16
                        }

                        HoverHandler{
                            id: cancelHovered
                        }

                        TapHandler{
                            onTapped: exitGroupConfirmDialog.reject()
                        }
                    }
                }
            }
        }

        onAccepted: {
            //清除Window.qml保存的当前群聊ID、群名称和群聊状态
            root.closeGroupChat()
            groupDetailDrawer.close()
        }
    }
}