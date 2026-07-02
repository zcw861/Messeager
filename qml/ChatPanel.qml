/**
* @file    ChatPanel.qml
* @date    2026-06-01
* @author  JiangFan
* @brief   右侧聊天面板
*
* Version: 0.1.0
* License: AGPLv3
* Created:  JiangFan 2026-06-01 22:41:47
*
*
* Change Log:
* [v0.1.0] JiangFan   2026-06-01
* * Initial creation
*
* [v0.2.0] JiangFan   2026-06-03
* * 实现聊天框的发送消息，增加enter, ctrl + enter事件处理
*
* [v0.2.2] HeZhiyuan  2026-06-05 22:39:59
* * 增加聊天消息前端显示功能，发送消息后，当前聊天窗口会立即显示自己发送的内容。
*
* [v0.2.3] HeZhiyuan  2026-06-07 13:57:18
* * 修改部分ui颜色
*
* [v0.2.4] HeZhiyuan  2026-06-13 17:47:01
* * 消息列表改为接收AppController提供的QVariantList。
* * delegate使用modelData.fromMe和modelData.content。
*
* [v0.2.5] JiangFan   2026-06-20
* * 重构：使用Layout管理主窗口结构
* * 修复：消息滚动条和消息内容重合的bug
* [v0.2.6] HeZhiyuan   2026-06-25
* * 使用私聊消息模型和群聊消息模型显示当前会话历史
* * 群聊中显示其他成员的发送者名称
* [v0.2.7] ZhouChengWei  2026-06-27
* * 添加了打开群聊详情界面的按钮，主要用于退出/解散群聊
* [v0.2.8] JiangFan  2026-06-28
* * 将图片文件与其他文件区别开，能够缓存图片文件，直接显示到聊天界面
*/

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle{
    id: root

    //Window.qml传入
    property string currentPeerId: ""
    property string currentPeerName: "请选择用户"
    property var messageModel: null //MessageWindow.qml 传入的消息模型
    property bool isGroupChat: false
    property var appController: null

    //文件传输信息
    property bool fileTransferVisible: false //是否显示文件传输进度条
    property string fileTransferName: "" //当前正在传输的文件名
    property int fileTransferPercent: 0 //当前传输进度，范围0到100
    property bool fileTransferFromMe: true //当前传输是否属于自己发送的文件消息

    color: "#FFFFFF"

    ColumnLayout {
        id: chatLayout

        anchors.fill: parent
        spacing: 0

        //顶部栏
        Rectangle {
            id: chatHeader

            Layout.preferredHeight: 50
            Layout.fillWidth: true

            color: "#FFFFFF"

            //分割线
            Rectangle {
                height: 1
                color: "#E5E5E5"

                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
            }

            Text {
                id: chatTitle
                text: currentPeerName

                font.pixelSize: 15
                font.bold: true
                color: "#333333"

                anchors.left: parent.left
                anchors.leftMargin: 15
                anchors.verticalCenter: parent.verticalCenter
            }

            //打开群聊详情界面（包括退出/解散群聊）
            Rectangle {
                id: groupDetailButton
                width: 30; height: 30; radius: 6
                anchors.verticalCenter: parent.verticalCenter
                anchors.right: parent.right
                anchors.rightMargin: 10
                visible: root.isGroupChat
                color: groupDetailButtonHovered.hovered ? "#efefef" : "#FFFFFF"

                Text {
                    text: "···"
                    font.pixelSize: 18
                    color: "black"
                    anchors.centerIn: parent
                }

                HoverHandler{
                    id:groupDetailButtonHovered
                    cursorShape: Qt.PointingHandCursor
                }

                TapHandler{
                    id: groupDetailButtonTapped
                    onTapped: {
                        parent.Window.window.openGroupDrawer()
                    }
                }
            }
        }


        //消息显示
        Rectangle {
            id: messageArea

            Layout.fillWidth: true
            Layout.fillHeight: true

            color: "#F5F5F5"

            //没有消息时的提示
            Text {
                text: qsTr("暂无消息")
                color: "#999999"
                font.pixelSize: 15
                anchors.centerIn: parent

                visible: messageList.count === 0
            }

            //消息列表
            ListView {
                id: messageList

                anchors.fill: parent
                anchors.margins: 12

                clip: true
                spacing: 8

                model: root.messageModel

                //每新增一条消息，自动滚动到底部
                onCountChanged: {
                    Qt.callLater(function() {
                        messageList.positionViewAtEnd()
                    })
                }

                delegate: ColumnLayout {
                    id: messageDelegate

                    required property var modelData

                    readonly property bool fromMe: Boolean(modelData.fromMe)
                    readonly property string content: String(modelData.content)
                    readonly property string senderName: modelData.senderName ? String(modelData.senderName) : ""

                    //普通文件消息前缀
                    readonly property string filePrefix: "[发送文件] "

                    //当前消息是否为普通文件消息。
                    readonly property bool isFileMessage: content.startsWith(filePrefix)

                    //取出普通文件消息中的文件名。
                    readonly property string fileName: isFileMessage ? content.substring(filePrefix.length).trim() : ""

                    //传输进度条的可见性
                    readonly property bool showFileProgress: root.fileTransferVisible //有文件在传输
                                                             && isFileMessage //普通文件传输
                                                             && fileName.trim() === root.fileTransferName.trim() //消息名=传输文件名
                                                             && fromMe === root.fileTransferFromMe //消息方向一致（防止左右两边同名文件））


                    //图片消息格式：[图片] /root/.../xxx.png  （[图片] 有个空格！）
                    readonly property string imagePrefix: "[图片] "
                    readonly property bool isImageMessage: content.startsWith(imagePrefix)
                    readonly property string imagePath: isImageMessage ? content.substring(imagePrefix.length).trim() : ""

                    //图片Url处理
                    //数据库中保存的是/root/.../xxx.png，所以这里转成 file:///root/.../xxx.png
                    readonly property url imageSource:
                        messageDelegate.isImageMessage && root.appController !== null
                        ? root.appController.localFileUrl(messageDelegate.imagePath)
                        : ""

                    //图片最大显示范围，避免超大图片撑爆聊天窗口
                    readonly property real maxImageWidth: Math.min(messageList.width * 0.7 - 16, 240)
                    readonly property real maxImageHeight: 220

                    //图片原始宽高，图片没加载完成前，先给一个默认值，避免布局为0
                    readonly property real naturalImageWidth:
                        messageImage.status === Image.Ready && messageImage.implicitWidth > 0
                        ? messageImage.implicitWidth
                        : 160

                    readonly property real naturalImageHeight:
                        messageImage.status === Image.Ready && messageImage.implicitHeight > 0
                        ? messageImage.implicitHeight
                        : 120

                    //缩放比例：不超过最大宽度、不超过最大高度
                    readonly property real imageScale:
                        Math.min(maxImageWidth / naturalImageWidth,
                                maxImageHeight / naturalImageHeight, 1)

                    //最终图片显示宽高
                    readonly property real imageDisplayWidth: naturalImageWidth * imageScale
                    readonly property real imageDisplayHeight: naturalImageHeight * imageScale

                    width: messageList.width
                    spacing: 3

                    //私聊不显示发送者名称
                    //群聊中自己的消息不重复显示自己名称
                    Text {
                        visible: root.isGroupChat && !messageDelegate.fromMe
                        text: messageDelegate.senderName
                        Layout.alignment: Qt.AlignLeft

                        color: "#6B7280"
                        font.pixelSize: 11
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 0

                        //自己发送时，将气泡推向右侧。
                        Item {
                            visible: messageDelegate.fromMe

                            Layout.fillWidth: true
                        }

                        Rectangle {
                            id: messageBubble

                            Layout.maximumWidth: messageList.width * 0.7

                            Layout.preferredWidth: messageDelegate.isImageMessage
                                                   ? messageDelegate.imageDisplayWidth + 16
                                                   : messageDelegate.showFileProgress
                                                     ? Math.min(Math.max(messageText.implicitWidth + 24, 190), messageList.width * 0.7)
                                                     : Math.min(messageText.implicitWidth + 24, messageList.width * 0.7)

                            Layout.preferredHeight: messageDelegate.isImageMessage
                                                    ? messageDelegate.imageDisplayHeight + 16
                                                    : messageText.implicitHeight + 18
                                                      + (messageDelegate.showFileProgress ? 40 : 0)

                            radius: 8

                            color:messageDelegate.fromMe ? "#12B7F5" : "#FFFFFF"

                            Text {
                                id: messageText

                                //图片消息不显示文本
                                visible: !messageDelegate.isImageMessage

                                width: messageDelegate.showFileProgress
                                       ? Math.max(160, Math.min(implicitWidth, messageList.width * 0.7 - 24))
                                       : Math.min(implicitWidth, messageList.width * 0.7 - 24)

                                height: implicitHeight

                                x: 12
                                y: 9

                                text: messageDelegate.content

                                font.pixelSize: 14

                                color: messageDelegate.fromMe ? "#FFFFFF" : "#222222"

                                wrapMode: Text.Wrap
                            }

                            //文件传输进度条
                            ColumnLayout {
                                visible: messageDelegate.showFileProgress

                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.leftMargin: 12
                                anchors.rightMargin: 12
                                anchors.top: messageText.bottom
                                anchors.topMargin: 6

                                spacing: 3

                                Text {
                                    text: qsTr("传输中 ") + root.fileTransferPercent + "%"
                                    color: "grey"
                                    font.pixelSize: 10

                                    Layout.alignment: messageDelegate.fromMe ? Qt.AlignRight : Qt.AlignLeft
                                }

                                ProgressBar {
                                    from: 0
                                    to: 100
                                    value: root.fileTransferPercent

                                    Layout.preferredHeight: 5
                                    Layout.fillWidth: true
                                }
                            }


                            Image {
                                id: messageImage

                                visible: messageDelegate.isImageMessage

                                width: messageDelegate.imageDisplayWidth
                                height: messageDelegate.imageDisplayHeight

                                anchors.fill: parent
                                anchors.margins: 8

                                source: messageDelegate.isImageMessage
                                        ? messageDelegate.imageSource
                                        : ""

                                fillMode: Image.PreserveAspectFit //保持图片原始宽高比
                                asynchronous: true //异步加载图片（防止图片加载卡住界面）
                                retainWhileLoading: true

                                TapHandler {
                                    acceptedButtons: Qt.LeftButton //限制左键点

                                    onTapped: {
                                        if (!messageDelegate.isImageMessage)
                                            return

                                        if (messageDelegate.imagePath.length === 0)
                                        {
                                            console.log("图片打开失败：路径为空！")
                                            return
                                        }

                                        if (root.appController === null)
                                        {
                                            console.log("图片打开失败：appController为空")
                                            return
                                        }

                                        console.log("点击图片，准备打开:", messageDelegate.imagePath)
                                        root.appController.openLocalFile(messageDelegate.imagePath)
                                    }
                                }

                                HoverHandler {
                                        cursorShape: Qt.PointingHandCursor
                                }
                            }
                        }

                        //他人发送时，将气泡保留在左侧。
                        Item {
                            visible: !messageDelegate.fromMe

                            Layout.fillWidth: true
                        }
                    }

                }

                //提供右侧垂直滚动条，用来显示当前滚动位置
                ScrollBar.vertical: ScrollBar {
                    parent: messageArea   //这一句很必要，不加的话，它下面都没用，会被messageList自动管理
                    anchors.top: messageList.top
                    anchors.bottom: messageList.bottom
                    anchors.left: messageList.right
                    anchors.leftMargin: 4 //刚好贴到最边上，不会和消息重合
                }
            }
        }

    }
}

