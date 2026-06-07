// Module
// File: Window.qml   Version: 0.1.0   License: AGPLv3
// Created: HeZhiyuan      2026-06-05 19:33:19
// Description:存放真正的窗口内容，负责整体布局和状态
//
//     [v0.1.2] HeZhiyuan    2026-06-05 21:22:32
//         * 加上底部输入框组件。
import QtQuick
import QtQuick.Controls

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

   //当前聊天窗口的前端消息列表。目前只做前端显示，未涉及真正网络发送
   ListModel {
       id: chatMessageModel
   }

   //先把消息显示出来，后续再接客户端网络发送逻辑
   function trySendMessage(content) {
       content = content.trim()

       if (root.currentPeerId === "") return
       if (content.length === 0) return

       //追加一条“我发送的消息”到前端消息模型
       chatMessageModel.append({
           fromMe: true,
           content: content,
           peerId: root.currentPeerId,
           peerName: root.currentPeerName
       })

       console.log("发送给:", root.currentPeerId, root.currentPeerName, "内容:", content)
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
           //把 Main.qml 当前选中的用户 id 传给 PeerPanel，用于左侧高亮
           currentPeerId: root.currentPeerId

           //     [v0.1.2] HeZhiyuan    2026-06-03 16:37:40
           //         * 用户点击左侧列表项后，这里会被调用
           onPeerSelected: function(peerId, username, ip) {
               //切换到另一个用户时，先清空当前前端消息
               if (root.currentPeerId !== peerId){
                   chatMessageModel.clear()
                   inputPanel.clear()
               }
               root.currentPeerId = peerId
               root.currentPeerName = username
               root.currentPeerIp = ip

               console.log("Main.qml 当前聊天对象:", peerId, username, ip)
           }

           //     [v0.1.2] HeZhiyuan    2026-06-04 20:47:45
           //         * 再次点击当前用户会关闭聊天窗口
           onPeerClosed: {
               root.currentPeerId = ""
               root.currentPeerName = ""
               root.currentPeerIp = ""

               // 关闭聊天窗口时清空前端消息
               chatMessageModel.clear()
               inputPanel.clear()
               console.log("Main.qml 已回到初始界面")
           }

           //接收PeerPanel发送的  搜索框改变  的信号
           onSearchTextChanged: function(keyword) {
               //待改
               console.log("搜索用户", keyword)
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
           messageModel: chatMessageModel

           color: "#FFFFFF"
       }
   }
}
