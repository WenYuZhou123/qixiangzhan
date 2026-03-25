import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ScrollView {
    id: root
    property var controller
    clip: true
    readonly property bool mobile: controller && controller.androidMode

    TaskTheme { id: theme }

    component InfoCard: Rectangle {
        required property string title
        required property string subtitle
        Layout.fillWidth: true
        radius: 28
        color: "#ffffff"
        border.color: theme.borderSoft
        border.width: 1
        implicitHeight: cardBody.implicitHeight + 44

        default property alias cardData: cardBody.data

        ColumnLayout {
            id: cardBody
            anchors.fill: parent
            anchors.margins: 18
            spacing: 12

            Label {
                text: parent.parent.title
                color: theme.textBody
                font.pixelSize: mobile ? 24 : 22
                font.bold: true
            }

            Label {
                text: parent.parent.subtitle
                color: theme.textMuted
                wrapMode: Text.Wrap
                font.pixelSize: mobile ? 16 : 13
            }
        }
    }

    component InfoRow: Item {
        required property string label
        required property string value
        Layout.fillWidth: true
        implicitHeight: rowBody.implicitHeight

        Column {
            id: rowBody
            width: parent.width
            spacing: 4

            Label {
                text: parent.parent.label
                color: theme.textMuted
                font.pixelSize: mobile ? 15 : 12
            }

            Label {
                width: parent.width
                text: parent.parent.value
                wrapMode: Text.WrapAnywhere
                color: theme.textBody
                font.pixelSize: mobile ? 18 : 16
                font.bold: true
            }
        }
    }

    ColumnLayout {
        width: root.availableWidth
        spacing: theme.sectionGap

        Rectangle {
            Layout.fillWidth: true
            radius: 30
            implicitHeight: mobile ? 212 : 194
            gradient: Gradient {
                GradientStop { position: 0.0; color: "#0d1f2f" }
                GradientStop { position: 0.55; color: "#16384f" }
                GradientStop { position: 1.0; color: "#315a74" }
            }
            border.color: "#7193a7"
            border.width: 1

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: mobile ? 18 : 22
                spacing: 12

                Label {
                    text: "平台设置"
                    color: theme.textPrimary
                    font.pixelSize: mobile ? 28 : 34
                    font.bold: true
                }

                Label {
                    text: "桌面和安卓都使用同一套 API Base 配置。移动端默认按局域网调试优先，公网地址继续保留为可切换选项。"
                    color: "#d0ddea"
                    wrapMode: Text.Wrap
                    font.pixelSize: mobile ? 15 : 14
                }

                GridLayout {
                    Layout.fillWidth: true
                    columns: mobile ? 2 : 3
                    columnSpacing: 10
                    rowSpacing: 10

                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: 42
                        radius: 18
                        color: "#10ffffff"
                        border.color: "#8ab0c8"
                        border.width: 1

                        Label {
                            anchors.centerIn: parent
                            text: theme.connectionStateText(root.controller.remoteSyncService.connectionState)
                            color: "#f0f6f9"
                            font.pixelSize: 14
                            font.bold: true
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: 42
                        radius: 18
                        color: "#10ffffff"
                        border.color: "#8ab0c8"
                        border.width: 1

                        Label {
                            anchors.centerIn: parent
                            text: theme.roleText(root.controller.authSession.role)
                            color: "#f0f6f9"
                            font.pixelSize: 14
                            font.bold: true
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: 42
                        radius: 18
                        color: "#10ffffff"
                        border.color: "#8ab0c8"
                        border.width: 1

                        Label {
                            anchors.centerIn: parent
                            text: root.controller.engineeringMode ? "工程模式" : "远程模式"
                            color: "#f0f6f9"
                            font.pixelSize: 14
                            font.bold: true
                        }
                    }
                }
            }
        }

        GridLayout {
            Layout.fillWidth: true
            columns: mobile ? 1 : 2
            columnSpacing: theme.sectionGap
            rowSpacing: theme.sectionGap

            InfoCard {
                title: "局域网 API"
                subtitle: "安卓和桌面共用同一入口。手机接入时，把 API Base 改成电脑在局域网里的地址；本地联调默认可用 127.0.0.1。"

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Label {
                        text: "API Base"
                        color: theme.textMuted
                        font.pixelSize: mobile ? 15 : 12
                    }

                    TextField {
                        Layout.fillWidth: true
                        text: root.controller.authSession.apiBaseUrl
                        placeholderText: "例如 http://192.168.1.20:8000/api/v1"
                        font.pixelSize: mobile ? 18 : 15
                        onEditingFinished: root.controller.authSession.apiBaseUrl = text
                    }

                    InfoRow {
                        label: "当前链路"
                        value: theme.connectionStateText(root.controller.remoteSyncService.connectionState)
                    }

                    InfoRow {
                        label: "调试建议"
                        value: "桌面本机： http://127.0.0.1:8000/api/v1\n安卓手机： 改成电脑局域网 IP，例如 http://192.168.1.20:8000/api/v1"
                    }
                }
            }

            InfoCard {
                title: "模式与权限"
                subtitle: "工程模式仅保留给桌面管理员，用于串口、本地 MQTT 和联调入口；统一交互主线始终走概览、控制、气象、设置。"

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 12

                    InfoRow {
                        label: "当前角色"
                        value: theme.roleText(root.controller.authSession.role)
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 12

                        Label {
                            text: "工程模式"
                            color: theme.textMuted
                            font.pixelSize: mobile ? 16 : 13
                        }

                        Switch {
                            checked: root.controller.engineeringMode
                            enabled: !root.controller.androidMode && root.controller.authSession.admin
                            text: checked ? "已开启" : "已关闭"
                            font.pixelSize: mobile ? 16 : 13
                            onToggled: root.controller.engineeringMode = checked
                        }
                    }

                    InfoRow {
                        label: "说明"
                        value: root.controller.engineeringMode
                               ? "当前可访问串口、日志和工程页，但桌面与安卓的核心控制流程仍然保持一致。"
                               : "当前优先使用远程 API 与实时同步，适合桌面值守和手机控制。"
                    }
                }
            }

            InfoCard {
                title: "缓存与数据库"
                subtitle: "本地缓存用于断线恢复与历史浏览；敏感令牌优先写入系统安全存储。"

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 12

                    InfoRow {
                        label: "本地数据库"
                        value: root.controller.localDatabasePath
                    }

                    InfoRow {
                        label: "缓存策略"
                        value: "设备状态、历史、告警和待确认命令都会进入本地缓存，确保桌面与安卓掉线后能保留最近状态。"
                    }

                    InfoRow {
                        label: "移动端策略"
                        value: "安卓优先走 WebSocket；如果实时链路不可用，会自动回落到轮询同步。"
                    }
                }
            }

            InfoCard {
                title: "控制与告警"
                subtitle: "继电器与停机坪控制继续沿用统一命令集合，状态变化会同步写入历史和告警。"

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 12

                    InfoRow {
                        label: "命令集合"
                        value: "set_r1、set_r2、set_all、query_status，以及停机坪协议下的 pad_open、pad_close、pad_stop、query_pad_status。"
                    }

                    InfoRow {
                        label: "反馈逻辑"
                        value: "控制页、概览页和 LCD 均以状态回传为准。按钮提交后会等待 ACK 或新状态，避免误判执行结果。"
                    }
                }
            }
        }
    }
}
