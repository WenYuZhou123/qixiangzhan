import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ScrollView {
    id: root
    property var controller
    readonly property var store: controller.realtimeGateway.stateStore
    readonly property bool mobile: controller.androidMode || width < 980
    readonly property bool phone: width < theme.breakpointCompact || controller.androidMode
    readonly property bool busy: controller.commandPending
    readonly property bool connected: controller.commandConnected
    readonly property bool padDevice: store.protocolProfile === "airport_pad_v1"

    property string confirmTitle: ""
    property string confirmText: ""
    property var confirmAction

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
                font.pixelSize: theme.bodySize(root.mobile)
                wrapMode: Text.Wrap
            }
        }
    }

    function ask(title, text, action) {
        confirmTitle = title
        confirmText = text
        confirmAction = action
        confirmDialog.open()
    }

    Dialog {
        id: confirmDialog
        title: root.confirmTitle
        modal: true
        standardButtons: Dialog.Ok | Dialog.Cancel
        x: Math.max(12, (root.width - width) / 2)
        y: 80
        width: Math.min(420, root.width - 24)

        Label {
            width: parent.width
            text: root.confirmText
            wrapMode: Text.Wrap
            color: theme.textBody
            font.pixelSize: 14
        }

        onAccepted: {
            if (root.confirmAction)
                root.confirmAction()
        }
    }

    ColumnLayout {
        width: root.availableWidth
        spacing: theme.sectionGap

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: root.mobile ? 142 : 132
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
                            text: "控制中心"
                            color: theme.textPrimary
                            font.pixelSize: theme.heroTitleSize(root.mobile)
                            font.bold: true
                            elide: Text.ElideRight
                        }

                        Label {
                            Layout.fillWidth: true
                            text: store.currentDeviceId.length > 0 ? store.currentDeviceId : "尚未选择设备"
                            color: "#b9c7d1"
                            font.pixelSize: 13
                            elide: Text.ElideRight
                        }
                    }

                    StatusPill {
                        textLabel: root.busy ? "等待 ACK" : (root.connected ? "控制可用" : "等待连接")
                        fill: root.busy ? theme.pending : (root.connected ? theme.success : theme.danger)
                    }
                }

                GridLayout {
                    Layout.fillWidth: true
                    columns: root.phone ? 2 : 5
                    columnSpacing: 8
                    rowSpacing: 8

                    StatusPill {
                        textLabel: "R1 " + theme.relayStateText(store.relay1On)
                        fill: store.relay1On ? theme.success : theme.offline
                    }

                    StatusPill {
                        textLabel: "R2 " + theme.relayStateText(store.relay2On)
                        fill: store.relay2On ? theme.success : theme.offline
                    }

                    StatusPill {
                        textLabel: theme.protocolText(store.protocolProfile)
                        fill: theme.pending
                    }

                    StatusPill {
                        textLabel: root.padDevice ? theme.padModeText(store.padMode) : theme.connectionStateText(controller.runtimeConnectionState)
                        fill: root.connected ? theme.success : theme.offline
                    }

                    StatusPill {
                        textLabel: store.activeAlarmCount > 0 ? "告警 " + store.activeAlarmCount : "无告警"
                        fill: store.activeAlarmCount > 0 ? theme.danger : theme.success
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            radius: theme.radiusMedium
            color: root.busy ? "#eaf2fb" : (store.lastError.length > 0 ? "#f8e2e5" : "#edf7ef")
            border.color: root.busy ? "#9bbde2" : (store.lastError.length > 0 ? "#e4a7af" : "#abd2bb")
            border.width: 1
            implicitHeight: feedback.implicitHeight + 20

            RowLayout {
                id: feedback
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: 10
                spacing: 10

                StatusPill {
                    textLabel: root.busy ? "PENDING" : (store.lastError.length > 0 ? "ERROR" : "READY")
                    fill: root.busy ? theme.pending : (store.lastError.length > 0 ? theme.danger : theme.success)
                }

                Label {
                    Layout.fillWidth: true
                    text: store.lastError.length > 0
                          ? store.lastError
                          : (store.lastAckSummary.length > 0 ? store.lastAckSummary : "命令链路空闲")
                    color: theme.textBody
                    font.pixelSize: theme.bodySize(root.mobile)
                    font.bold: true
                    wrapMode: Text.Wrap
                }

                OpsButton {
                    visible: store.lastError.length > 0
                    text: "重试"
                    Layout.preferredWidth: 86
                    onClicked: root.controller.retryLastCommand()
                }
            }
        }

        GridLayout {
            Layout.fillWidth: true
            columns: root.mobile ? 1 : 2
            columnSpacing: theme.sectionGap
            rowSpacing: theme.sectionGap

            SectionCard {
                title: "继电器控制"
                subtitle: "单路动作直接执行，批量动作需要确认。"

                GridLayout {
                    Layout.fillWidth: true
                    columns: root.phone ? 1 : 2
                    columnSpacing: 10
                    rowSpacing: 10

                    OpsButton {
                        Layout.fillWidth: true
                        text: "R1 开启"
                        pending: root.busy
                        enabled: root.connected && !root.busy
                        fillColor: "#dcecef"
                        labelColor: theme.accentCyanDeep
                        onClicked: root.controller.setRelay1(true)
                    }

                    OpsButton {
                        Layout.fillWidth: true
                        text: "R1 关闭"
                        pending: root.busy
                        enabled: root.connected && !root.busy
                        onClicked: root.controller.setRelay1(false)
                    }

                    OpsButton {
                        Layout.fillWidth: true
                        text: "R2 开启"
                        pending: root.busy
                        enabled: root.connected && !root.busy
                        fillColor: "#dcecef"
                        labelColor: theme.accentCyanDeep
                        onClicked: root.controller.setRelay2(true)
                    }

                    OpsButton {
                        Layout.fillWidth: true
                        text: "R2 关闭"
                        pending: root.busy
                        enabled: root.connected && !root.busy
                        onClicked: root.controller.setRelay2(false)
                    }

                    OpsButton {
                        Layout.fillWidth: true
                        text: "全部开启"
                        enabled: root.connected && !root.busy
                        fillColor: "#f3eadf"
                        labelColor: "#6c471f"
                        onClicked: root.ask("确认全部开启", "将同时开启 R1 和 R2，请确认现场设备允许执行。", function() { root.controller.setAll(true) })
                    }

                    OpsButton {
                        Layout.fillWidth: true
                        text: "全部关闭"
                        enabled: root.connected && !root.busy
                        fillColor: "#f3eadf"
                        labelColor: "#6c471f"
                        onClicked: root.ask("确认全部关闭", "将同时关闭 R1 和 R2，请确认不会影响现场流程。", function() { root.controller.setAll(false) })
                    }
                }

                OpsButton {
                    Layout.fillWidth: true
                    text: "刷新设备状态"
                    enabled: root.connected && !root.busy
                    onClicked: root.controller.queryStatus()
                }
            }

            SectionCard {
                title: "状态反馈"
                subtitle: "控制动作以设备回执和最新状态为准。"

                GridLayout {
                    Layout.fillWidth: true
                    columns: root.phone ? 1 : 2
                    columnSpacing: 10
                    rowSpacing: 10

                    MetricTile {
                        mobile: root.mobile
                        title: "最近 ACK"
                        value: store.lastAckSummary.length > 0 ? store.lastAckSummary : "等待回执"
                        caption: root.busy ? "命令处理中" : "控制通道空闲"
                        accent: root.busy ? theme.pending : theme.statusColor(store.lastAckResult)
                    }

                    MetricTile {
                        mobile: root.mobile
                        title: "链路状态"
                        value: theme.connectionStateText(controller.runtimeConnectionState)
                        caption: root.connected ? "可以下发控制命令" : "检查 API 或实时链路"
                        accent: root.connected ? theme.success : theme.danger
                    }

                    MetricTile {
                        mobile: root.mobile
                        title: "控制模式"
                        value: root.padDevice ? theme.padModeText(store.padMode) : theme.protocolText(store.protocolProfile)
                        caption: root.padDevice ? theme.occupancyText(store.padOccupied) : "继电器开关控制"
                    }

                    MetricTile {
                        mobile: root.mobile
                        title: "环境摘要"
                        value: store.windCapability ? "风 " + Number(store.windSpeed).toFixed(1) + " m/s" : "环境数据就绪"
                        caption: store.rainCapability ? (store.rainDetected ? "雨滴已检测" : "无雨滴") : "PM2.5 " + Number(store.pm25).toFixed(0)
                    }
                }
            }
        }

        SectionCard {
            title: "停机坪控制"
            subtitle: root.padDevice ? "停机坪动作需要确认，停止动作直接执行。" : "当前设备未启用停机坪协议。"

            Loader {
                Layout.fillWidth: true
                sourceComponent: root.padDevice ? padControlCard : padFallbackCard
            }
        }
    }

    Component {
        id: padControlCard

        ColumnLayout {
            spacing: 10

            GridLayout {
                Layout.fillWidth: true
                columns: root.phone ? 1 : 3
                columnSpacing: 10
                rowSpacing: 10

                OpsButton {
                    Layout.fillWidth: true
                    text: "开启停机坪"
                    enabled: root.connected && !root.busy
                    fillColor: "#dcecef"
                    labelColor: theme.accentCyanDeep
                    onClicked: root.ask("确认开启停机坪", "将执行停机坪开启动作，请确认现场无机械阻挡。", function() { root.controller.padOpen() })
                }

                OpsButton {
                    Layout.fillWidth: true
                    text: "关闭停机坪"
                    enabled: root.connected && !root.busy
                    fillColor: "#f3eadf"
                    labelColor: "#6c471f"
                    onClicked: root.ask("确认关闭停机坪", "将执行停机坪关闭动作，请确认现场允许关闭。", function() { root.controller.padClose() })
                }

                OpsButton {
                    Layout.fillWidth: true
                    text: "停止动作"
                    enabled: root.connected && !root.busy
                    fillColor: "#f8e2e5"
                    labelColor: theme.danger
                    onClicked: root.controller.padStop()
                }
            }

            GridLayout {
                Layout.fillWidth: true
                columns: root.phone ? 1 : 3
                columnSpacing: 10
                rowSpacing: 10

                MetricTile {
                    mobile: root.mobile
                    title: "左门"
                    value: theme.padStateText(store.padLeftState)
                    caption: "机械门 A"
                    accent: theme.stateColor(store.padLeftState)
                }

                MetricTile {
                    mobile: root.mobile
                    title: "右门"
                    value: theme.padStateText(store.padRightState)
                    caption: "机械门 B"
                    accent: theme.stateColor(store.padRightState)
                }

                MetricTile {
                    mobile: root.mobile
                    title: "占用与就绪"
                    value: theme.occupancyText(store.padOccupied)
                    caption: store.padReady ? "允许降落" : "待命"
                    accent: theme.readinessColor(store.padReady)
                }
            }

            OpsButton {
                Layout.fillWidth: true
                text: "刷新停机坪状态"
                enabled: root.connected && !root.busy
                onClicked: root.controller.queryPadStatus()
            }
        }
    }

    Component {
        id: padFallbackCard

        MetricTile {
            mobile: root.mobile
            title: "协议状态"
            value: theme.protocolText(store.protocolProfile)
            caption: "接入 airport_pad_v1 后自动显示停机坪动作。"
            accent: theme.offline
        }
    }
}
