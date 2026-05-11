import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ScrollView {
    id: root
    property var controller
    readonly property bool mobile: controller && controller.androidMode

    clip: true

    TaskTheme { id: theme }

    component SectionCard: Rectangle {
        default property alias content: body.data
        property string title: ""
        property string subtitle: ""

        Layout.fillWidth: true
        radius: theme.radiusLarge
        color: theme.surfacePrimary
        border.color: theme.borderSoft
        border.width: 1
        implicitHeight: body.implicitHeight + 24

        ColumnLayout {
            id: body
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 12
            spacing: 10

            Label {
                Layout.fillWidth: true
                text: parent.parent.title
                color: theme.textBody
                font.pixelSize: theme.pageTitleSize(root.mobile)
                font.bold: true
            }

            Label {
                visible: parent.parent.subtitle.length > 0
                Layout.fillWidth: true
                text: parent.parent.subtitle
                color: theme.textMuted
                wrapMode: Text.Wrap
                font.pixelSize: theme.bodySize(root.mobile)
            }
        }
    }

    ColumnLayout {
        width: root.availableWidth
        spacing: theme.sectionGap

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: mobile ? 140 : 128
            radius: theme.radiusLarge
            color: theme.navSurface
            border.color: "#2f3d4c"
            border.width: 1

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 16
                spacing: 10

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 3

                        Label {
                            Layout.fillWidth: true
                            text: "平台设置"
                            color: theme.textPrimary
                            font.pixelSize: theme.heroTitleSize(mobile)
                            font.bold: true
                            elide: Text.ElideRight
                        }

                        Label {
                            Layout.fillWidth: true
                            text: "桌面端与 Android 共用同一套 API、缓存和健康检查。"
                            color: "#b9c7d1"
                            font.pixelSize: 13
                            wrapMode: Text.Wrap
                        }
                    }

                    StatusPill {
                        textLabel: root.controller.remoteSyncService.systemHealthStatus.toUpperCase()
                        fill: theme.statusColor(root.controller.remoteSyncService.systemHealthStatus)
                    }
                }

                GridLayout {
                    Layout.fillWidth: true
                    columns: mobile ? 2 : 4
                    columnSpacing: 8
                    rowSpacing: 8

                    StatusPill {
                        textLabel: theme.connectionStateText(root.controller.remoteSyncService.connectionState)
                        fill: root.controller.remoteSyncService.connected ? theme.success : theme.danger
                    }

                    StatusPill {
                        textLabel: theme.roleText(root.controller.authSession.role)
                        fill: root.controller.authSession.authenticated ? theme.pending : theme.offline
                    }

                    StatusPill {
                        textLabel: root.controller.remoteSyncService.healthMqttConnected ? "MQTT UP" : "MQTT DOWN"
                        fill: root.controller.remoteSyncService.healthMqttConnected ? theme.success : theme.danger
                    }

                    StatusPill {
                        textLabel: "Disk " + root.controller.remoteSyncService.healthDiskSummary
                        fill: root.controller.remoteSyncService.healthDiskFreePercent < 10 ? theme.danger : theme.success
                    }
                }
            }
        }

        GridLayout {
            Layout.fillWidth: true
            columns: mobile ? 1 : 2
            columnSpacing: theme.sectionGap
            rowSpacing: theme.sectionGap

            SectionCard {
                title: "后端连接"
                subtitle: "Android 调试时把 API Base 改成 Jetson 或电脑的局域网地址。"

                Label {
                    Layout.fillWidth: true
                    text: "API Base"
                    color: theme.textMuted
                    font.pixelSize: theme.labelSize(mobile)
                }

                TextField {
                    Layout.fillWidth: true
                    text: root.controller.authSession.apiBaseUrl
                    placeholderText: "例如 https://qixiangzhan.online/api/v1"
                    font.pixelSize: mobile ? 16 : 14
                    selectByMouse: true
                    onEditingFinished: root.controller.authSession.apiBaseUrl = text
                }

                InfoRow {
                    mobile: root.mobile
                    label: "当前链路"
                    value: theme.connectionStateText(root.controller.remoteSyncService.connectionState)
                }

                InfoRow {
                    mobile: root.mobile
                    label: "最后错误"
                    value: root.controller.remoteSyncService.lastError.length > 0 ? root.controller.remoteSyncService.lastError : "无"
                }
            }

            SectionCard {
                title: "Jetson 健康"
                subtitle: "来自 /system/health，用于现场快速判断主站状态。"

                GridLayout {
                    Layout.fillWidth: true
                    columns: mobile ? 1 : 2
                    columnSpacing: 10
                    rowSpacing: 10

                    MetricTile {
                        mobile: root.mobile
                        title: "API"
                        value: root.controller.remoteSyncService.healthApiOk ? "OK" : "UNKNOWN"
                        caption: "运行 " + root.controller.remoteSyncService.healthUptimeText
                        accent: root.controller.remoteSyncService.healthApiOk ? theme.success : theme.offline
                    }

                    MetricTile {
                        mobile: root.mobile
                        title: "MySQL"
                        value: root.controller.remoteSyncService.healthMysqlOk ? "OK" : "DOWN"
                        caption: root.controller.remoteSyncService.healthMysqlSummary
                        accent: root.controller.remoteSyncService.healthMysqlOk ? theme.success : theme.danger
                    }

                    MetricTile {
                        mobile: root.mobile
                        title: "MQTT"
                        value: root.controller.remoteSyncService.healthMqttConnected ? "UP" : "DOWN"
                        caption: root.controller.remoteSyncService.healthMqttSummary
                        accent: root.controller.remoteSyncService.healthMqttConnected ? theme.success : theme.danger
                    }

                    MetricTile {
                        mobile: root.mobile
                        title: "磁盘"
                        value: Number(root.controller.remoteSyncService.healthDiskFreePercent).toFixed(1)
                        unit: "%"
                        caption: "剩余空间"
                        fillRatio: Math.max(0, Math.min(1, root.controller.remoteSyncService.healthDiskFreePercent / 100.0))
                        accent: root.controller.remoteSyncService.healthDiskFreePercent < 10 ? theme.danger : theme.success
                    }
                }

                InfoRow {
                    mobile: root.mobile
                    label: "最近设备心跳"
                    value: root.controller.remoteSyncService.healthLastSeenAt
                }

                InfoRow {
                    mobile: root.mobile
                    label: "最近气象入库"
                    value: root.controller.remoteSyncService.healthLastWeatherAt
                }
            }

            SectionCard {
                title: "账号与模式"
                subtitle: "工程模式仅桌面管理员可开启，Android 保持现场操作主线。"

                InfoRow {
                    mobile: root.mobile
                    label: "当前用户"
                    value: root.controller.authSession.authenticated
                           ? (root.controller.authSession.displayName.length > 0 ? root.controller.authSession.displayName : root.controller.authSession.username)
                           : "未登录"
                }

                InfoRow {
                    mobile: root.mobile
                    label: "当前角色"
                    value: theme.roleText(root.controller.authSession.role)
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10

                    Label {
                        Layout.fillWidth: true
                        text: "工程模式"
                        color: theme.textBody
                        font.pixelSize: mobile ? 16 : 14
                        font.bold: true
                    }

                    Switch {
                        checked: root.controller.engineeringMode
                        enabled: !root.controller.androidMode && root.controller.authSession.admin
                        text: checked ? "已开启" : "已关闭"
                        font.pixelSize: mobile ? 15 : 13
                        onToggled: root.controller.engineeringMode = checked
                    }
                }
            }

            SectionCard {
                title: "本地缓存"
                subtitle: "离线恢复、历史浏览和待确认命令会进入本地 SQLite 缓存。"

                InfoRow {
                    mobile: root.mobile
                    label: "本地数据库"
                    value: root.controller.localDatabasePath
                }

                InfoRow {
                    mobile: root.mobile
                    label: "设备概况"
                    value: root.controller.remoteSyncService.healthDeviceSummary
                }

                InfoRow {
                    mobile: root.mobile
                    label: "移动端策略"
                    value: "优先使用 WebSocket，实时链路不可用时自动回落到轮询同步。"
                }
            }
        }
    }
}
