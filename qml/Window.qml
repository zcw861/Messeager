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

import QtQuick
import QtQuick.Controls
import se.qt.messager
ApplicationWindow {
   id: root

   width: 900
   height: 600
   minimumWidth: 810
   minimumHeight: 540

   visible: true
   title: "Messager 信使"

   //当前正在聊天的局域网用户信息
   property string currentPeerId: ""
   property string currentPeerName: ""
   property string currentPeerIp: ""

   AppController {
       id: appController
       Component.onCompleted: {
           appController.initialize("zcw")
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
       onOperationFailed: function(message) {
           console.error("操作失败:", message)
       }
   }

   //消息显示
   function trySendMessage(content) {
       content = content.trim()

       if (root.currentPeerId === "") return
       if (content.length === 0) return

       appController.sendMessage(
           root.currentPeerId,
           root.currentPeerName,
           root.currentPeerIp,
           content
       )
       console.log("发送给:", root.currentPeerId, root.currentPeerName, "内容:", content)
   }

   //处理文件发送请求
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
}

   Rectangle {
       id: background
       anchors.fill: parent
       color: "white"

       PeerPanel {
           id: peerPanel

           width: 200
           anchors.left: parent.left
           anchors.top: parent.top
           anchors.bottom: parent.bottom
           //把Main.qml当前选中的用户id传给PeerPanel，用于左侧高亮
           currentPeerId: root.currentPeerId
           peerModel: appController.peers

           //     [v0.1.2] HeZhiyuan    2026-06-03 16:37:40
           //         * 用户点击左侧列表项后，这里会被调用
           onPeerSelected: function(peerId, username, ip) {
               //切换到另一个用户时，先清空当前前端消息
               if (root.currentPeerId !== peerId){
                   inputPanel.clear()
               }
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


       //没有选择用户时显示提示区域
       Rectangle {
           id: emptyPanel

           anchors.left: peerPanel.right
           anchors.right: background.right
           anchors.top: background.top
           anchors.bottom: background.bottom

           color: "#FAFAFA"
           visible: root.currentPeerId === ""

           Text {
               text: qsTr("点击左侧用户开始聊天")
               font.pixelSize: 16
               color: "#999999"
               anchors.centerIn: parent
           }
       }

       //底部输入框：点击左侧用户后才显示
       InputPanel {
           id: inputPanel

           height: 200

           anchors.left: peerPanel.right
           anchors.right: background.right
           anchors.bottom: background.bottom

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

       //右侧聊天窗口：点击左侧用户后才显示
       ChatPanel {
           id: chatPanel

           anchors.left: peerPanel.right
           anchors.right: background.right
           anchors.top: background.top
           anchors.bottom: inputPanel.top

           visible: root.currentPeerId !== ""

           currentPeerId: root.currentPeerId
           currentPeerName: root.currentPeerName

           // 把窗口中的消息模型传给聊天显示区
           messageModel: appController.messages
           color: "#FFFFFF"
       }
   }
}
