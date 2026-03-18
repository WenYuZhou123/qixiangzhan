import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ScrollView {
    id: root
    property var controller
    readonly property var store: controller.realtimeGateway.stateStore
    readonly property bool mobile: controller.androidMode || width < 1080
    readonly property bool busy: controller.commandPending

    clip: true

    TaskTheme { id: theme }

    component ActionButton: Button {
        property color fillColor: theme.glassStrong
        property color labelColor: theme.textBody
        Layout.fillWidth: true
        implicitHeight: 48

        contentItem: Text {
            text: parent.text
            color: parent.enabled ? parent.labelColor : "#70808b"
            font.pixelSize: 15
            font.bold: true
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }

        background: Rectangle {
            radius: 20
            color: parent.enabled ? parent.fillColor : theme.neutral
            border.color: theme.borderSoft
            border.width: 1
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
                GradientStop { position: 0.6; color: theme.shellMid }
                GradientStop { position: 1.0; color: "#294257" }
            }
            border.color: theme.borderStrong
            border.width: 1
            implicitHeight: 190

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 20
                spacing: 10

                Label {
                    text: "工程模式"
                    color: theme.textPrimary
                    font.pixelSize: 32
                    font.bold: true
                }

                Label {
                    width: parent.width
                    text: "保留继电器、本地 MQTT 与串口联调用于旧设备兼容和底层调试，不进入普通远程业务主链路。"
                    wrapMode: Text.Wrap
                    color: "#cfdbe2"
                    font.pixelSize: 14
                }

                RowLayout {
                    spacing: 10

                    Rectangle {
                        radius: 16
                        implicitWidth: 126
                        implicitHeight: 34
                        color: controller.runtimeConnected ? theme.success : theme.danger
                        border.color: "#d9e5ea"
                        border.width: 1

                        Label {
                            anchors.centerIn: parent
                            text: controller.runtimeConnectionState
                            color: "white"
                            font.pixelSize: 13
                            font.bold: true
                        }
                    }

                    Rectangle {
                        radius: 16
                        implicitWidth: 140
                        implicitHeight: 34
                        color: "#12000000"
                        border.color: theme.borderStrong
                        border.width: 1

                        Label {
                            anchors.centerIn: parent
                            text: "当前设备 " + store.currentDeviceId
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
                implicitHeight: 260

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 18
                    spacing: 12

                    Label {
                        text: "继电器调试"
                        color: theme.textBody
                        font.pixelSize: 24
                        font.bold: true
                    }

                    GridLayout {
                        Layout.fillWidth: true
                        columns: 2
                        columnSpacing: 12
                        rowSpacing: 12

                        ActionButton {
                            text: "R1 开启"
                            enabled: controller.commandConnected && !root.busy
                            fillColor: "#d9eaee"
                            labelColor: theme.accentCyanDeep
                            onClicked: controller.setRelay1(true)
                        }
                        ActionButton {
                            text: "R1 关闭"
                            enabled: controller.commandConnected && !root.busy
                            onClicked: controller.setRelay1(false)
                        }
                        ActionButton {
                            text: "R2 开启"
                            enabled: controller.commandConnected && !root.busy
                            fillColor: "#d9eaee"
                            labelColor: theme.accentCyanDeep
                            onClicked: controller.setRelay2(true)
                        }
                        ActionButton {
                            text: "R2 关闭"
                            enabled: controller.commandConnected && !root.busy
                            onClicked: controller.setRelay2(false)
                        }
                        ActionButton {
                            text: "全部开启"
                            enabled: controller.commandConnected && !root.busy
                            fillColor: "#ece4dc"
                            labelColor: "#5a4432"
                            onClicked: controller.setAll(true)
                        }
                        ActionButton {
                            text: "全部关闭"
                            enabled: controller.commandConnected && !root.busy
                            fillColor: "#ece4dc"
                            labelColor: "#5a4432"
                            onClicked: controller.setAll(false)
                        }
                    }

                    ActionButton {
                        text: "查询继电器状态"
                        enabled: controller.commandConnected && !root.busy
                        onClicked: controller.queryStatus()
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                radius: theme.radiusLarge
                color: "#fbfcfd"
                border.color: theme.borderSoft
                border.width: 1
                implicitHeight: 260

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 18
                    spacing: 12

                    Label {
                        text: "本地联调"
                        color: theme.textBody
                        font.pixelSize: 24
                        font.bold: true
                    }

                    Label {
                        width: parent.width
                        text: "串口调试、本地 MQTT 与历史日志都集中在工程模式下，避免影响普通远程用户视图。"
                        wrapMode: Text.Wrap
                        color: theme.textMuted
                        font.pixelSize: 13
                    }

                    ActionButton {
                        text: "打开串口调试"
                        onClicked: controller.currentPage = "serial"
                    }

                    ActionButton {
                        text: "查看运行日志"
                        onClicked: controller.currentPage = "logs"
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
                                text: "当前状态"
                                color: theme.textBody
                                font.pixelSize: 16
                                font.bold: true
                            }

                            Label {
                                text: "R1 " + (store.relay1On ? "ON" : "OFF") + "  ·  R2 " + (store.relay2On ? "ON" : "OFF")
                                color: theme.textMuted
                                font.pixelSize: 13
                            }

                            Label {
                                text: "最新回执： " + (store.lastAckSummary.length > 0 ? store.lastAckSummary : "等待中")
                                color: theme.textMuted
                                font.pixelSize: 13
                                wrapMode: Text.Wrap
                            }
                        }
                    }
                }
            }
        }
    }
}
