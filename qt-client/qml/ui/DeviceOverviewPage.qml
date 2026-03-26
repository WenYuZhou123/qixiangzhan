import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root
    property var controller
    readonly property var store: controller.realtimeGateway.stateStore
    readonly property bool mobile: controller.androidMode || width < 1180
    readonly property bool phone: width < theme.breakpointCompact || controller.androidMode

    TaskTheme { id: theme }

    component MetricCard: Rectangle {
        required property string title
        required property string value
        required property string caption

        Layout.fillWidth: true
        radius: theme.radiusMedium
        color: theme.surfacePrimary
        border.color: theme.borderSoft
        border.width: 1
        implicitHeight: 98

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
                font.pixelSize: root.mobile ? 22 : 24
                font.bold: true
                elide: Text.ElideRight
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
        implicitWidth: pillLabel.implicitWidth + 28
        radius: 17
        color: fill
        border.color: "#dbe5ea"
        border.width: 1

        Label {
            id: pillLabel
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
        required property real windSpeed
        required property real temperature
        required property real pm25
        required property bool rainDetected
        required property bool windCapability
        required property bool airCapability
        required property bool rainCapability
        required property bool selected

        function summaryLine() {
            var parts = ["RSSI " + rssi]
            if (windCapability)
                parts.push("风速 " + Number(windSpeed).toFixed(1) + " m/s")
            if (airCapability)
                parts.push("温度 " + Number(temperature).toFixed(1) + " °C")
            if (rainCapability)
                parts.push(rainDetected ? "雨滴已检测" : "无雨滴")
            return parts.join(" · ")
        }

        function detailLine() {
            var parts = [theme.protocolText(protocolProfile)]
            parts.push("左门 " + theme.padStateText(padLeftState))
            parts.push("右门 " + theme.padStateText(padRightState))
            parts.push(padReady ? "允许降落" : "待命")
            parts.push(theme.occupancyText(padOccupied))
            if (airCapability)
                parts.push("PM2.5 " + Number(pm25).toFixed(0))
            if (alarmCount > 0)
                parts.push("告警 " + alarmCount)
            return parts.join(" · ")
        }

        width: ListView.view ? ListView.view.width : 320
        radius: 24
        color: selected ? theme.glassStrong : "#f8fbfc"
        border.color: selected ? theme.accentCyan : theme.borderSoft
        border.width: selected ? 2 : 1
        implicitHeight: 150

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

            Label {
                width: parent.width
                text: parent.parent.summaryLine()
                color: theme.textBody
                font.pixelSize: 13
                wrapMode: Text.Wrap
            }

            Label {
                width: parent.width
                text: parent.parent.detailLine()
                color: theme.textMuted
                font.pixelSize: 12
                wrapMode: Text.Wrap
            }
        }

        MouseArea {
            anchors.fill: parent
            onClicked: root.controller.deviceRepository.selectDevice(deviceId)
        }
    }

    RowLayout {
        anchors.fill: parent
        spacing: theme.sectionGap

        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true

            ColumnLayout {
                width: root.width - (!root.mobile ? 340 + theme.sectionGap : 0)
                spacing: theme.sectionGap

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
                    implicitHeight: root.mobile ? 252 : 282

                    Rectangle {
                        width: parent.width * 0.36
                        height: parent.height * 1.04
                        x: parent.width * 0.6
                        y: -parent.height * 0.2
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
                                text: "统一概览"
                                color: theme.textPrimary
                                font.pixelSize: theme.heroTitleSize(root.mobile)
                                font.bold: true
                            }

                            Label {
                                width: parent.width
                                text: "主页面只保留当前板卡真实接入的数据，概览、控制、气象和本地 LCD 保持同一套状态表达。"
                                wrapMode: Text.Wrap
                                color: "#cfdae2"
                                font.pixelSize: theme.bodySize(root.mobile)
                            }

                            RowLayout {
                                spacing: 10

                                StatePill {
                                    textLabel: store.online ? "设备在线" : "设备离线"
                                    fill: store.online ? theme.success : theme.danger
                                }

                                StatePill {
                                    textLabel: store.padReady ? "允许降落" : "停机坪待命"
                                    fill: store.padReady ? theme.success : theme.warning
                                }

                                StatePill {
                                    visible: store.rainCapability
                                    textLabel: store.rainDetected ? "雨滴已检测" : "无雨滴"
                                    fill: store.rainDetected ? theme.warning : theme.accentCyanDeep
                                }
                            }

                            RowLayout {
                                spacing: 10

                                Button {
                                    text: "进入控制"
                                    Layout.preferredWidth: 142
                                    Layout.preferredHeight: theme.touchTarget
                                    onClicked: root.controller.currentPage = "pad"
                                }

                                Button {
                                    text: "查看气象"
                                    Layout.preferredWidth: 142
                                    Layout.preferredHeight: theme.touchTarget
                                    onClicked: root.controller.currentPage = "weather"
                                }
                            }
                        }

                        Item {
                            visible: !root.mobile
                            Layout.preferredWidth: 300
                            Layout.fillHeight: true

                            Rectangle {
                                anchors.fill: parent
                                radius: 28
                                color: "#20ffffff"
                                border.color: "#49667a"
                                border.width: 1
                            }

                            Column {
                                anchors.centerIn: parent
                                spacing: 10

                                Rectangle {
                                    width: 182
                                    height: 182
                                    radius: 91
                                    color: "#16384b"
                                    border.color: theme.accentCyan
                                    border.width: 1
                                }

                                Label {
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    text: "概览、控制、气象保持同一语义"
                                    color: theme.textPrimary
                                    font.pixelSize: 14
                                }
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
                    implicitHeight: 78

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
                                text: "活动告警 " + store.activeAlarmCount + " 条，建议优先检查链路、继电器和传感器状态。"
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
                    columns: root.phone ? 2 : 4
                    columnSpacing: 12
                    rowSpacing: 12

                    MetricCard {
                        title: "当前设备"
                        value: store.currentDeviceId.length > 0 ? store.currentDeviceId : "未选择"
                        caption: theme.protocolText(store.protocolProfile)
                    }

                    MetricCard {
                        title: "继电器"
                        value: "R1 " + theme.relayStateText(store.relay1On) + " / R2 " + theme.relayStateText(store.relay2On)
                        caption: "控制页与 LCD 状态一致"
                    }

                    MetricCard {
                        visible: store.windCapability
                        title: "风速"
                        value: Number(store.windSpeed).toFixed(1) + " m/s"
                        caption: "风向 " + (store.windDirectionText.length > 0
                                              ? store.windDirectionText + " "
                                              : "") + Number(store.windDirection).toFixed(0) + "°"
                    }

                    MetricCard {
                        visible: store.airCapability
                        title: "温湿度"
                        value: Number(store.temperature).toFixed(1) + " °C"
                        caption: "湿度 " + Number(store.humidity).toFixed(0) + "%"
                    }

                    MetricCard {
                        title: "停机坪"
                        value: store.padReady ? "允许降落" : "待命"
                        caption: theme.occupancyText(store.padOccupied) + " · " + theme.padModeText(store.padMode)
                    }

                    MetricCard {
                        visible: store.airCapability
                        title: "空气质量"
                        value: "PM2.5 " + Number(store.pm25).toFixed(0)
                        caption: "PM10 " + Number(store.pm10).toFixed(0) + " ug/m3"
                    }

                    MetricCard {
                        visible: store.rainCapability
                        title: "雨滴"
                        value: store.rainDetected ? "已检测" : "未检测"
                        caption: "湿润度 " + Number(store.rainValue).toFixed(0) + "%"
                    }

                    MetricCard {
                        title: "最近回执"
                        value: store.lastAckSummary.length > 0 ? store.lastAckSummary : "等待回执"
                        caption: store.lastError.length > 0 ? store.lastError : "控制链路正常"
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
                            font.pixelSize: theme.pageTitleSize(true)
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
                                windSpeed: model.windSpeed
                                temperature: model.temperature
                                pm25: model.pm25
                                rainDetected: model.rainDetected
                                windCapability: model.windCapability
                                airCapability: model.airCapability
                                rainCapability: model.rainCapability
                                selected: model.selected
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
                    font.pixelSize: theme.pageTitleSize(false)
                    font.bold: true
                }

                Label {
                    text: "在线设备优先排列。这里只展示真实测量摘要，未接入项会自动隐藏。"
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
                        windSpeed: model.windSpeed
                        temperature: model.temperature
                        pm25: model.pm25
                        rainDetected: model.rainDetected
                        windCapability: model.windCapability
                        airCapability: model.airCapability
                        rainCapability: model.rainCapability
                        selected: model.selected
                    }
                }
            }
        }
    }
}
