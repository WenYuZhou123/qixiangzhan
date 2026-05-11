import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Pane {
    id: root
    property var controller
    property string stateFilter: "active"
    property string severityFilter: "all"
    readonly property bool mobile: controller && controller.androidMode

    TaskTheme { id: theme }

    function matchesFilter(active, severity) {
        if (stateFilter === "active" && !active)
            return false
        if (stateFilter === "resolved" && active)
            return false
        if (severityFilter !== "all" && severity !== severityFilter)
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
                            text: "告警中心"
                            color: theme.textPrimary
                            font.pixelSize: mobile ? 24 : 26
                            font.bold: true
                            elide: Text.ElideRight
                        }

                        Label {
                            Layout.fillWidth: true
                            text: controller.alarmRepository.currentDeviceId.length > 0
                                  ? "当前设备 " + controller.alarmRepository.currentDeviceId
                                  : "显示全部设备告警"
                            color: "#b9c7d1"
                            font.pixelSize: 13
                            elide: Text.ElideRight
                        }
                    }

                    OpsButton {
                        text: "刷新"
                        Layout.preferredWidth: 86
                        onClicked: root.controller.refreshAlarms()
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    StatusPill {
                        textLabel: "活动"
                        fill: root.stateFilter === "active" ? theme.danger : theme.offline
                        MouseArea { anchors.fill: parent; onClicked: root.stateFilter = "active" }
                    }

                    StatusPill {
                        textLabel: "全部"
                        fill: root.stateFilter === "all" ? theme.pending : theme.offline
                        MouseArea { anchors.fill: parent; onClicked: root.stateFilter = "all" }
                    }

                    StatusPill {
                        textLabel: "已恢复"
                        fill: root.stateFilter === "resolved" ? theme.success : theme.offline
                        MouseArea { anchors.fill: parent; onClicked: root.stateFilter = "resolved" }
                    }

                    Item { Layout.fillWidth: true }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            StatusPill {
                textLabel: "全部级别"
                fill: root.severityFilter === "all" ? theme.pending : theme.offline
                MouseArea { anchors.fill: parent; onClicked: root.severityFilter = "all" }
            }

            StatusPill {
                textLabel: "critical"
                fill: root.severityFilter === "critical" ? theme.danger : theme.offline
                MouseArea { anchors.fill: parent; onClicked: root.severityFilter = "critical" }
            }

            StatusPill {
                textLabel: "warning"
                fill: root.severityFilter === "warning" ? theme.warning : theme.offline
                MouseArea { anchors.fill: parent; onClicked: root.severityFilter = "warning" }
            }

            Item { Layout.fillWidth: true }
        }

        ListView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 8
            model: root.controller.alarmRepository
            ScrollBar.vertical: ScrollBar {}

            delegate: Rectangle {
                required property string deviceId
                required property string code
                required property string severity
                required property string message
                required property string source
                required property bool active
                required property string createdAt
                required property string resolvedAt

                readonly property bool match: root.matchesFilter(active, severity)

                width: ListView.view.width
                visible: match
                height: match ? implicitHeight : 0
                radius: theme.radiusMedium
                color: active ? "#fff3f4" : theme.surfaceSecondary
                border.color: active ? "#e2a2aa" : theme.borderSoft
                border.width: 1
                implicitHeight: alarmColumn.implicitHeight + 24

                ColumnLayout {
                    id: alarmColumn
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: 12
                    spacing: 8

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        StatusPill {
                            textLabel: active ? "ACTIVE" : "RESOLVED"
                            fill: active ? theme.danger : theme.success
                        }

                        StatusPill {
                            textLabel: severity.length > 0 ? severity : "unknown"
                            fill: theme.statusColor(severity)
                        }

                        Label {
                            Layout.fillWidth: true
                            text: source
                            color: theme.textMuted
                            font.pixelSize: 12
                            elide: Text.ElideRight
                        }
                    }

                    Label {
                        Layout.fillWidth: true
                        text: deviceId + " / " + code
                        color: theme.textBody
                        font.pixelSize: mobile ? 18 : 16
                        font.bold: true
                        elide: Text.ElideRight
                    }

                    Label {
                        Layout.fillWidth: true
                        text: message
                        wrapMode: Text.Wrap
                        color: theme.textBody
                        font.pixelSize: mobile ? 15 : 13
                    }

                    Label {
                        Layout.fillWidth: true
                        text: active ? "创建 " + createdAt : "创建 " + createdAt + " / 恢复 " + resolvedAt
                        wrapMode: Text.WrapAnywhere
                        color: theme.textMuted
                        font.pixelSize: 12
                    }
                }
            }
        }
    }
}
