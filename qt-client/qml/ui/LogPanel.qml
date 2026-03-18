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
        color: "#0d1720"
        border.width: 1
        border.color: "#294458"
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: mobile ? 16 : 18
        spacing: 12

        RowLayout {
            Layout.fillWidth: true

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2

                Label {
                    text: "运行日志"
                    color: "#f6efe4"
                    font.pixelSize: mobile ? 24 : 22
                    font.bold: true
                }

                Label {
                    text: "Runtime Trace / Link Observation"
                    color: "#97b8cb"
                    font.pixelSize: mobile ? 13 : 11
                }
            }

            Rectangle {
                radius: 16
                implicitWidth: mobile ? 136 : 150
                implicitHeight: mobile ? 38 : 34
                color: "#173547"
                border.color: "#4a7187"
                border.width: 1

                Label {
                    anchors.centerIn: parent
                    text: root.controller.runtimeConnectionState
                    color: "#d8edf7"
                    font.pixelSize: mobile ? 14 : 13
                    font.bold: true
                }
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
                visible: logView.count === 0
                text: "暂无运行日志\n连接设备或发起远程操作后，这里会实时滚动显示收发记录。"
                horizontalAlignment: Text.AlignHCenter
                color: "#6f8796"
                font.pixelSize: mobile ? 15 : 13
            }

            ListView {
                id: logView
                anchors.fill: parent
                anchors.margins: 12
                clip: true
                spacing: 8
                model: root.controller.logModel

                delegate: Text {
                    property string entryText: typeof display !== "undefined"
                                               ? display
                                               : (typeof modelData !== "undefined" ? modelData : "")
                    width: logView.width
                    text: entryText
                    wrapMode: Text.WrapAnywhere
                    color: "#c0deed"
                    font.family: Qt.platform.os === "android" ? "monospace" : "Cascadia Mono"
                    font.pixelSize: mobile ? 14 : 13
                }

                ScrollBar.vertical: ScrollBar {}
                onCountChanged: positionViewAtEnd()
            }
        }
    }
}
