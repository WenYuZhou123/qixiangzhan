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
                GradientStop { position: 0.0; color: "#2a1820" }
                GradientStop { position: 0.55; color: "#4a2834" }
                GradientStop { position: 1.0; color: "#7b4852" }
            }
            border.color: "#b98b92"
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
                            text: "告警中心"
                            color: "#faf0f1"
                            font.pixelSize: mobile ? 26 : 30
                            font.bold: true
                        }

                        Label {
                            text: controller.alarmRepository.currentDeviceId.length > 0
                                  ? "当前设备: " + controller.alarmRepository.currentDeviceId
                                  : "显示全部设备告警"
                            color: "#f0d7db"
                            font.pixelSize: mobile ? 15 : 13
                        }
                    }

                    Button {
                        text: "刷新"
                        font.pixelSize: mobile ? 17 : 14
                        onClicked: root.controller.refreshAlarms()

                        contentItem: Text {
                            text: parent.text
                            color: "#3a1c25"
                            font: parent.font
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }

                        background: Rectangle {
                            radius: 22
                            color: "#f3d8dc"
                            border.color: "#f9eaec"
                            border.width: 1
                        }
                    }
                }

                Label {
                    text: "设备离线、低 RSSI、命令超时与链路故障都会统一收口到这里。"
                    color: "#f3e7e8"
                    font.pixelSize: mobile ? 16 : 13
                }
            }
        }

        ListView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 10
            model: root.controller.alarmRepository

            delegate: Rectangle {
                required property string deviceId
                required property string code
                required property string severity
                required property string message
                required property string source
                required property bool active
                required property string createdAt
                required property string resolvedAt

                width: ListView.view.width
                radius: 22
                color: active ? "#f8e5e7" : "#f0f4f6"
                border.color: active ? "#c05c6b" : "#c5d0d6"
                border.width: 1
                implicitHeight: alarmColumn.implicitHeight + 26

                Column {
                    id: alarmColumn
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.margins: 14
                    spacing: 8

                    Row {
                        spacing: 10

                        Rectangle {
                            radius: 12
                            color: active ? "#e5b5bc" : "#d9e3e7"
                            implicitHeight: 24
                            implicitWidth: active ? 84 : 98

                            Label {
                                anchors.centerIn: parent
                                text: active ? "ACTIVE" : "RESOLVED"
                                color: active ? "#712331" : "#5c6d78"
                                font.pixelSize: 11
                                font.bold: true
                            }
                        }

                        Label {
                            text: severity + " / " + source
                            color: "#5e6f7b"
                            font.pixelSize: mobile ? 15 : 12
                        }
                    }

                    Label {
                        text: deviceId + "  " + code
                        color: "#162d3e"
                        font.pixelSize: mobile ? 20 : 17
                        font.bold: true
                    }

                    Label {
                        text: message
                        wrapMode: Text.Wrap
                        color: "#324753"
                        font.pixelSize: mobile ? 16 : 14
                    }

                    Label {
                        text: active
                              ? "创建时间: " + createdAt
                              : "创建: " + createdAt + "   解除: " + resolvedAt
                        wrapMode: Text.WrapAnywhere
                        color: "#667985"
                        font.pixelSize: mobile ? 15 : 13
                    }
                }
            }

            ScrollBar.vertical: ScrollBar {}
        }
    }
}
