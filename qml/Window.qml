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

import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import se.qt.messager
import QtQuick.Layouts

ApplicationWindow {
   id: root

   width: 1000
   height: 700
   minimumWidth: 800
   minimumHeight: 500

   visible: true
   title: "Messager 信使"
   //flags: Qt.Window | Qt.FramelessWindowHint //隐藏顶部默认菜单栏，但是需要自己实现窗口缩放（暂时还没有实现）

   //自己的信息
   property string myName: ""
   property string myIp: ""

   //当前正在聊天的局域网用户信息
   property string currentPeerId: ""
   property string currentPeerName: ""
   property string currentPeerIp: ""

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
       Component.onCompleted: {
           appController.initialize("lll")
           myName = "lll" //这一个建议和上一个整合
           myIp = appController.localIp()
       }
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

   //校验当前会话和消息内容，并将发送请求交给AppController，网络发送和数据库保存均由C++控制层完成
   function trySendMessage(content) {
       //去除消息首尾空白，防止发送空白消息
       content = content.trim()

       //未选择用户或正文为空时不执行发送
       if (root.currentPeerId === "") return
       if (content.length === 0) return

       //将完整聊天对象信息和消息正文交给C++控制器
       appController.sendMessage(
           root.currentPeerId,
           root.currentPeerName,
           root.currentPeerIp,
           content
       )
       console.log("发送给:", root.currentPeerId, root.currentPeerName, "内容:", content)
   }

   //发送文件
   function trySendFile(fileUrl) {

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

   Rectangle {
          id: background
          anchors.fill: parent
          color: "white"

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

                       Rectangle {
                          id: headPortrait

                          Layout.preferredHeight: 25
                          Layout.preferredWidth: 25
                          Layout.leftMargin: 5
                          color: "white"

                          //圆框好像有点丑
                          //radius: 100
                          //border.color: "black"
                          //border.width: 1

                          //头像
                          Image {
                             source: "source/headPortrait.svg"

                             width: 20
                             height: 20

                             anchors.centerIn: parent
                          }
                       }

                       Text {
                          id: personName
                          text: myName + "  "+ myIp
                          font.pixelSize: 15
                          Layout.leftMargin: 5

                       }

                       //占位
                       Rectangle {
                          Layout.fillHeight: true
                          Layout.fillWidth: true
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
                         //用户列表数据直接来自AppController的peer属性
                         peerModel: appController.peers

                         //     [v0.1.2] HeZhiyuan    2026-06-03 16:37:40
                         //         * 用户点击左侧列表项后，这里会被调用
                         //并通知AppController从数据库读取该用户的历史消息
                         onPeerSelected: function(peerId, username, ip) {
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
                                    visible: root.currentPeerId === ""

                                    Text {
                                        text: qsTr("点击左侧用户开始聊天")
                                        font.pixelSize: 16
                                        color: "#999999"
                                        anchors.centerIn: parent
                                    }
                                }

                                //右侧聊天窗口：点击左侧用户后才显示
                                ChatPanel {
                                    id: chatPanel

                                    anchors.fill: parent

                                    visible: root.currentPeerId !== ""

                                    currentPeerId: root.currentPeerId
                                    currentPeerName: root.currentPeerName

                                    //把窗口中的消息模型传给聊天显示区
                                    messageModel: appController.messages
                                    color: "#FFFFFF"
                                }
                             }

                             //底部输入框：点击左侧用户后才显示
                             InputPanel {
                                 id: inputPanel

                                 Layout.fillWidth: true
                                 Layout.preferredHeight: 200

                                 visible: root.currentPeerId !== ""

                                 currentPeerId: root.currentPeerId

                                 onSendRequested: function(content) {
                                     root.trySendMessage(content)
                                 }

                                 //处理文件按钮的点击
                                 onFileSendRequested: function(fileUrl) {
                                     root.trySendFile(fileUrl)
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
                         + "\n大小：" + root.pendingFileSize + " 字节"
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
      }

}