/**
* @file    Main.qml
* @date    2026-05-26
* @author  JiangFan
* @brief   聊天主窗口
*
* Version: 0.1.0
* License: AGPLv3
* Created:  JiangFan 2026-05-26 21:01:33
*
*
* Change Log:
* [v0.1.0]    2026-05-26
* * Initial creation
*
* Change Log:
* [v0.2.0]    2026-06-3
* * 使用-Id、-Name传递消息于Chatnel.qml 与 PeerPanel.qml。目前主要以左右两部分划分，左(PeerPanel, ChatPanel
*/

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
               root.currentPeerId = peerId
               root.currentPeerName = username
               root.currentPeerIp = ip

               console.log("Main.qml 当前聊天对象:", peerId, username, ip)
           }

           //接收PeerPanel发送的  搜索框改变  的信号
           onSearchTextChanged: function(keyword) {
               //待改
               console.log("搜索用户", keyword)
           }
       }


       // 没有选择用户时显示提示区域
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

       //右侧聊天窗口：点击左侧用户后才显示
       ChatPanel {
           id: chatPanel

           anchors.left: peerPanel.right
           anchors.right: background.right
           anchors.top: background.top
           anchors.bottom: background.bottom


           //     [v0.1.2] HeZhiyuan    2026-06-03 17:26:58
           //         * 点击左侧用户后弹出聊天框。只有currentPeerId不为空，聊天框才显示
           visible: root.currentPeerId !== ""

           currentPeerId: root.currentPeerId
           currentPeerName: root.currentPeerName

           color: "#FFFFFF"
       }
   }
}
