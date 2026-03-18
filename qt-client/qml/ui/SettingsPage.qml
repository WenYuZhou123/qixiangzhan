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
        implicitHeight: body.implicitHeight + 44

        default property alias cardData: body.data

        ColumnLayout {
            id: body
            anchors.fill: parent
            anchors.margins: 18
            spacing: 12

            Label {
                text: parent.parent.title
                color: theme.textBody
                font.pixelSize: root.mobile ? 24 : 22
                font.bold: true
            }

            Label {
                text: parent.parent.subtitle
                color: theme.textMuted
                wrapMode: Text.Wrap
                font.pixelSize: root.mobile ? 16 : 13
            }
        }
    }

    component InfoRow: Item {
        required property string label
        required property string value
        Layout.fillWidth: true
        implicitHeight: infoColumn.implicitHeight

        Column {
            id: infoColumn
            width: parent.width
            spacing: 4

            Label {
                text: parent.parent.label
                color: theme.textMuted
                font.pixelSize: root.mobile ? 15 : 12
            }

            Label {
                width: parent.width
                text: parent.parent.value
                wrapMode: Text.WrapAnywhere
                color: theme.textBody
                font.pixelSize: root.mobile ? 18 : 16
                font.bold: true
            }
        }
    }

    ColumnLayout {
        width: root.availableWidth
        spacing: 16

        ConnectionPage {
            Layout.fillWidth: true
            gateway: root.controller.realtimeGateway
            visible: !root.controller.androidMode && root.controller.engineeringMode
        }

        Rectangle {
            Layout.fillWidth: true
            radius: 30
            implicitHeight: mobile ? 196 : 188
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
                    text: "远程接入、缓存策略与工程模式开关都在这里统一管理。"
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

                GridLayout {
                    Layout.fillWidth: true
                    columns: mobile ? 1 : 2
                    columnSpacing: 12
                    rowSpacing: 12

                    Button {
                        Layout.fillWidth: true
                        text: root.controller.authSession.authenticated ? "前往登录管理" : "前往登录"
                        onClicked: root.controller.currentPage = "login"

                        contentItem: Text {
                            text: parent.text
                            color: "#102434"
                            font: parent.font
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }

                        background: Rectangle {
                            radius: 20
                            color: "#dce7ec"
                            border.color: "#c4d2da"
                            border.width: 1
                        }
                    }

                    Button {
                        Layout.fillWidth: true
                        visible: root.controller.authSession.authenticated
                        enabled: root.controller.authSession.authenticated
                        text: "退出登录"
                        onClicked: root.controller.authSession.logout()

                        contentItem: Text {
                            text: parent.text
                            color: "#173042"
                            font: parent.font
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }

                        background: Rectangle {
                            radius: 20
                            color: "#eef3f5"
                            border.color: "#c9d4db"
                            border.width: 1
                        }
                    }
                }
            }
        }

        GridLayout {
            Layout.fillWidth: true
            columns: mobile ? 1 : 2
            columnSpacing: 16
            rowSpacing: 16

            InfoCard {
                title: "远程接入"
                subtitle: "公网和局域网都通过 API Base 统一接入；移动端默认优先使用后端接口。"

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Label {
                        text: "接口地址"
                        color: theme.textMuted
                        font.pixelSize: root.mobile ? 15 : 12
                    }

                    TextField {
                        Layout.fillWidth: true
                        text: root.controller.authSession.apiBaseUrl
                        font.pixelSize: root.mobile ? 18 : 15
                        onEditingFinished: root.controller.authSession.apiBaseUrl = text
                    }

                    InfoRow {
                        label: "云端链路"
                        value: theme.connectionStateText(root.controller.remoteSyncService.connectionState)
                    }

                    InfoRow {
                        label: "移动端策略"
                        value: "移动端优先使用快速轮询；WebSocket 可用时会自动切到实时推送。"
                    }
                }
            }

            InfoCard {
                title: "模式与权限"
                subtitle: "工程模式仅供桌面管理员使用，用于串口、本地 MQTT 与旧继电器联调。"

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
                            font.pixelSize: root.mobile ? 16 : 13
                        }

                        Switch {
                            checked: root.controller.engineeringMode
                            enabled: !root.controller.androidMode && root.controller.authSession.admin
                            text: checked ? "已开启" : "已关闭"
                            font.pixelSize: root.mobile ? 16 : 13
                            onToggled: root.controller.engineeringMode = checked
                        }
                    }

                    InfoRow {
                        label: "说明"
                        value: root.controller.engineeringMode
                               ? "当前可访问串口诊断、旧设备控制与本地 MQTT 联调能力。"
                               : "当前优先使用远程 API，适合公网和移动端接入。"
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
                        value: "每台设备保留最近 7 天数据，单设备最多缓存 10000 条消息。"
                    }

                    InfoRow {
                        label: "刷新策略"
                        value: root.controller.androidMode
                               ? "设备页高频轮询，其他页面低频同步。"
                               : "桌面端轮询更稳，适合值守与联动控制。"
                    }
                }
            }

            InfoCard {
                title: "告警规则"
                subtitle: "系统会把设备离线、命令超时、低 RSSI 和链路异常统一收口到告警中心。"

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 12

                    InfoRow {
                        label: "触发条件"
                        value: "设备离线、命令 ACK 超时、命令失败、MQTT 断连和低 RSSI。"
                    }

                    InfoRow {
                        label: "控制反馈"
                        value: "状态上报与命令结果会一起回写，按钮会在状态确认后尽快恢复。"
                    }
                }
            }
        }
    }
}
