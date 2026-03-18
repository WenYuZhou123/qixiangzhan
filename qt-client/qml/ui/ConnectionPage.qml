import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Pane {
    id: root
    property var gateway

    padding: 0
    background: Rectangle {
        radius: 30
        color: "#ffffff"
        border.width: 1
        border.color: "#d2dde3"
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 18
        spacing: 14

        Rectangle {
            Layout.fillWidth: true
            radius: 26
            implicitHeight: 154
            gradient: Gradient {
                GradientStop { position: 0.0; color: "#102232" }
                GradientStop { position: 0.58; color: "#193a50" }
                GradientStop { position: 1.0; color: "#406278" }
            }
            border.color: "#7495aa"
            border.width: 1

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 20
                spacing: 10

                RowLayout {
                    Layout.fillWidth: true

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 3

                        Label {
                            text: "链路配置"
                            color: "#f5efe4"
                            font.pixelSize: 30
                            font.bold: true
                        }

                        Label {
                            text: "MQTT Uplink / Engineering Gateway"
                            color: "#d0deea"
                            font.pixelSize: 13
                        }
                    }

                    Rectangle {
                        radius: 18
                        implicitWidth: 170
                        implicitHeight: 38
                        color: "#10ffffff"
                        border.color: "#89aec8"
                        border.width: 1

                        Label {
                            anchors.centerIn: parent
                            text: root.gateway.connectionState
                            color: "#eef5f8"
                            font.pixelSize: 14
                            font.bold: true
                        }
                    }
                }

                Label {
                    text: "仅桌面工程模式显示。这里保留直连 MQTT 与现场联调能力，用于实验室快速排障。"
                    color: "#e4edf4"
                    font.pixelSize: 13
                    wrapMode: Text.Wrap
                }
            }
        }

        GridLayout {
            Layout.fillWidth: true
            columns: 2
            columnSpacing: 16
            rowSpacing: 16

            Rectangle {
                Layout.fillWidth: true
                radius: 24
                color: "#f7fafc"
                border.color: "#d2dde3"
                border.width: 1

                GridLayout {
                    anchors.fill: parent
                    anchors.margins: 18
                    columns: 2
                    columnSpacing: 12
                    rowSpacing: 10

                    Label { text: "Host"; color: "#6f808c" }
                    TextField {
                        Layout.fillWidth: true
                        text: root.gateway.brokerHost
                        onEditingFinished: root.gateway.brokerHost = text
                    }

                    Label { text: "Port"; color: "#6f808c" }
                    SpinBox {
                        Layout.fillWidth: true
                        from: 1
                        to: 65535
                        value: root.gateway.brokerPort
                        editable: true
                        onValueModified: root.gateway.brokerPort = value
                    }

                    Label { text: "用户名"; color: "#6f808c" }
                    TextField {
                        Layout.fillWidth: true
                        text: root.gateway.username
                        onEditingFinished: root.gateway.username = text
                    }

                    Label { text: "密码"; color: "#6f808c" }
                    TextField {
                        Layout.fillWidth: true
                        text: root.gateway.password
                        echoMode: TextInput.Password
                        onEditingFinished: root.gateway.password = text
                    }

                    Label { text: "当前设备"; color: "#6f808c" }
                    TextField {
                        Layout.fillWidth: true
                        text: root.gateway.currentDeviceId
                        onEditingFinished: root.gateway.currentDeviceId = text
                    }

                    Label { text: "TLS"; color: "#6f808c" }
                    Switch {
                        checked: root.gateway.useTls
                        onToggled: root.gateway.useTls = checked
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                radius: 24
                color: "#f7fafc"
                border.color: "#d2dde3"
                border.width: 1

                Column {
                    anchors.fill: parent
                    anchors.margins: 18
                    spacing: 12

                    Label {
                        text: "会话概览"
                        color: "#152d3d"
                        font.pixelSize: 22
                        font.bold: true
                    }

                    Rectangle {
                        width: parent.width
                        radius: 18
                        color: "#eef4f7"
                        border.color: "#d0dbe1"
                        border.width: 1
                        implicitHeight: 94

                        Column {
                            anchors.fill: parent
                            anchors.margins: 14
                            spacing: 6

                            Label {
                                text: "当前会话"
                                color: "#677885"
                                font.pixelSize: 12
                            }

                            Label {
                                text: root.gateway.connectionState
                                color: "#102030"
                                font.pixelSize: 24
                                font.bold: true
                            }

                            Label {
                                width: parent.width
                                text: "订阅: device/+/up/*   发布: device/" + root.gateway.currentDeviceId + "/down/cmd"
                                color: "#647681"
                                font.pixelSize: 12
                                wrapMode: Text.WrapAnywhere
                            }
                        }
                    }

                    Label {
                        width: parent.width
                        text: "工程模式建议在桌面现场使用；公网正式模式优先通过后端 API 下发命令。"
                        wrapMode: Text.Wrap
                        color: "#596a76"
                        font.pixelSize: 13
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            Button {
                Layout.fillWidth: true
                text: root.gateway.connected ? "断开连接" : "连接链路"
                onClicked: {
                    if (root.gateway.connected) {
                        root.gateway.disconnectBroker()
                    } else {
                        root.gateway.connectBroker()
                    }
                }

                contentItem: Text {
                    text: parent.text
                    color: "#102434"
                    font: parent.font
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }

                background: Rectangle {
                    radius: 22
                    color: "#f0d8ba"
                    border.color: "#faecd8"
                    border.width: 1
                }
            }

            Button {
                Layout.fillWidth: true
                text: "刷新状态"
                enabled: root.gateway.connected && !root.gateway.commandTracker.pending
                onClicked: root.gateway.queryStatus()

                contentItem: Text {
                    text: parent.text
                    color: parent.enabled ? "#173042" : "#6c7982"
                    font: parent.font
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }

                background: Rectangle {
                    radius: 22
                    color: parent.enabled ? "#e3edf2" : "#d8e0e5"
                    border.color: "#c4ced4"
                    border.width: 1
                }
            }
        }
    }
}
