import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root
    property var controller
    readonly property var store: controller.realtimeGateway.stateStore
    readonly property bool mobile: controller.androidMode || width < 1180

    TaskTheme { id: theme }

    component MetricCard: Rectangle {
        required property string title
        required property string value
        required property string caption
        Layout.fillWidth: true
        radius: theme.radiusMedium
        color: theme.glassSoft
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
                font.pixelSize: 12
            }

            Label {
                text: parent.parent.value
                color: theme.textBody
                font.pixelSize: root.mobile ? 22 : 24
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

    component StatePill: Rectangle {
        required property string textLabel
        required property color fill
        implicitHeight: 34
        implicitWidth: stateLabel.implicitWidth + 28
        radius: 17
        color: fill
        border.color: "#dbe5ea"
        border.width: 1

        Label {
            id: stateLabel
            anchors.centerIn: parent
            text: parent.textLabel
            color: "white"
            font.pixelSize: 13
            font.bold: true
        }
    }

    component DeviceCard: Rectangle {
        required property string deviceId
        required property string displayName
        required property bool online
        required property string protocolProfile
        required property string padLeftState
        required property string padRightState
        required property bool padReady
        required property bool padOccupied
        required property int rssi
        required property int alarmCount
        required property bool selected

        Layout.fillWidth: true
        radius: 24
        color: selected ? theme.glassStrong : "#f8fbfc"
        border.color: selected ? theme.accentCyan : theme.borderSoft
        border.width: selected ? 2 : 1
        implicitHeight: 138

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 14
            spacing: 8

            RowLayout {
                Layout.fillWidth: true

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2

                    Label {
                        text: displayName
                        color: theme.textBody
                        font.pixelSize: 18
                        font.bold: true
                    }

                    Label {
                        text: deviceId
                        color: theme.textMuted
                        font.pixelSize: 12
                    }
                }

                StatePill {
                    textLabel: online ? "在线" : "离线"
                    fill: online ? theme.success : theme.danger
                }
            }

            RowLayout {
                spacing: 8

                Rectangle {
                    radius: 14
                    color: theme.pageSurface
                    border.color: theme.borderSoft
                    border.width: 1
                    implicitWidth: 92
                    implicitHeight: 30

                    Label {
                        anchors.centerIn: parent
                        text: protocolProfile === "airport_pad_v1" ? "停机场" : "工程设备"
                        color: theme.textBody
                        font.pixelSize: 12
                        font.bold: true
                    }
                }

                Rectangle {
                    radius: 14
                    color: theme.pageSurface
                    border.color: theme.borderSoft
                    border.width: 1
                    implicitWidth: 160
                    implicitHeight: 30

                    Label {
                        anchors.centerIn: parent
                        text: "左门 " + theme.padStateText(padLeftState) + " / 右门 " + theme.padStateText(padRightState)
                        color: theme.textBody
                        font.pixelSize: 12
                    }
                }
            }

            Label {
                width: parent.width
                text: "RSSI " + rssi + "  ·  " + (padReady ? "允许降落" : "待命") + "  ·  "
                      + theme.occupancyText(padOccupied) + "  ·  告警 " + alarmCount
                color: theme.textMuted
                font.pixelSize: 12
                elide: Text.ElideRight
            }
        }

        MouseArea {
            anchors.fill: parent
            onClicked: root.controller.deviceRepository.selectDevice(deviceId)
        }
    }

    RowLayout {
        anchors.fill: parent
        spacing: 14

        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true

            ColumnLayout {
                width: root.mobile ? root.width - 8 : parent.width
                spacing: 14

                Rectangle {
                    Layout.fillWidth: true
                    radius: theme.radiusLarge
                    gradient: Gradient {
                        GradientStop { position: 0.0; color: theme.shellTop }
                        GradientStop { position: 0.55; color: theme.shellMid }
                        GradientStop { position: 1.0; color: "#294257" }
                    }
                    border.color: theme.borderStrong
                    border.width: 1
                    implicitHeight: root.mobile ? 248 : 286

                    Rectangle {
                        width: parent.width * 0.42
                        height: parent.height * 1.1
                        x: parent.width * 0.54
                        y: -parent.height * 0.22
                        radius: width / 2
                        color: "#24d7e5ed"
                    }

                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: root.mobile ? 18 : 24
                        spacing: root.mobile ? 0 : 18

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 10

                            Label {
                                text: "任务总览"
                                color: theme.textPrimary
                                font.pixelSize: root.mobile ? 28 : 38
                                font.bold: true
                            }

                            Label {
                                width: parent.width
                                text: "停机场开合、机场核心气象和无人机任务会在同一任务框架内协同工作。当前阶段先固定停机场控制与气象摘要。"
                                wrapMode: Text.Wrap
                                color: "#cfdae2"
                                font.pixelSize: root.mobile ? 14 : 15
                            }

                            RowLayout {
                                spacing: 10

                                StatePill {
                                    textLabel: theme.flightRuleText(store.windSpeed, store.visibility)
                                    fill: theme.flightRuleColor(store.windSpeed, store.visibility)
                                }

                                StatePill {
                                    textLabel: store.padReady ? "允许降落" : "停机场待命"
                                    fill: store.padReady ? theme.success : theme.warning
                                }

                                StatePill {
                                    textLabel: store.online ? "设备在线" : "设备离线"
                                    fill: store.online ? theme.success : theme.danger
                                }
                            }

                            Button {
                                text: "查询当前设备"
                                Layout.preferredWidth: root.mobile ? 176 : 196
                                enabled: root.controller.commandConnected && !root.controller.commandPending
                                onClicked: root.controller.queryPadStatus()

                                contentItem: Text {
                                    text: parent.text
                                    color: theme.textBody
                                    font: parent.font
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }

                                background: Rectangle {
                                    radius: 22
                                    color: parent.enabled ? theme.glassStrong : theme.neutral
                                    border.color: theme.borderSoft
                                    border.width: 1
                                }
                            }
                        }

                        Item {
                            visible: !root.mobile
                            Layout.preferredWidth: 320
                            Layout.fillHeight: true

                            Rectangle {
                                anchors.fill: parent
                                radius: 28
                                color: "#22ffffff"
                                border.color: "#49667a"
                                border.width: 1
                            }

                            Rectangle {
                                width: 212
                                height: 212
                                anchors.centerIn: parent
                                radius: 106
                                color: "#16384b"
                                border.color: theme.accentCyan
                                border.width: 1
                            }

                            Rectangle {
                                width: 126
                                height: 12
                                radius: 6
                                anchors.horizontalCenter: parent.horizontalCenter
                                anchors.verticalCenter: parent.verticalCenter
                                anchors.verticalCenterOffset: -32
                                color: "#dcebf1"
                            }

                            Rectangle {
                                width: 126
                                height: 12
                                radius: 6
                                anchors.horizontalCenter: parent.horizontalCenter
                                anchors.verticalCenter: parent.verticalCenter
                                anchors.verticalCenterOffset: 32
                                color: "#dcebf1"
                            }

                            Rectangle {
                                width: 116
                                height: 32
                                radius: 16
                                anchors.centerIn: parent
                                color: theme.accentCyanDeep
                                border.color: "#9bc9d7"
                                border.width: 1
                            }

                            Label {
                                anchors.horizontalCenter: parent.horizontalCenter
                                anchors.bottom: parent.bottom
                                anchors.bottomMargin: 22
                                text: "无人机任务主视觉"
                                color: theme.textPrimary
                                font.pixelSize: 14
                            }
                        }
                    }
                }

                Rectangle {
                    visible: store.activeAlarmCount > 0
                    Layout.fillWidth: true
                    radius: theme.radiusMedium
                    color: "#f7eeef"
                    border.color: "#d9b8bc"
                    border.width: 1
                    implicitHeight: 74

                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 14
                        spacing: 12

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2

                            Label {
                                text: "当前存在活动告警"
                                color: theme.textBody
                                font.pixelSize: 18
                                font.bold: true
                            }

                            Label {
                                text: "活动告警 " + store.activeAlarmCount + " 条，建议优先检查停机场状态与链路健康。"
                                color: theme.textMuted
                                font.pixelSize: 12
                            }
                        }

                        Button {
                            text: "查看告警"
                            onClicked: root.controller.currentPage = "alarms"
                        }
                    }
                }

                GridLayout {
                    Layout.fillWidth: true
                    columns: root.mobile ? 2 : 4
                    columnSpacing: 12
                    rowSpacing: 12

                    MetricCard {
                        title: "风速"
                        value: Number(store.windSpeed).toFixed(1) + " m/s"
                        caption: "风向 " + Number(store.windDirection).toFixed(0) + "°"
                    }

                    MetricCard {
                        title: "温度"
                        value: Number(store.temperature).toFixed(1) + " °C"
                        caption: "湿度 " + Number(store.humidity).toFixed(0) + " %"
                    }

                    MetricCard {
                        title: "气压"
                        value: Number(store.pressure).toFixed(1) + " hPa"
                        caption: "能见度 " + Number(store.visibility).toFixed(1) + " km"
                    }

                    MetricCard {
                        title: "停机场"
                        value: store.padReady ? "允许降落" : "待命"
                        caption: theme.occupancyText(store.padOccupied) + " · " + theme.padModeText(store.padMode)
                    }
                }

                Rectangle {
                    visible: root.mobile
                    Layout.fillWidth: true
                    radius: theme.radiusLarge
                    color: "#fbfcfd"
                    border.color: theme.borderSoft
                    border.width: 1

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 16
                        spacing: 10

                        Label {
                            text: "设备切换"
                            color: theme.textBody
                            font.pixelSize: 22
                            font.bold: true
                        }

                        Repeater {
                            model: root.controller.deviceRepository

                            delegate: DeviceCard {
                                deviceId: model.deviceId
                                displayName: model.displayName
                                online: model.online
                                protocolProfile: model.protocolProfile
                                padLeftState: model.padLeftState
                                padRightState: model.padRightState
                                padReady: model.padReady
                                padOccupied: model.padOccupied
                                rssi: model.rssi
                                alarmCount: model.alarmCount
                                selected: model.selected
                            }
                        }
                    }
                }

                GridLayout {
                    Layout.fillWidth: true
                    columns: root.mobile ? 1 : 2
                    columnSpacing: 14
                    rowSpacing: 14

                    Rectangle {
                        Layout.fillWidth: true
                        radius: theme.radiusLarge
                        color: "#fbfcfd"
                        border.color: theme.borderSoft
                        border.width: 1
                        implicitHeight: 220

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 18
                            spacing: 10

                            Label {
                                text: "停机场摘要"
                                color: theme.textBody
                                font.pixelSize: 24
                                font.bold: true
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 10

                                MetricCard {
                                    title: "左舱门"
                                    value: theme.padStateText(store.padLeftState)
                                    caption: "机械舱门 A"
                                }

                                MetricCard {
                                    title: "右舱门"
                                    value: theme.padStateText(store.padRightState)
                                    caption: "机械舱门 B"
                                }
                            }

                            Label {
                                width: parent.width
                                text: "最近回执：" + (store.lastAckSummary.length > 0 ? store.lastAckSummary : "等待新的任务命令")
                                wrapMode: Text.Wrap
                                color: theme.textMuted
                                font.pixelSize: 13
                            }

                            RowLayout {
                                spacing: 10

                                Button {
                                    text: "进入停机场控制"
                                    onClicked: root.controller.currentPage = "pad"
                                }

                                Button {
                                    text: "查看历史"
                                    onClicked: root.controller.currentPage = "history"
                                }
                            }
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        radius: theme.radiusLarge
                        color: "#fbfcfd"
                        border.color: theme.borderSoft
                        border.width: 1
                        implicitHeight: 220

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 18
                            spacing: 10

                            Label {
                                text: "任务扩展预留"
                                color: theme.textBody
                                font.pixelSize: 24
                                font.bold: true
                            }

                            Label {
                                width: parent.width
                                text: "无人机起飞、降落、自动任务调度和航线地图会在后续阶段接入。当前阶段先固定停机场与机场核心气象链路。"
                                wrapMode: Text.Wrap
                                color: theme.textMuted
                                font.pixelSize: 13
                            }

                            Rectangle {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                radius: 22
                                color: "#eff4f7"
                                border.color: theme.borderSoft
                                border.width: 1

                                Column {
                                    anchors.centerIn: parent
                                    spacing: 8

                                    Label {
                                        anchors.horizontalCenter: parent.horizontalCenter
                                        text: "无人机任务与航线"
                                        color: theme.accentCyanDeep
                                        font.pixelSize: 16
                                        font.bold: true
                                    }

                                    Label {
                                        anchors.horizontalCenter: parent.horizontalCenter
                                        text: "即将接入"
                                        color: theme.textMuted
                                        font.pixelSize: 13
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        Rectangle {
            visible: !root.mobile
            Layout.preferredWidth: 340
            Layout.fillHeight: true
            radius: theme.radiusLarge
            color: "#fbfcfd"
            border.color: theme.borderSoft
            border.width: 1

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 16
                spacing: 12

                Label {
                    text: "设备矩阵"
                    color: theme.textBody
                    font.pixelSize: 24
                    font.bold: true
                }

                Label {
                    text: "在线设备优先排列。停机场新协议设备显示正式任务状态，旧继电器设备仅保留工程测试属性。"
                    color: theme.textMuted
                    font.pixelSize: 12
                    wrapMode: Text.Wrap
                }

                ListView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    spacing: 10
                    clip: true
                    model: root.controller.deviceRepository
                    ScrollBar.vertical: ScrollBar {}

                    delegate: DeviceCard {
                        deviceId: model.deviceId
                        displayName: model.displayName
                        online: model.online
                        protocolProfile: model.protocolProfile
                        padLeftState: model.padLeftState
                        padRightState: model.padRightState
                        padReady: model.padReady
                        padOccupied: model.padOccupied
                        rssi: model.rssi
                        alarmCount: model.alarmCount
                        selected: model.selected
                    }
                }
            }
        }
    }
}
