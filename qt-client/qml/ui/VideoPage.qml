import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtWebView

Item {
    id: root
    property var controller
    readonly property bool mobile: controller.androidMode || width < 900
    readonly property string deviceId: controller.deviceRepository.currentDeviceId.length > 0
                                     ? controller.deviceRepository.currentDeviceId : "drone-01"

    TaskTheme { id: theme }

    function connectVideo() {
        controller.videoStreamService.start(deviceId)
    }

    onVisibleChanged: {
        if (visible)
            connectVideo()
        else
            controller.videoStreamService.stop()
    }

    Component.onCompleted: {
        if (visible)
            connectVideo()
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: theme.cardGap

        RowLayout {
            Layout.fillWidth: true

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2

                Label {
                    text: "无人机图传"
                    color: theme.textBody
                    font.pixelSize: theme.pageTitleSize(root.mobile)
                    font.bold: true
                }
                Label {
                    text: root.deviceId + "  ·  " + controller.videoStreamService.state
                    color: theme.textMuted
                    font.pixelSize: theme.labelSize(root.mobile)
                }
            }

            ToolButton {
                text: "刷新"
                enabled: !controller.videoStreamService.loading
                onClicked: root.connectVideo()
                Accessible.name: "刷新图传"
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: "#101820"
            radius: root.mobile ? 8 : theme.radiusMedium
            clip: true

            WebView {
                id: player
                anchors.fill: parent
                visible: controller.videoStreamService.viewerUrl.length > 0
                url: controller.videoStreamService.viewerUrl
            }

            Column {
                anchors.centerIn: parent
                width: Math.min(parent.width - 40, 310)
                spacing: 10
                visible: !player.visible

                BusyIndicator {
                    anchors.horizontalCenter: parent.horizontalCenter
                    running: controller.videoStreamService.loading
                    visible: running
                }
                Label {
                    width: parent.width
                    text: controller.videoStreamService.lastError.length > 0
                          ? controller.videoStreamService.lastError : "正在建立低延迟视频连接"
                    color: "#e9f1f4"
                    wrapMode: Text.Wrap
                    horizontalAlignment: Text.AlignHCenter
                }
                Button {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: "重新连接"
                    onClicked: root.connectVideo()
                }
            }
        }
    }
}
