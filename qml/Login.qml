// Module
// File: Login.qml   Version: 0.1.0   License: AGPLv3
// Created: HeZhiyuan      2026-06-26 13:13:38
// Description:     将登录从Window.qml拆分
//
//     [v0.1.1] HeZhiyuan    2026-06-26 14:34:21
//         * 每次程序启动都显示登录窗口并将上一次保存的用户名自动填入用户名输入框
//           为TextField增加固定白色背景和边框，修复暗色模式下输入框变黑的问题
//           固定输入文字、提示文字和选中文字颜色，避免系统暗色模式影响
//     [v0.1.2] HeZhiyuan    2026-06-26 15:25:13
//         * 修改登录焦点判断，有读取到非空字符串的用户名时，焦点在登录键上，没有则在输入框上
//           增加TAB切换焦点功能，回车键执行登录按钮
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
Window {
    id: root

    //接收Window.qml中的AppController
    required property AppController controller
    //记录当前登录流程是否已经成功完成
    property bool loginCompleted: false
    //登录成功后向 Window.qml 返回最终用户名和本机IP
    signal loginSucceeded(string userName, string localIp)

    //登录窗口开始时先保持隐藏
    visible: false

    width: 250
    height: 200

    //这里保留原来的窗口标题
    title: qsTr("登录")

    //统一处理手动登录和自动登录成功后的操作
    function finishLogin(userName) {
        //读取本机IP
        const localIp = controller.localIp()

        //先记录登录成功状态
        root.loginCompleted = true

        //通过信号把用户名和本机IP返回给Window.qml
        root.loginSucceeded(userName, localIp)

        //登录成功后关闭登录窗口
        root.close()
    }

    //处理用户点击登录按钮后的手动登录流程
    function submitLogin() {
        //读取输入框内容，并去除用户名首尾的空白字符
        const userName = loginNameField.text.trim()

        //空用户名不能登录
        if (userName.length === 0) {
            loginErrorText.text = qsTr("用户名不能为空！")
            return
        }

        //清除上一次错误
        loginErrorText.text = ""

        if (!controller.initialize(userName)) {
            loginErrorText.text = controller.lastError.length > 0 ? controller.lastError : qsTr("登录失败，请重试！")

            return
        }

        //初始化成功后登录
        root.finishLogin(userName)
    }

    //登录组件的统一启动入口
    //每次程序启动都显示登录界面，填上一次保存的用户名，不登录
    function beginLogin() {
        //重置登录完成状态
        root.loginCompleted = false

        //读取上一次成功登录后保存的用户名
        const savedUserName = controller.savedUserName().trim()

        //无论是否读取到用户名，都把结果写入输入框
        loginNameField.text = savedUserName

        //清除上一次可能残留的错误信息
        loginErrorText.text = ""

        //每次启动都显示登录窗口
        root.show()

        //没有登录过时让用户名输入框获得键盘焦点
        if (savedUserName.length > 0){
            loginButton.forceActiveFocus()
            return
        }
        //没有已保存用户名时，让用户名输入框获得键盘焦点
        loginNameField.forceActiveFocus()
    }

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

                //固定提示文字颜色，避免暗色模式下的显示为问题
                placeholderTextColor: "#808080"

                //固定选中文字的背景颜色
                selectionColor: "#12B7F5"

                //固定被选中文字的颜色
                selectedTextColor: "#FFFFFF"

                //覆盖Qt Quick Controls根据系统主题生成的背景，在系统暗色模式下，输入框仍然保持白色
                background: Rectangle {
                    //输入框获得焦点时显示蓝色边框，否则显示灰色边框
                    border.color: loginNameField.activeFocus ? "#12B7F5" : "#C8C8C8"

                    border.width: 1

                    color: "#FFFFFF"

                    radius: 4
                }
                //用户在输入框中按下回车后执行手动登录
                onAccepted: root.submitLogin()

                //用户开始修改用户名时清除旧错误信息
                onTextChanged: loginErrorText.text = ""
            }

            //错误提示（默认不显示）
            Text {
                id: loginErrorText
                text: ""
                color: "red"
            }

            //登录按钮
            Rectangle {
                id: loginButton
                //允许该自定义按钮通过Tab键获得键盘焦点
                activeFocusOnTab: true

                //让按钮占满ColumnLayout提供的横向空间
                Layout.fillWidth: true

                Layout.preferredHeight: 30

                radius: 4

                //按下按钮时使用更深的颜色
                color: loginTapHandler.pressed ? "#0F8FD1" : "#12B7F5"

                //显示登录按钮文字
                Text {
                    //让文字区域和按钮保持相同宽度
                    width: parent.width

                    //让文字区域和按钮保持相同高度
                    height: parent.height

                    text: qsTr("登录")

                    color: "white"

                    //让文字水平居中
                    horizontalAlignment: Text.AlignHCenter

                    //垂直居中
                    verticalAlignment: Text.AlignVCenter
                }

                TapHandler {
                    id: loginTapHandler

                    acceptedButtons: Qt.LeftButton

                    //按下和释放都在按钮范围内
                    gesturePolicy: TapHandler.ReleaseWithinBounds

                    onTapped: root.submitLogin()
                }
                //处理登录按钮获得焦点后的键盘输入
                Keys.onPressed: function(event) {
                    //回车键执行登录
                    if (event.key === Qt.Key_Return) {
                        root.submitLogin()

                        //标记该键盘事件已经处理，避免继续向其他对象传递
                        event.accepted = true
                    }
                }
            }
        }
    }

    //关闭登录界面 且 没有登录
    onClosing: function(close){
        if(!root.loginCompleted)
            Qt.quit()
    }
}