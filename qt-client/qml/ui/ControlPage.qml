import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ScrollView {
    id: root
    property var controller
    readonly property var store: controller.realtimeGateway.stateStore
    readonly property bool mobile: controller.androidMode || width < 1080
    readonly property bool phone: width < theme.breakpointCompact || controller.androidMode
    readonly property bool busy: controller.commandPending
    readonly property bool connected: controller.commandConnected
    readonly property bool padDevice: store.protocolProfile === "airport_pad_v1"

    clip: true

    TaskTheme { id: theme }

    component ActionButton: Button {
        property color fillColor: theme.glassStrong
        property color labelColor: theme.textBody
        property color borderColor: theme.borderSoft
        Layout.fillWidth: true
        implicitHeight: theme.touchTarget + 4

        contentItem: Text {
            text: parent.text
            color: parent.enabled ? parent.labelColor : "#73838d"
            font.pixelSize: theme.actionSize(root.mobile)
            font.bold: true
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }

        background: Rectangle {
            radius: 20
            color: parent.enabled ? parent.fillColor : theme.neutral
            border.color: parent.enabled ? parent.borderColor : "#c8d3d8"
            border.width: 1
            opacity: parent.down ? 0.88 : 1.0
        }
    }

    component MetricTile: Rectangle {
        required property string title
        required property string value
        required property string caption
        Layout.fillWidth: true
        radius: theme.radiusMedium
        color: theme.surfacePrimary
        border.color: theme.borderSoft
        border.width: 1
        implicitHeight: 96

        Column {
            anchors.fill: parent
            anchors.margins: 14
            spacing: 4

            Label {
                text: parent.parent.title
                color: theme.textMuted
                font.pixelSize: theme.labelSize(root.mobile)
            }

            Label {
                text: parent.parent.value
                color: theme.textBody
                font.pixelSize: root.mobile ? 21 : 22
                font.bold: true
            }

            Label {
                width: parent.width
                text: parent.parent.caption
                color: theme.textMuted
                font.pixelSize: 11
                elide: Text.ElideRight
            }
        }
    }

    component SectionCard: Rectangle {
        Layout.fillWidth: true
        radius: theme.radiusLarge
        color: "#fbfcfd"
        border.color: theme.borderSoft
        border.width: 1
    }

    ColumnLayout {
        width: root.availableWidth
        spacing: theme.sectionGap

        Rectangle {
            Layout.fillWidth: true
            radius: theme.radiusLarge
            gradient: Gradient {
                GradientStop { position: 0.0; color: theme.shellTop }
                GradientStop { position: 0.6; color: theme.shellMid }
                GradientStop { position: 1.0; color: "#294257" }
            }
            border.color: theme.borderStrong
            border.width: 1
            implicitHeight: mobile ? 238 : 246

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: mobile ? 18 : 22
                spacing: 12

                RowLayout {
                    Layout.fillWidth: true

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 4

                        Label {
                            text: "控制中心"
                            color: theme.textPrimary
                            font.pixelSize: theme.heroTitleSize(mobile)
                            font.bold: true
                        }

                        Label {
                            text: store.currentDeviceId.length > 0 ? store.currentDeviceId : "尚未选中设备"
                            color: "#cbd8e0"
                            font.pixelSize: mobile ? 15 : 16
                        }
                    }

                    Rectangle {
                        radius: 18
                        implicitWidth: mobile ? 112 : 138
                        implicitHeight: 40
                        color: connected ? theme.success : theme.danger
                        border.color: "#d9e5ea"
                        border.width: 1

                        Label {
                            anchors.centerIn: parent
                            text: connected ? "控制可用" : "等待连接"
                            color: "white"
                            font.pixelSize: 15
                            font.bold: true
                        }
                    }
                }

                Label {
                    width: parent.width
                    text: padDevice
                          ? "当前设备支持停机坪开合和继电器控制。桌面与安卓共用同一套控制语义，状态反馈统一落在这里。"
                          : "当前设备使用继电器协议，控制页优先提供 R1、R2 和全开全关操作，保持与本地 LCD 相同的状态表达。"
                    wrapMode: Text.Wrap
                    color: "#d2dee5"
                    font.pixelSize: theme.bodySize(mobile)
                }

                GridLayout {
                    Layout.fillWidth: true
                    columns: phone ? 2 : 4
                    columnSpacing: 10
                    rowSpacing: 10

                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: 38
                        radius: 16
                        color: "#12000000"
                        border.color: "#6d8999"
                        border.width: 1

                        Label {
                            anchors.centerIn: parent
                            text: "R1 " + theme.relayStateText(store.relay1On)
                            color: theme.textPrimary
                            font.pixelSize: 13
                            font.bold: true
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: 38
                        radius: 16
                        color: "#12000000"
                        border.color: "#6d8999"
                        border.width: 1

                        Label {
                            anchors.centerIn: parent
                            text: "R2 " + theme.relayStateText(store.relay2On)
                            color: theme.textPrimary
                            font.pixelSize: 13
                            font.bold: true
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: 38
                        radius: 16
                        color: "#12000000"
                        border.color: "#6d8999"
                        border.width: 1

                        Label {
                            anchors.centerIn: parent
                            text: padDevice ? theme.padStateText(store.padLeftState) : "继电器设备"
                            color: theme.textPrimary
                            font.pixelSize: 13
                            font.bold: true
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: 38
                        radius: 16
                        color: "#12000000"
                        border.color: "#6d8999"
                        border.width: 1

                        Label {
                            anchors.centerIn: parent
                            text: padDevice
                                  ? (store.padReady ? "允许降落" : "待命")
                                  : theme.connectionStateText(controller.runtimeConnectionState)
                            color: theme.textPrimary
                            font.pixelSize: 13
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

            SectionCard {
                implicitHeight: relayBody.implicitHeight + 36

                ColumnLayout {
                    id: relayBody
                    anchors.fill: parent
                    anchors.margins: 18
                    spacing: 12

                    Label {
                        text: "继电器控制"
                        color: theme.textBody
                        font.pixelSize: theme.pageTitleSize(mobile)
                        font.bold: true
                    }

                    Label {
                        width: parent.width
                        text: "R1、R2、全开全关和刷新状态在三端保持同一套动作含义。按钮触控尺寸按手机优先设置。"
                        wrapMode: Text.Wrap
                        color: theme.textMuted
                        font.pixelSize: theme.bodySize(mobile)
                    }

                    GridLayout {
                        Layout.fillWidth: true
                        columns: phone ? 1 : 2
                        columnSpacing: 12
                        rowSpacing: 12

                        ActionButton {
                            text: "R1 开启"
                            enabled: connected && !busy
                            fillColor: "#d9eaee"
                            labelColor: theme.accentCyanDeep
                            onClicked: controller.setRelay1(true)
                        }

                        ActionButton {
                            text: "R1 关闭"
                            enabled: connected && !busy
                            fillColor: theme.surfaceSecondary
                            onClicked: controller.setRelay1(false)
                        }

                        ActionButton {
                            text: "R2 开启"
                            enabled: connected && !busy
                            fillColor: "#d9eaee"
                            labelColor: theme.accentCyanDeep
                            onClicked: controller.setRelay2(true)
                        }

                        ActionButton {
                            text: "R2 关闭"
                            enabled: connected && !busy
                            fillColor: theme.surfaceSecondary
                            onClicked: controller.setRelay2(false)
                        }

                        ActionButton {
                            text: "全部开启"
                            enabled: connected && !busy
                            fillColor: "#ece4dc"
                            labelColor: "#5a4432"
                            onClicked: controller.setAll(true)
                        }

                        ActionButton {
                            text: "全部关闭"
                            enabled: connected && !busy
                            fillColor: "#ece4dc"
                            labelColor: "#5a4432"
                            onClicked: controller.setAll(false)
                        }
                    }

                    ActionButton {
                        text: "刷新继电器状态"
                        enabled: connected && !busy
                        fillColor: theme.glassStrong
                        labelColor: theme.textBody
                        onClicked: controller.queryStatus()
                    }
                }
            }

            SectionCard {
                implicitHeight: feedbackBody.implicitHeight + 36

                ColumnLayout {
                    id: feedbackBody
                    anchors.fill: parent
                    anchors.margins: 18
                    spacing: 12

                    Label {
                        text: "状态反馈"
                        color: theme.textBody
                        font.pixelSize: theme.pageTitleSize(mobile)
                        font.bold: true
                    }

                    GridLayout {
                        Layout.fillWidth: true
                        columns: phone ? 1 : 2
                        columnSpacing: 12
                        rowSpacing: 12

                        MetricTile {
                            title: "最后回执"
                            value: store.lastAckSummary.length > 0 ? store.lastAckSummary : "等待回执"
                            caption: busy ? "命令处理中" : "控制通道空闲"
                        }

                        MetricTile {
                            title: "天气联动"
                            value: theme.flightRuleText(store.windSpeed, store.visibility)
                            caption: "风速 " + Number(store.windSpeed).toFixed(1) + " m/s · 能见度 " + Number(store.visibility).toFixed(1) + " km"
                        }

                        MetricTile {
                            title: "控制模式"
                            value: padDevice ? theme.padModeText(store.padMode) : theme.protocolText(store.protocolProfile)
                            caption: padDevice ? theme.occupancyText(store.padOccupied) : "继电器开关控制"
                        }

                        MetricTile {
                            title: "连接状态"
                            value: theme.connectionStateText(controller.runtimeConnectionState)
                            caption: connected ? "可下发控制命令" : "请检查 API 或实时链路"
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        radius: theme.radiusMedium
                        color: "#eff4f7"
                        border.color: theme.borderSoft
                        border.width: 1
                        implicitHeight: 92

                        Column {
                            anchors.fill: parent
                            anchors.margins: 14
                            spacing: 8

                            Label {
                                text: "运行提示"
                                color: theme.textBody
                                font.pixelSize: 16
                                font.bold: true
                            }

                            Label {
                                width: parent.width
                                text: store.lastError.length > 0
                                      ? "错误： " + store.lastError
                                      : (busy ? "命令已发出，等待状态回传。" : "当前没有待确认命令。")
                                wrapMode: Text.Wrap
                                color: store.lastError.length > 0 ? theme.danger : theme.textMuted
                                font.pixelSize: theme.bodySize(mobile)
                            }
                        }
                    }
                }
            }
        }

        SectionCard {
            implicitHeight: padBody.implicitHeight + 36

            ColumnLayout {
                id: padBody
                anchors.fill: parent
                anchors.margins: 18
                spacing: 12

                Label {
                    text: "停机坪控制"
                    color: theme.textBody
                    font.pixelSize: theme.pageTitleSize(mobile)
                    font.bold: true
                }

                Loader {
                    Layout.fillWidth: true
                    sourceComponent: padDevice ? padControlCard : padFallbackCard
                }
            }
        }
    }

    Component {
        id: padControlCard

        ColumnLayout {
            spacing: 12

            GridLayout {
                Layout.fillWidth: true
                columns: phone ? 1 : 2
                columnSpacing: 12
                rowSpacing: 12

                ActionButton {
                    text: "开启停机坪"
                    enabled: connected && !busy
                    fillColor: "#d9eaee"
                    labelColor: theme.accentCyanDeep
                    onClicked: controller.padOpen()
                }

                ActionButton {
                    text: "关闭停机坪"
                    enabled: connected && !busy
                    fillColor: "#ece4dc"
                    labelColor: "#5a4432"
                    onClicked: controller.padClose()
                }

                ActionButton {
                    text: "停止动作"
                    enabled: connected && !busy
                    fillColor: "#efe2e4"
                    labelColor: "#5b3640"
                    onClicked: controller.padStop()
                }

                ActionButton {
                    text: "刷新停机坪状态"
                    enabled: connected && !busy
                    onClicked: controller.queryPadStatus()
                }
            }

            GridLayout {
                Layout.fillWidth: true
                columns: phone ? 1 : 3
                columnSpacing: 12
                rowSpacing: 12

                MetricTile {
                    title: "左门"
                    value: theme.padStateText(store.padLeftState)
                    caption: "机械门 A"
                }

                MetricTile {
                    title: "右门"
                    value: theme.padStateText(store.padRightState)
                    caption: "机械门 B"
                }

                MetricTile {
                    title: "占用与就绪"
                    value: theme.occupancyText(store.padOccupied)
                    caption: store.padReady ? "允许降落" : "待命"
                }
            }
        }
    }

    Component {
        id: padFallbackCard

        Rectangle {
            Layout.fillWidth: true
            radius: theme.radiusMedium
            color: "#eff4f7"
            border.color: theme.borderSoft
            border.width: 1
            implicitHeight: 112

            Column {
                anchors.fill: parent
                anchors.margins: 14
                spacing: 8

                Label {
                    text: "当前设备未启用停机坪协议"
                    color: theme.textBody
                    font.pixelSize: 16
                    font.bold: true
                }

                Label {
                    width: parent.width
                    text: "控制页仍然保留继电器开关主流程。若设备后续接入 airport_pad_v1，这里会自动切换为停机坪控制面板。"
                    wrapMode: Text.Wrap
                    color: theme.textMuted
                    font.pixelSize: theme.bodySize(root.mobile)
                }
            }
        }
    }
}
