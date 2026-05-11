import QtQuick
import QtQuick.Controls

Rectangle {
    id: root
    property string textLabel: ""
    property color fill: theme.offline
    property color labelColor: "white"

    TaskTheme { id: theme }

    implicitHeight: 30
    implicitWidth: label.implicitWidth + 22
    radius: theme.radiusSmall
    color: fill

    Label {
        id: label
        anchors.centerIn: parent
        text: root.textLabel
        color: root.labelColor
        font.pixelSize: 12
        font.bold: true
        elide: Text.ElideRight
    }
}
