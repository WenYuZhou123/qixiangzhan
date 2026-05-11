import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root
    property var controller
    readonly property var store: controller.realtimeGateway.stateStore
    readonly property bool mobile: controller.androidMode || width < 980
    readonly property bool phone: width < theme.breakpointCompact || controller.androidMode

    TaskTheme { id: theme }

    component DeviceCard: Rectangle {
        required property string deviceId
        required property string displayName
        required property bool online
        required property string protocolProfile
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

        function weatherLine() {
            var parts = []
            if (windCapability)
                parts.push("风 " + Number(windSpeed).toFixed(1) + " m/s")
            if (airCapability)
                parts.push("温 " + Number(temperature).toFixed(1) + " C")
            if (airCapability)
                parts.push("PM2.5 " + Number(pm25).toFixed(0))
            if (rainCapability)
                parts.push(rainDetected ? "雨滴" : "无雨")
            return parts.length > 0 ? parts.join(" / ") : "无气象模块"
        }

        width: ListView.view ? ListView.view.width : parent.width
        radius: theme.radiusMedium
        color: selected ? "#edf7f8" : theme.surfacePrimary
        border.color: selected ? theme.accentCyan : theme.borderSoft
        border.width: selected ? 2 : 1
        implicitHeight: 126

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 12
            spacing: 7

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2

                    Label {
                        Layout.fillWidth: true
                        text: displayName.length > 0 ? displayName : deviceId
                        color: theme.textBody
                        font.pixelSize: 16
                        font.bold: true
                        elide: Text.ElideRight
                    }

                    Label {
                        Layout.fillWidth: true
                        text: deviceId
                        color: theme.textMuted
                        font.pixelSize: 11
                        elide: Text.ElideRight
                    }
                }

                StatusPill {
                    textLabel: online ? "在线" : "离线"
                    fill: online ? theme.success : theme.offline
                }
            }

            Label {
                Layout.fillWidth: true
                text: theme.protocolText(protocolProfile) + " / RSSI " + rssi
                color: theme.textMuted
                font.pixelSize: 12
                elide: Text.ElideRight
            }

            Label {
                Layout.fillWidth: true
                text: weatherLine()
                color: theme.textBody
                font.pixelSize: 13
                wrapMode: Text.Wrap
            }

            StatusPill {
                visible: alarmCount > 0
                textLabel: "告警 " + alarmCount
                fill: theme.danger
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
                width: root.mobile ? root.width : root.width - 332 - theme.sectionGap
                spacing: theme.sectionGap

                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: root.mobile ? 154 : 166
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
                                    text: "现场总览"
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
                                textLabel: store.online ? "设备在线" : "设备离线"
                                fill: store.online ? theme.success : theme.offline
                            }

                            StatusPill {
                                textLabel: root.controller.remoteSyncService.systemHealthStatus.toUpperCase()
                                fill: theme.statusColor(root.controller.remoteSyncService.systemHealthStatus)
                            }
                        }

                        GridLayout {
                            Layout.fillWidth: true
                            columns: root.phone ? 2 : 4
                            columnSpacing: 8
                            rowSpacing: 8

                            StatusPill {
                                Layout.fillWidth: true
                                textLabel: root.controller.remoteSyncService.healthMqttConnected ? "MQTT UP" : "MQTT DOWN"
                                fill: root.controller.remoteSyncService.healthMqttConnected ? theme.success : theme.danger
                            }

                            StatusPill {
                                Layout.fillWidth: true
                                textLabel: "设备 " + root.controller.remoteSyncService.healthDeviceSummary
                                fill: theme.pending
                            }

                            StatusPill {
                                Layout.fillWidth: true
                                textLabel: "磁盘 " + root.controller.remoteSyncService.healthDiskSummary
                                fill: root.controller.remoteSyncService.healthDiskFreePercent < 10 ? theme.danger : theme.success
                            }

                            StatusPill {
                                Layout.fillWidth: true
                                textLabel: store.activeAlarmCount > 0 ? "告警 " + store.activeAlarmCount : "无活动告警"
                                fill: store.activeAlarmCount > 0 ? theme.danger : theme.success
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8

                            OpsButton {
                                Layout.preferredWidth: 120
                                text: "控制"
                                fillColor: "#dcecef"
                                labelColor: theme.accentCyanDeep
                                onClicked: root.controller.currentPage = "pad"
                            }

                            OpsButton {
                                Layout.preferredWidth: 120
                                text: "气象"
                                onClicked: root.controller.currentPage = "weather"
                            }

                            OpsButton {
                                Layout.preferredWidth: 120
                                text: "告警"
                                fillColor: store.activeAlarmCount > 0 ? "#f7dfe2" : theme.surfaceSecondary
                                labelColor: store.activeAlarmCount > 0 ? theme.danger : theme.textBody
                                onClicked: root.controller.currentPage = "alarms"
                            }

                            Item { Layout.fillWidth: true }
                        }
                    }
                }

                GridLayout {
                    Layout.fillWidth: true
                    columns: root.phone ? 2 : 4
                    columnSpacing: 12
                    rowSpacing: 12

                    MetricTile {
                        mobile: root.mobile
                        title: "最近数据"
                        value: store.timestamp.length > 0 ? store.timestamp : "--"
                        caption: "设备状态更新时间"
                        accent: theme.pending
                    }

                    MetricTile {
                        mobile: root.mobile
                        title: "最后 ACK"
                        value: store.lastAckSummary.length > 0 ? store.lastAckSummary : "等待回执"
                        caption: store.lastError.length > 0 ? store.lastError : "命令链路空闲"
                        accent: theme.statusColor(store.lastAckResult)
                    }

                    MetricTile {
                        mobile: root.mobile
                        visible: store.windCapability
                        title: "风速"
                        value: Number(store.windSpeed).toFixed(1)
                        unit: "m/s"
                        caption: "风向 " + (store.windDirectionText.length > 0 ? store.windDirectionText + " " : "") + Number(store.windDirection).toFixed(0) + " deg"
                        fillRatio: Math.min(1, store.windSpeed / 20.0)
                    }

                    MetricTile {
                        mobile: root.mobile
                        visible: store.airCapability
                        title: "温湿度"
                        value: Number(store.temperature).toFixed(1) + " C"
                        caption: "湿度 " + Number(store.humidity).toFixed(0) + "%"
                        fillRatio: Math.min(1, store.humidity / 100.0)
                        accent: theme.success
                    }

                    MetricTile {
                        mobile: root.mobile
                        visible: store.airCapability
                        title: "空气质量"
                        value: "PM2.5 " + Number(store.pm25).toFixed(0)
                        caption: "PM10 " + Number(store.pm10).toFixed(0) + " / CO2 " + Number(store.co2).toFixed(0) + " ppm"
                        fillRatio: Math.min(1, store.pm25 / 150.0)
                        accent: store.pm25 > 75 ? theme.warning : theme.success
                    }

                    MetricTile {
                        mobile: root.mobile
                        visible: store.rainCapability
                        title: "雨滴"
                        value: store.rainDetected ? "已检测" : "未检测"
                        caption: "湿润度 " + Number(store.rainValue).toFixed(0) + "%"
                        fillRatio: Math.min(1, store.rainValue / 100.0)
                        accent: store.rainDetected ? theme.warning : theme.accentCyanDeep
                    }

                    MetricTile {
                        mobile: root.mobile
                        title: "停机坪"
                        value: store.padReady ? "允许降落" : "待命"
                        caption: theme.occupancyText(store.padOccupied) + " / " + theme.padModeText(store.padMode)
                        accent: theme.readinessColor(store.padReady)
                    }

                    MetricTile {
                        mobile: root.mobile
                        title: "传感器"
                        value: store.sensorFailureCount > 0 ? "异常 " + store.sensorFailureCount : "正常"
                        caption: store.sensorLastError.length > 0 ? store.sensorLastError : "风/雨/空气状态可用"
                        accent: store.sensorFailureCount > 0 ? theme.danger : theme.success
                    }
                }

                Rectangle {
                    visible: root.mobile
                    Layout.fillWidth: true
                    radius: theme.radiusLarge
                    color: theme.surfacePrimary
                    border.color: theme.borderSoft
                    border.width: 1
                    implicitHeight: deviceColumn.implicitHeight + 24

                    ColumnLayout {
                        id: deviceColumn
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.margins: 12
                        spacing: 10

                        Label {
                            Layout.fillWidth: true
                            text: "设备切换"
                            color: theme.textBody
                            font.pixelSize: theme.pageTitleSize(root.mobile)
                            font.bold: true
                        }

                        Repeater {
                            model: root.controller.deviceRepository

                            delegate: DeviceCard {
                                Layout.fillWidth: true
                                deviceId: model.deviceId
                                displayName: model.displayName
                                online: model.online
                                protocolProfile: model.protocolProfile
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
            Layout.preferredWidth: 332
            Layout.fillHeight: true
            radius: theme.radiusLarge
            color: theme.surfacePrimary
            border.color: theme.borderSoft
            border.width: 1

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 10

                Label {
                    Layout.fillWidth: true
                    text: "设备矩阵"
                    color: theme.textBody
                    font.pixelSize: theme.pageTitleSize(false)
                    font.bold: true
                }

                Label {
                    Layout.fillWidth: true
                    text: "1-3 台现场设备快速切换，在线和告警优先看。"
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
