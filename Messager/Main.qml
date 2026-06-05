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
//     [v0.1.2] HeZhiyuan    2026-06-03 17:26:58
//         * 点击左侧用户后弹出聊天框。只有currentPeerId不为空，聊天框才显示
//     [v0.1.2] HeZhiyuan    2026-06-05 19:31:27
//         * Main.qml改为程序入口。真正的窗口内容放到MessageWindow.qml，方便后续维护。
import QtQuick

MessageWindow{
}
