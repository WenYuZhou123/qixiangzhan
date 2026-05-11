import QtQuick
import QtQuick.Controls

Button {
    id: root
    property color fillColor: theme.surfaceSecondary
    property color labelColor: theme.textBody
    property color borderColor: theme.borderSoft
    property bool pending: false

    TaskTheme { id: theme }

    implicitHeight: theme.touchTarget

    contentItem: Text {
        text: root.pending ? root.text + "..." : root.text
        color: root.enabled ? root.labelColor : "#74828c"
        font.pixelSize: 14
        font.bold: true
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    background: Rectangle {
        radius: theme.radiusSmall
        color: root.enabled ? root.fillColor : theme.neutral
        border.color: root.enabled ? root.borderColor : "#c6d0d7"
        border.width: 1
        opacity: root.down ? 0.85 : 1.0
    }
}
