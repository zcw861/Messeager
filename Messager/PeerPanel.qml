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
* Change Log:
* [v0.2.3]  ZhouChengWei  2026-06-06
* * 添加创建群聊按钮并且添加了相关菜单项
*
* Change Log:
* [v0.2.4]  JiangFan  2026-06-10
* * 增加左侧栏搜索框焦点清除功能
*/


import QtQuick
import QtQuick.Controls

//左侧栏
Rectangle {
    id: peerPanel
    color: "#D9D9D9"
    //color: "#F7F8FA"

    //后续交给 PeerModel 或 AppController 处理
    //Main.qml给，控制左侧用户高亮
    property string currentPeerId: ""

    //当前搜索关键字,搜索框变化时更新，过滤左侧用户列表
    property string searchKeyword : ""

    //后续交给PeerModel或AppController处理
    signal searchTextChanged(string keyword)

    //点击左侧用户后，向 Main.qml 通知当前选中的用户
    signal peerSelected(string peerId, string username, string ip)

    //点击当前用户时，通知 Main.qml 关闭聊天窗口
    signal peerClosed()

    //目前先使用测试数据
    ListModel {
        id: testPeerModel

        ListElement {
            peerId: "01"
            username: "张三"
            ip: "192.168.1.1"
            online: true
        }

        ListElement {
            peerId: "02"
            username: "李斯"
            ip: "192.168.1.2"
            online: true
        }

        ListElement {
            peerId: "03"
            username: "王五"
            ip: "192.168.1.3"
            online: false
        }
    }

    //判断某个用户是否匹配当前搜索关键字
    function matchSearch(username, ip)
    {
        if (searchKeyword.length == 0)
            return true

        var keyword = searchKeyword.toLowerCase()

        //用户名 / ip地址 里包含keyword
        return username.toLowerCase().indexOf(keyword) !== -1
                || ip.toLowerCase().indexOf(keyword) !== -1
    }



    Text {
        id: peerTitle
        text: qsTr("局域网用户")
        font.pixelSize: 15
        font.bold: true          //加粗
        color: "#333333"         //灰色

        anchors.left: parent.left
        anchors.leftMargin: 10   //左边留10px空隙差不多
        anchors.top: parent.top  //顶部一样
        anchors.topMargin: 10
        }

    //Search
    TextField {
        id: searchField

        width: parent.width - 50
        height: 30

        placeholderText: qsTr("搜索用户")
        font.pixelSize: 12

        anchors.left: parent.left
        anchors.leftMargin: 10
        anchors.top: peerTitle.bottom       //标题框下方
        anchors.topMargin: 10

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

    ListView {
        id: peerListView

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: searchField.bottom
        anchors.topMargin: 12
        anchors.bottom: parent.bottom

        clip: true
        spacing: 6

        model: testPeerModel

        delegate: Rectangle {
            id: peerItem
            width: peerListView.width - 20
            radius: 10

            //当前用户是否匹配关键字
            property bool matched: peerPanel.matchSearch(model.username, model.ip)

            //当前用户是否被选中
            property bool selected: peerPanel.currentPeerId === model.peerId

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
                color: model.online ? "#00CA00" : "#B8B8B8"

                anchors.left: parent.left
                anchors.leftMargin: 12
                anchors.verticalCenter: parent.verticalCenter
            }

            //用户名
            Text {
                id: usernameText

                text: model.username
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

                text: model.ip
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

                onTapped: {                    
                    //清除搜索框焦点
                    peerPanel.clearSearchFocus()

                    //如果点击的是当前正在聊天的用户，则关闭聊天窗口，回到初始界面
                    if (peerPanel.currentPeerId === model.peerId) {
                        peerPanel.peerClosed()
                        console.log("关闭当前聊天窗口:", model.peerId, model.username)
                        return
                    }

                    //否则切换到该用户
                    peerPanel.peerSelected(model.peerId, model.username, model.ip)

                    //调试输出，后续可以删除
                    console.log("选择用户:", model.peerId, model.username, model.ip)
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

    //创建群聊按钮
    Rectangle{
        id: createGroupChat
        width: 30
        height:  30
        radius: 10
        color: typeChange.hovered ? "#a9a9a9" : "#f5f5f5"
        anchors.leftMargin: 5
        anchors.left: searchField.right
        anchors.top: searchField.top

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
                /*
                TapHandler{
                    onTapped: inviteUserInterfaceLoader.item.show()
                }
                */
                //MenuItem本身就是可以点击的控件 并自带triggered信号,所以不应该用TapHandler
                onTriggered: {
                    peerPanel.clearSearchFocus()
                    inviteUserInterfaceLoader.item.show()
                }

            }
            MenuItem{
                text: qsTr("加好友")
            }
        }
    }

    Loader {
            id: inviteUserInterfaceLoader
            source: "InviteUserInterface.qml"
        }
}