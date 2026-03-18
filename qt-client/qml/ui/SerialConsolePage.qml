import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Pane {
    id: root
    property var controller
    readonly property var serial: controller.serialConsoleService

    padding: 0
    background: Rectangle {
        radius: 30
        color: "#ffffff"
        border.color: "#d2dde3"
        border.width: 1
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
                GradientStop { position: 0.0; color: "#0f2030" }
                GradientStop { position: 0.55; color: "#18374d" }
                GradientStop { position: 1.0; color: "#3f6177" }
            }
            border.color: "#7296aa"
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
                            text: "串口实验室"
                            color: "#f4efe3"
                            font.pixelSize: 30
                            font.bold: true
                        }

                        Label {
                            text: serial.supported
                                  ? "AT / MQTT 联调快速面板"
                                  : "当前构建未带 Qt SerialPort，页面以占位模式运行"
                            color: "#d0deea"
                            font.pixelSize: 13
                        }
                    }

                    Button {
                        text: "刷新串口"
                        onClicked: serial.refreshPorts()

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
                }

                Label {
                    text: "工程模式下可直接调用开发板 AT 指令、网络初始化与 MQTT 自检命令。"
                    color: "#e4edf4"
                    font.pixelSize: 13
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            ComboBox {
                Layout.fillWidth: true
                model: serial.availablePorts
                currentIndex: Math.max(0, serial.availablePorts.indexOf(serial.selectedPort))
                onActivated: serial.selectedPort = currentText
            }

            SpinBox {
                from: 1200
                to: 921600
                value: serial.baudRate
                editable: true
                onValueModified: serial.baudRate = value
            }

            Button {
                text: serial.connected ? "关闭" : "连接"
                onClicked: {
                    if (serial.connected) {
                        serial.closePort()
                    } else {
                        serial.openPort()
                    }
                }
            }
        }

        Flow {
            Layout.fillWidth: true
            spacing: 8

            Repeater {
                model: ["ATRAW", "SELFTEST", "NETINIT", "MQTTINIT", "MQTTCLOSE", "STATUS"]

                delegate: Button {
                    text: modelData
                    onClicked: serial.sendPreset(modelData)
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            TextField {
                id: rawInput
                Layout.fillWidth: true
                placeholderText: "输入串口命令"
                onAccepted: {
                    serial.sendRaw(text)
                    clear()
                }
            }

            Button {
                text: "发送"
                onClicked: {
                    serial.sendRaw(rawInput.text)
                    rawInput.clear()
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            color: serial.lastError.length > 0 ? "#f7e1e3" : "#edf6e9"
            radius: 18
            border.color: serial.lastError.length > 0 ? "#e4b7be" : "#c6d9c2"
            border.width: 1
            implicitHeight: 72

            Label {
                anchors.fill: parent
                anchors.margins: 14
                verticalAlignment: Text.AlignVCenter
                text: serial.lastError.length > 0 ? serial.lastError : "串口链路正常，可继续发送调试指令。"
                wrapMode: Text.Wrap
                color: serial.lastError.length > 0 ? "#8f2231" : "#446543"
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: 22
            color: "#091119"
            border.color: "#234055"
            border.width: 1

            Label {
                anchors.centerIn: parent
                visible: serialLogView.count === 0
                text: "暂无串口日志\n连接串口或发送指令后，这里会实时显示返回内容。"
                horizontalAlignment: Text.AlignHCenter
                color: "#6f8796"
            }

            ListView {
                id: serialLogView
                anchors.fill: parent
                anchors.margins: 12
                clip: true
                spacing: 8
                model: serial.logModel

                delegate: Text {
                    property string entryText: typeof display !== "undefined"
                                               ? display
                                               : (typeof modelData !== "undefined" ? modelData : "")
                    width: serialLogView.width
                    text: entryText
                    wrapMode: Text.WrapAnywhere
                    color: "#b9d7e8"
                    font.family: "Cascadia Mono"
                    font.pixelSize: 13
                }

                ScrollBar.vertical: ScrollBar {}
                onCountChanged: positionViewAtEnd()
            }
        }
    }
}
