import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    property string title: ""
    property bool online: true
    property string detail: ""
    property string raw: ""
    property bool mobile: false

    TaskTheme { id: theme }

    Layout.fillWidth: true
    implicitHeight: 68
    radius: theme.radiusSmall
    color: theme.surfaceSecondary
    border.color: online ? "#bdd8cb" : "#e1b6bd"
    border.width: 1

    RowLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 10

        Rectangle {
            Layout.preferredWidth: 10
            Layout.fillHeight: true
            radius: 4
            color: root.online ? theme.success : theme.danger
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 3

            Label {
                Layout.fillWidth: true
                text: root.title
                color: theme.textBody
                font.pixelSize: root.mobile ? 16 : 15
                font.bold: true
                elide: Text.ElideRight
            }

            Label {
                Layout.fillWidth: true
                text: root.detail
                color: theme.textMuted
                font.pixelSize: 12
                elide: Text.ElideRight
            }
        }

        StatusPill {
            textLabel: root.online ? "ONLINE" : "OFFLINE"
            fill: root.online ? theme.success : theme.danger
        }
    }
}
