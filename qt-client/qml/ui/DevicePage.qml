import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ScrollView {
    id: root
    property var controller
    readonly property var store: controller.realtimeGateway.stateStore
    readonly property bool mobile: controller.androidMode || width < 1040
    readonly property bool busy: controller.commandPending
    readonly property bool padDevice: store.protocolProfile === "airport_pad_v1"

    clip: true

    TaskTheme { id: theme }

    component ValueCard: Rectangle {
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
                font.pixelSize: 22
                font.bold: true
            }

            Label {
                text: parent.parent.caption
                color: theme.textMuted
                font.pixelSize: 11
                wrapMode: Text.Wrap
            }
        }
    }

    ColumnLayout {
        width: root.availableWidth
        spacing: 14

        Rectangle {
            Layout.fillWidth: true
            radius: theme.radiusLarge
            gradient: Gradient {
                GradientStop { position: 0.0; color: theme.shellTop }
                GradientStop { position: 0.65; color: theme.shellMid }
                GradientStop { position: 1.0; color: "#294257" }
            }
            border.color: theme.borderStrong
            border.width: 1
            implicitHeight: root.mobile ? 212 : 236

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: root.mobile ? 18 : 24
                spacing: 12

                RowLayout {
                    Layout.fillWidth: true

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 4

                        Label {
                            text: "设备详情"
                            color: theme.textPrimary
                            font.pixelSize: root.mobile ? 26 : 34
                            font.bold: true
                        }

                        Label {
                            text: store.currentDeviceId.length > 0 ? store.currentDeviceId : "尚未选中设备"
                            color: "#cbd8e0"
                            font.pixelSize: root.mobile ? 15 : 16
                        }
                    }

                    Rectangle {
                        radius: 18
                        implicitWidth: root.mobile ? 104 : 130
                        implicitHeight: root.mobile ? 38 : 40
                        color: store.online ? theme.success : theme.danger
                        border.color: store.online ? "#a4c8bf" : "#d7b2b7"
                        border.width: 1

                        Label {
                            anchors.centerIn: parent
                            text: store.online ? "在线" : "离线"
                            color: "white"
                            font.pixelSize: 15
                            font.bold: true
                        }
                    }
                }

                Label {
                    width: parent.width
                    text: "详情页同步跟随新的测量能力规则，只展示真实接入的环境数据。"
                    wrapMode: Text.Wrap
                    color: "#cfdae2"
                    font.pixelSize: 13
                }

                GridLayout {
                    Layout.fillWidth: true
                    columns: root.mobile ? 2 : 4
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
                            text: padDevice ? "停机坪协议" : theme.protocolText(store.protocolProfile)
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
                            text: store.padReady ? "允许降落" : "待命"
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
                            text: theme.occupancyText(store.padOccupied)
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
                            text: busy ? "命令处理中" : "状态稳定"
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
            columns: root.mobile ? 1 : 2
            columnSpacing: 14
            rowSpacing: 14

            Rectangle {
                Layout.fillWidth: true
                radius: theme.radiusLarge
                color: "#fbfcfd"
                border.color: theme.borderSoft
                border.width: 1
                implicitHeight: 232

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 18
                    spacing: 12

                    Label {
                        text: "停机坪状态"
                        color: theme.textBody
                        font.pixelSize: 24
                        font.bold: true
                    }

                    GridLayout {
                        Layout.fillWidth: true
                        columns: 2
                        columnSpacing: 12
                        rowSpacing: 12

                        ValueCard {
                            title: "左门"
                            value: theme.padStateText(store.padLeftState)
                            caption: "机械门 A"
                        }

                        ValueCard {
                            title: "右门"
                            value: theme.padStateText(store.padRightState)
                            caption: "机械门 B"
                        }

                        ValueCard {
                            title: "继电器"
                            value: "R1 " + theme.relayStateText(store.relay1On) + " / R2 " + theme.relayStateText(store.relay2On)
                            caption: "控制页与本地 LCD 同步"
                        }

                        ValueCard {
                            title: "模式"
                            value: theme.padModeText(store.padMode)
                            caption: theme.connectionStateText(controller.runtimeConnectionState)
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
                implicitHeight: 232

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 18
                    spacing: 12

                    Label {
                        text: "环境摘要"
                        color: theme.textBody
                        font.pixelSize: 24
                        font.bold: true
                    }

                    Label {
                        width: parent.width
                        text: store.windCapability
                              ? "风速 " + Number(store.windSpeed).toFixed(1) + " m/s · 风向 "
                                + (store.windDirectionText.length > 0 ? store.windDirectionText + " " : "")
                                + Number(store.windDirection).toFixed(0) + "°"
                              : "当前设备未接入风场模块"
                        wrapMode: Text.Wrap
                        color: theme.textMuted
                        font.pixelSize: 13
                    }

                    Label {
                        width: parent.width
                        text: store.airCapability
                              ? "温度 " + Number(store.temperature).toFixed(1) + " °C · 湿度 "
                                + Number(store.humidity).toFixed(0) + "% · PM2.5 "
                                + Number(store.pm25).toFixed(0)
                              : "当前设备未接入空气模块"
                        wrapMode: Text.Wrap
                        color: theme.textMuted
                        font.pixelSize: 13
                    }

                    Label {
                        width: parent.width
                        text: store.rainCapability
                              ? (store.rainDetected ? "已检测到雨滴" : "未检测到雨滴") + " · 湿润度 "
                                + Number(store.rainValue).toFixed(0) + "%"
                              : "当前设备未接入雨滴模块"
                        wrapMode: Text.Wrap
                        color: theme.textMuted
                        font.pixelSize: 13
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        radius: theme.radiusMedium
                        color: "#eff4f7"
                        border.color: theme.borderSoft
                        border.width: 1

                        Column {
                            anchors.fill: parent
                            anchors.margins: 14
                            spacing: 8

                            Label {
                                text: "最新回执"
                                color: theme.textBody
                                font.pixelSize: 16
                                font.bold: true
                            }

                            Label {
                                width: parent.width
                                text: store.lastAckSummary.length > 0 ? store.lastAckSummary : "尚未收到新的任务回执"
                                wrapMode: Text.Wrap
                                color: theme.textBody
                                font.pixelSize: 16
                                font.bold: true
                            }

                            Label {
                                width: parent.width
                                text: store.lastError.length > 0
                                      ? "错误：" + store.lastError
                                      : "未接入测量项已从详情页自动移除。"
                                wrapMode: Text.Wrap
                                color: store.lastError.length > 0 ? theme.danger : theme.textMuted
                                font.pixelSize: 12
                            }
                        }
                    }
                }
            }
        }
    }
}
