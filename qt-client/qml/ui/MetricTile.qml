import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    property string title: ""
    property string value: ""
    property string unit: ""
    property string caption: ""
    property color accent: theme.accentCyanDeep
    property real fillRatio: -1
    property bool mobile: false

    TaskTheme { id: theme }

    Layout.fillWidth: true
    implicitHeight: fillRatio >= 0 ? 124 : 104
    radius: theme.radiusMedium
    color: theme.surfacePrimary
    border.color: theme.borderSoft
    border.width: 1

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 5

        Label {
            Layout.fillWidth: true
            text: root.title
            color: theme.textMuted
            font.pixelSize: theme.labelSize(root.mobile)
            elide: Text.ElideRight
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 5

            Label {
                Layout.fillWidth: true
                text: root.value
                color: theme.textBody
                font.pixelSize: root.mobile ? 22 : 23
                font.bold: true
                elide: Text.ElideRight
            }

            Label {
                visible: root.unit.length > 0
                text: root.unit
                color: theme.textMuted
                font.pixelSize: 12
            }
        }

        Rectangle {
            visible: root.fillRatio >= 0
            Layout.fillWidth: true
            implicitHeight: 7
            radius: 3
            color: theme.surfaceTint

            Rectangle {
                width: parent.width * Math.max(0, Math.min(1, root.fillRatio))
                height: parent.height
                radius: parent.radius
                color: root.accent
            }
        }

        Label {
            Layout.fillWidth: true
            text: root.caption
            color: theme.textMuted
            font.pixelSize: 11
            wrapMode: Text.Wrap
            maximumLineCount: 2
            elide: Text.ElideRight
        }
    }
}
