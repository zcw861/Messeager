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

   //当前正在聊天的局域网用户信息，后续选择用户后，会修改这些值
   property string currentPeerId: ""
   property string currentPeerName: "请选择用户"



   Rectangle {
       id: background
       anchors.fill: parent
       color: "white"

       //左侧用户面板
       PeerPanel {
               id: peerPanel

               width: 200
               anchors.left: parent.left
               anchors.top: parent.top
               anchors.bottom: parent.bottom

               currentPeerId: root.currentPeerId

               //接收PeerPanel发送的  选择用户  的信号
               onPeerSelected: function(peerId, username) {
                  root.currentPeerId = peerId
                  root.currentPeerName = username
               }

               //接收PeerPanel发送的  搜索框改变  的信号
               onSearchTextChanged: function(keyword) {

                  //待改
                  console.log("搜索用户", keyword)
               }

       }


       //右侧聊天窗口
       ChatPanel {
           id: chatPanel

           anchors.left: peerPanel.right    //左侧栏右边
           anchors.right: background.right  //此处用parent一样，下同
           anchors.top: background.top
           anchors.bottom: background.bottom

           //传递当前用户信息
           currentPeerName: root.currentPeerName
           currentPeerId: root.currentPeerId

           color: "#FFFFFF"     
       }
   }
}
