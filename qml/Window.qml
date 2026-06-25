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
//
//     [v0.1.5] JiangFan    2026-06-14
//         * 修复全局焦点处理bug
//
//     [v0.1.5] HeZhiyuan    2026-06-14 16:14:29
//         * 删除成功后，清理QML的当前用户状态
//
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

import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import se.qt.messager
import QtQuick.Layouts

ApplicationWindow {
   id: root

   Component.onCompleted: {
       root.autoLogin()
   }

   width: 1000
   height: 700
   minimumWidth: 800
   minimumHeight: 500

   visible: true

   title: "Messager 信使"
   //flags: Qt.Window | Qt.FramelessWindowHint //隐藏顶部默认菜单栏，但是需要自己实现窗口缩放（暂时还没有实现）

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
   property bool currentIsGroup: false //当前会话是否为群聊
   property bool currentIsShrink: false //当前右侧用户列表是否收缩
   //当前界面是否存在活动会话
   readonly property bool hasActiveConversation: currentIsGroup ? currentGroupId.length > 0 : currentPeerId.length > 0
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

       //收到对方文件发送请求
       onFileRequestReceived: function(fromIp, fileName, fileSize)
       {
                  root.pendingFileIp = fromIp
                  root.pendingFileName = fileName
                  root.pendingFileSize = fileSize
                  root.fileTransferPercent = 0
                  root.fileTransferStatusText = qsTr("收到文件请求")

                  receiveFilePanel.visible = true

                  console.log("收到文件请求", fromIp, fileName, fileSize)
       }

       //文件传输进度
       onFileTransferProgress: function(ip, fileName, percent) {
           root.fileTransferPercent = percent
           root.fileTransferStatusText = qsTr("文件传输中: ") + percent + "%"

           console.log("文件传输进度:", ip, fileName, percent)
       }

       //文件传输完成
       onFileTransferFinished: function(ip, fileName, success) {
           root.fileTransferPercent = success ? 100 : root.fileTransferPercent
           root.fileTransferStatusText = success
                   ? qsTr("文件传输完成: ") + fileName
                   : qsTr("文件传输失败: ") + fileName

           console.log("文件传输结束:", ip, fileName, success)
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
          console.log("当前群聊暂未实现群文件协议")
          return
      }
      if (root.currentPeerId === "")
      {
         console.log("请先选择聊天对象")
         return
      }

      if(fileUrl.toString().length === 0)
      {
         console.log("文件路径为空")
         return
      }

      appController.sendFile(root.currentPeerId, root.currentPeerName, root.currentPeerIp, fileUrl)
      console.log("请求发送文件给:", root.currentPeerName, root.currentPeerIp, fileUrl)
   }

   //窗口缩放函数
   function toggleMaxinized()
   {
      if(root.visibility === Window.Maximized)
         root.showNormal()
      else
         root.showMaximized()
   }

   //登录函数
   function login() {
       var userName = loginNameField.text.trim()

       if(userName.length === 0){
           loginErrorText.text = qsTr("用户名不能为空！")
           return
       }

       if(!appController.initialize(userName)){
           loginErrorText.text = qsTr("登录失败！")
           console.log("登录失败！")
           return
       }

       root.myName = userName
       root.isLogin = true
       root.myIp = appController.localIp()
       loginWindow.close()

       console.log("登录成功！用户名： ", userName)
   }

   //自动登录函数:如果此前保存过用户名，则直接自动登录
   function autoLogin() {
      var saveName = appController.savedUserName()

      if(saveName.length === 0)
      {
           console.log("未保存过此用户")
           return
      }

      if(!appController.initialize(saveName))
      {
           console.log("自动登录失败")
           return
      }

      root.myName = saveName
      root.isLogin = true
      root.myIp = appController.localIp()

      console.log("自动登录成功！用户名： ", saveName)
   }

   //打开指定群聊并加载群成员和群消息
   function openGroupChat(groupId, groupName)
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

   //登录弹窗
   Window {
      id: loginWindow

      width: 250
      height: 200
      visible: !root.isLogin

      title: qsTr("登录")
      color: "white"

      Rectangle {
         anchors.fill: parent
         color: "white"
         border.color: "#DDDDDD"
         border.width: 1

         ColumnLayout {
            anchors.fill: parent
            anchors.margins: 20
            spacing: 10

            Text {
                text: qsTr("请输入用户名：")
                font.pixelSize: 15
                font.bold: true
                color: "black"

                Layout.alignment: Qt.AlignHCenter
            }

            //用户名输入框
            TextField {
                id: loginNameField

                color: "black"
                Layout.fillWidth: true
                Layout.preferredHeight: 30

                placeholderText: qsTr("用户名")

                onAccepted: {
                   root.login()
                }
            }

            //这个目前无法正常使用
            // //显示当前ip
            // Text {
            //     text: qsTr("当前IP: ") + (root.myIp.length > 0 ? root.myIp : qsTr("未获取到IP"))
            //     font.pixelSize: 15
            //     color: "black"
            // }

            //错误提示（默认不显示）
            Text {
                id: loginErrorText
                text: ""
                color: "red"
            }

            //登录按钮
            Button {
                text: qsTr("登录")

                Layout.fillWidth: true
                Layout.preferredHeight: 30

                onClicked: {
                    root.login()
                }
            }
         }
      }

      //关闭登录界面 且 没有登录
      onClosing: function(close){
          if(!root.isLogin)
              Qt.quit()
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

                           //回车确认修改(这里不用onAccepted了，用更高级的，可以对失去焦点响应
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
                         onGroupSelected: function(groupId, groupName){
                                 root.openGroupChat(groupId, groupName)
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
                                       messageModel: root.currentIsGroup ? appController.groupMessages : appController.messages
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

                                              color: "#FFFFFF"

                                              Text {
                                                 text: qsTr("群成员： ") + appController.groupMembers.length
                                                 font.pixelSize: 15
                                                 font.bold: true
                                                 color: "black"

                                                 anchors.left: parent.left
                                                 anchors.leftMargin: 10
                                                 anchors.verticalCenter: parent.verticalCenter
                                              }
                                          }

                                          //分割线
                                          Rectangle {
                                             Layout.fillWidth: true
                                             Layout.preferredHeight: 1
                                             color: "#e0e0e0"
                                          }

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

                                                    visible: memberDelegate.showMemberInfo //悬浮 + 悬浮时间 共同决定显示

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
                                                                 text: username.length > 0 ? username.charAt(0) : "?"
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
                                      }
                                   }
                                }

                                //群聊右侧用户列表收缩按钮
                                Rectangle {
                                   id: shrinkButton

                                   color: shrinkButtonHover.hovered ? "#F2F3F5" : "#FFFFFF"

                                   visible: currentIsGroup && root.currentGroupId !== ""
                                            && (groupMemberHover.hovered || shrinkButtonHover.hovered || root.currentIsShrink)
                                                   //不加按钮的这个悬浮的话，准备点击的时候回瞬间消失 OvO

                                   height: 30
                                   width: 15
                                   z:2
                                   anchors.verticalCenter: parent.verticalCenter
                                   anchors.right: parent.right
                                   anchors.rightMargin: root.currentIsShrink ? 2 : 150

                                   border.color: "#E0E0E0"
                                   border.width: 1

                                   Text {
                                      anchors.centerIn: parent
                                      text: currentIsShrink ? "<" : ">"
                                      font.pixelSize: 20
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

                                 visible: root.hasActiveConversation
                                 currentPeerId: root.currentConversationId
                                 fileSendingEnabled: !root.currentIsGroup
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

                  appController.acceptFile(root.pendingFileIp, saveUrl)

                  receiveFilePanel.visible = false
                  root.fileTransferStatusText = qsTr("已接受文件，等待传输")

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
                       appController.rejectFile(root.pendingFileIp)

                       receiveFilePanel.visible = false
                       root.fileTransferStatusText = qsTr("已拒绝文件")

                       console.log("拒绝文件:", root.pendingFileName, root.pendingFileIp)
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

       //群聊用户右侧用户列表左拉伸
       Rectangle {

       }
   }
}