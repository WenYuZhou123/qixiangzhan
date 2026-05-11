import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Pane {
    id: root
    property var controller
    property string directionFilter: "all"
    property string resultFilter: "all"
    readonly property bool mobile: controller && controller.androidMode

    TaskTheme { id: theme }

    function resultGroup(result) {
        const normalized = (result || "").toLowerCase()
        if (normalized.length === 0 || normalized === "pending" || normalized === "queued" || normalized === "sent")
            return "pending"
        if (normalized === "success" || normalized === "ack_success" || normalized === "ack")
            return "success"
        if (normalized.indexOf("timeout") >= 0)
            return "timeout"
        return "failed"
    }

    function matchesFilter(direction, result) {
        if (directionFilter !== "all" && direction !== directionFilter)
            return false
        if (resultFilter !== "all" && resultGroup(result) !== resultFilter)
            return false
        return true
    }

    padding: 0
    background: Rectangle {
        radius: theme.radiusLarge
        color: theme.surfacePrimary
        border.color: theme.borderSoft
        border.width: 1
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: mobile ? 12 : 14
        spacing: 12

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: mobile ? 130 : 116
            radius: theme.radiusLarge
            color: theme.navSurface
            border.color: "#2f3d4c"
            border.width: 1

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 14
                spacing: 8

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2

                        Label {
                            Layout.fillWidth: true
                            text: "命令与消息历史"
                            color: theme.textPrimary
                            font.pixelSize: mobile ? 24 : 26
                            font.bold: true
                            elide: Text.ElideRight
                        }

                        Label {
                            Layout.fillWidth: true
                            text: controller.historyRepository.currentDeviceId.length > 0
                                  ? "当前设备 " + controller.historyRepository.currentDeviceId
                                  : "显示全部缓存与云端同步消息"
                            color: "#b9c7d1"
                            font.pixelSize: 13
                            elide: Text.ElideRight
                        }
                    }

                    OpsButton {
                        text: "刷新"
                        Layout.preferredWidth: 86
                        onClicked: root.controller.refreshHistory()
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    StatusPill { textLabel: "全部"; fill: root.directionFilter === "all" ? theme.pending : theme.offline; MouseArea { anchors.fill: parent; onClicked: root.directionFilter = "all" } }
                    StatusPill { textLabel: "TX"; fill: root.directionFilter === "out" ? theme.warning : theme.offline; MouseArea { anchors.fill: parent; onClicked: root.directionFilter = "out" } }
                    StatusPill { textLabel: "RX"; fill: root.directionFilter === "in" ? theme.success : theme.offline; MouseArea { anchors.fill: parent; onClicked: root.directionFilter = "in" } }
                    Item { Layout.fillWidth: true }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            StatusPill { textLabel: "全部结果"; fill: root.resultFilter === "all" ? theme.pending : theme.offline; MouseArea { anchors.fill: parent; onClicked: root.resultFilter = "all" } }
            StatusPill { textLabel: "pending"; fill: root.resultFilter === "pending" ? theme.pending : theme.offline; MouseArea { anchors.fill: parent; onClicked: root.resultFilter = "pending" } }
            StatusPill { textLabel: "success"; fill: root.resultFilter === "success" ? theme.success : theme.offline; MouseArea { anchors.fill: parent; onClicked: root.resultFilter = "success" } }
            StatusPill { textLabel: "failed"; fill: root.resultFilter === "failed" ? theme.danger : theme.offline; MouseArea { anchors.fill: parent; onClicked: root.resultFilter = "failed" } }
            StatusPill { textLabel: "timeout"; fill: root.resultFilter === "timeout" ? theme.warning : theme.offline; MouseArea { anchors.fill: parent; onClicked: root.resultFilter = "timeout" } }
            Item { Layout.fillWidth: true }
        }

        ListView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 8
            model: root.controller.historyRepository
            ScrollBar.vertical: ScrollBar {}

            delegate: Rectangle {
                required property string deviceId
                required property string direction
                required property string channel
                required property string topic
                required property string command
                required property string payload
                required property string result
                required property string createdAt

                readonly property string group: root.resultGroup(result)
                readonly property bool match: root.matchesFilter(direction, result)

                width: ListView.view.width
                visible: match
                height: match ? implicitHeight : 0
                radius: theme.radiusMedium
                color: direction === "out" ? "#fff7ec" : "#eef8fb"
                border.color: direction === "out" ? "#e0c38e" : "#b8dce5"
                border.width: 1
                implicitHeight: historyColumn.implicitHeight + 24

                ColumnLayout {
                    id: historyColumn
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: 12
                    spacing: 8

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        StatusPill {
                            textLabel: direction === "out" ? "TX" : "RX"
                            fill: direction === "out" ? theme.warning : theme.success
                        }

                        StatusPill {
                            textLabel: group
                            fill: theme.statusColor(group)
                        }

                        Label {
                            Layout.fillWidth: true
                            text: createdAt
                            color: theme.textMuted
                            font.pixelSize: 12
                            elide: Text.ElideRight
                        }
                    }

                    Label {
                        Layout.fillWidth: true
                        text: command.length > 0 ? command : topic
                        wrapMode: Text.WrapAnywhere
                        color: theme.textBody
                        font.pixelSize: mobile ? 18 : 16
                        font.bold: true
                    }

                    Label {
                        Layout.fillWidth: true
                        text: deviceId + " / " + channel
                        color: theme.textMuted
                        font.pixelSize: 12
                        elide: Text.ElideRight
                    }

                    Label {
                        Layout.fillWidth: true
                        text: payload
                        wrapMode: Text.WrapAnywhere
                        maximumLineCount: 3
                        elide: Text.ElideRight
                        color: "#455560"
                        font.pixelSize: mobile ? 14 : 12
                    }

                    Label {
                        Layout.fillWidth: true
                        text: result.length > 0 ? "结果 " + result : "等待结果"
                        color: theme.statusColor(group)
                        font.pixelSize: 13
                        font.bold: true
                    }
                }
            }
        }
    }
}
