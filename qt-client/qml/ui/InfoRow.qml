import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: root
    property string label: ""
    property string value: ""
    property bool mobile: false

    Layout.fillWidth: true
    spacing: 4

    TaskTheme { id: theme }

    Label {
        Layout.fillWidth: true
        text: root.label
        color: theme.textMuted
        font.pixelSize: theme.labelSize(root.mobile)
        elide: Text.ElideRight
    }

    Label {
        Layout.fillWidth: true
        text: root.value
        wrapMode: Text.WrapAnywhere
        color: theme.textBody
        font.pixelSize: root.mobile ? 15 : 14
        font.bold: true
    }
}
