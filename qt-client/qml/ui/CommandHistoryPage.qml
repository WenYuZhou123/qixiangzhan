import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Pane {
    id: root
    property var controller
    readonly property bool mobile: controller && controller.androidMode

    padding: 0
    background: Rectangle {
        radius: mobile ? 28 : 30
        color: "#ffffff"
        border.color: "#d2dde3"
        border.width: 1
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: mobile ? 16 : 18
        spacing: 14

        Rectangle {
            Layout.fillWidth: true
            radius: 26
            implicitHeight: mobile ? 168 : 144
            gradient: Gradient {
                GradientStop { position: 0.0; color: "#112436" }
                GradientStop { position: 0.55; color: "#1f4259" }
                GradientStop { position: 1.0; color: "#4d6f84" }
            }
            border.color: "#6f93a8"
            border.width: 1

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: mobile ? 18 : 20
                spacing: 10

                RowLayout {
                    Layout.fillWidth: true

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 3

                        Label {
                            text: "命令与消息历史"
                            color: "#f4efe4"
                            font.pixelSize: mobile ? 26 : 30
                            font.bold: true
                        }

                        Label {
                            text: controller.historyRepository.currentDeviceId.length > 0
                                  ? "当前过滤设备: " + controller.historyRepository.currentDeviceId
                                  : "显示全部缓存与云端同步后的消息"
                            color: "#d0deea"
                            font.pixelSize: mobile ? 15 : 13
                        }
                    }

                    Button {
                        text: "刷新"
                        font.pixelSize: mobile ? 17 : 14
                        onClicked: root.controller.refreshHistory()

                        contentItem: Text {
                            text: parent.text
                            color: "#122635"
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
                    text: "发送与接收统一收口，便于排查延迟、重发和命令确认。"
                    color: "#e5eef4"
                    font.pixelSize: mobile ? 16 : 13
                }
            }
        }

        ListView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 10
            model: root.controller.historyRepository

            delegate: Rectangle {
                required property string deviceId
                required property string direction
                required property string channel
                required property string topic
                required property string command
                required property string payload
                required property string result
                required property string createdAt

                width: ListView.view.width
                radius: 22
                color: direction === "out" ? "#f7efe4" : "#eef6f9"
                border.color: direction === "out" ? "#d8bd89" : "#b7d4df"
                border.width: 1
                implicitHeight: historyColumn.implicitHeight + 26

                Column {
                    id: historyColumn
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.margins: 14
                    spacing: 8

                    Row {
                        spacing: 10

                        Rectangle {
                            radius: 12
                            color: direction === "out" ? "#e7d1a8" : "#cfe5ee"
                            implicitHeight: 24
                            implicitWidth: 74

                            Label {
                                anchors.centerIn: parent
                                text: direction === "out" ? "TX" : "RX"
                                color: "#132635"
                                font.pixelSize: 11
                                font.bold: true
                            }
                        }

                        Label {
                            text: createdAt
                            color: "#253948"
                            font.pixelSize: mobile ? 15 : 13
                            font.bold: true
                        }

                        Label {
                            text: "[" + channel + "]"
                            color: "#60717e"
                            font.pixelSize: mobile ? 15 : 12
                        }
                    }

                    Label {
                        text: command.length > 0 ? command : topic
                        wrapMode: Text.WrapAnywhere
                        color: "#102534"
                        font.pixelSize: mobile ? 20 : 17
                        font.bold: true
                    }

                    Label {
                        text: deviceId
                        color: "#617280"
                        font.pixelSize: mobile ? 16 : 13
                    }

                    Label {
                        text: payload
                        wrapMode: Text.WrapAnywhere
                        color: "#465863"
                        font.pixelSize: mobile ? 15 : 13
                    }

                    Label {
                        text: result.length > 0 ? "结果: " + result : "等待结果"
                        color: result.length > 0 ? "#2c6d4e" : "#6b7c88"
                        font.pixelSize: mobile ? 15 : 13
                    }
                }
            }

            ScrollBar.vertical: ScrollBar {}
        }
    }
}
