import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ScrollView {
    id: root
    property var controller
    readonly property bool mobile: controller && controller.androidMode

    clip: true

    ColumnLayout {
        width: root.availableWidth
        spacing: 16

        Rectangle {
            Layout.fillWidth: true
            radius: 30
            implicitHeight: mobile ? 220 : 202
            gradient: Gradient {
                GradientStop { position: 0.0; color: "#111e2b" }
                GradientStop { position: 0.56; color: "#233446" }
                GradientStop { position: 1.0; color: "#4a6071" }
            }
            border.color: "#869cac"
            border.width: 1

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: mobile ? 18 : 22
                spacing: 12

                Label {
                    text: "用户控制台"
                    color: "#f5efe4"
                    font.pixelSize: mobile ? 28 : 34
                    font.bold: true
                }

                Label {
                    text: "Identity Grid / Device Permissions"
                    color: "#d0deea"
                    font.pixelSize: mobile ? 15 : 14
                }

                Label {
                    Layout.fillWidth: true
                    text: "仅管理员可见。支持创建用户、启停账号，并按设备维度授予访问权限。"
                    wrapMode: Text.Wrap
                    color: "#e4edf4"
                    font.pixelSize: mobile ? 16 : 14
                }

                Flow {
                    Layout.fillWidth: true
                    spacing: 10

                    Rectangle {
                        radius: 16
                        color: "#10ffffff"
                        border.color: "#8bb0c8"
                        border.width: 1
                        implicitHeight: 38
                        implicitWidth: 170

                        Label {
                            anchors.centerIn: parent
                            text: controller.userAdminService.busy ? "同步中" : "管理员可操作"
                            color: "#eef5f8"
                            font.pixelSize: 14
                            font.bold: true
                        }
                    }

                    Rectangle {
                        radius: 16
                        color: "#10ffffff"
                        border.color: "#8bb0c8"
                        border.width: 1
                        implicitHeight: 38
                        implicitWidth: 150

                        Label {
                            anchors.centerIn: parent
                            text: controller.authSession.role
                            color: "#eef5f8"
                            font.pixelSize: 14
                            font.bold: true
                        }
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            radius: 30
            color: "#ffffff"
            border.color: "#d2dde3"
            border.width: 1

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: mobile ? 18 : 22
                spacing: 16

                RowLayout {
                    Layout.fillWidth: true

                    Label {
                        text: "新建或更新授权"
                        color: "#143040"
                        font.pixelSize: mobile ? 24 : 22
                        font.bold: true
                    }

                    Item { Layout.fillWidth: true }

                    Button {
                        text: "刷新"
                        onClicked: root.controller.userAdminService.refreshUsers()

                        contentItem: Text {
                            text: parent.text
                            color: "#102434"
                            font: parent.font
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }

                        background: Rectangle {
                            radius: 22
                            color: "#e4edf2"
                            border.color: "#c4ced4"
                            border.width: 1
                        }
                    }
                }

                GridLayout {
                    Layout.fillWidth: true
                    columns: mobile ? 1 : 2
                    columnSpacing: 14
                    rowSpacing: 14

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 6
                        Label { text: "用户名"; color: "#6f808c"; font.pixelSize: mobile ? 15 : 12 }
                        TextField { id: usernameField; Layout.fillWidth: true; font.pixelSize: mobile ? 17 : 15 }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 6
                        Label { text: "显示名称"; color: "#6f808c"; font.pixelSize: mobile ? 15 : 12 }
                        TextField { id: displayNameField; Layout.fillWidth: true; font.pixelSize: mobile ? 17 : 15 }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 6
                        Label { text: "密码"; color: "#6f808c"; font.pixelSize: mobile ? 15 : 12 }
                        TextField { id: passwordField; Layout.fillWidth: true; echoMode: TextInput.Password; font.pixelSize: mobile ? 17 : 15 }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 6
                        Label { text: "角色"; color: "#6f808c"; font.pixelSize: mobile ? 15 : 12 }
                        ComboBox {
                            id: roleBox
                            Layout.fillWidth: true
                            model: ["operator", "admin"]
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.columnSpan: mobile ? 1 : 2
                        spacing: 6

                        Label {
                            text: "设备 ID 列表"
                            color: "#6f808c"
                            font.pixelSize: mobile ? 15 : 12
                        }

                        TextField {
                            id: deviceIdsField
                            Layout.fillWidth: true
                            placeholderText: "relay_h743_001, relay_h743_002"
                            font.pixelSize: mobile ? 17 : 15
                        }
                    }
                }

                Button {
                    Layout.fillWidth: true
                    text: "创建用户"
                    enabled: !root.controller.userAdminService.busy
                    font.pixelSize: mobile ? 18 : 15
                    onClicked: root.controller.userAdminService.createUser(
                                   usernameField.text,
                                   displayNameField.text,
                                   passwordField.text,
                                   roleBox.currentText,
                                   deviceIdsField.text)

                    contentItem: Text {
                        text: parent.text
                        color: "#102434"
                        font: parent.font
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }

                    background: Rectangle {
                        radius: 22
                        color: parent.enabled ? "#f0d8ba" : "#d5dde2"
                        border.color: parent.enabled ? "#f9ebd7" : "#c7d0d6"
                        border.width: 1
                    }
                }

                Label {
                    visible: root.controller.userAdminService.lastError.length > 0
                    text: root.controller.userAdminService.lastError
                    color: "#9a3344"
                    wrapMode: Text.Wrap
                    font.pixelSize: mobile ? 16 : 13
                }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 12

            Repeater {
                model: root.controller.userAdminService.users

                delegate: Rectangle {
                    required property var modelData
                    Layout.fillWidth: true
                    radius: 26
                    color: "#ffffff"
                    border.color: "#d2dde3"
                    border.width: 1
                    implicitHeight: userColumn.implicitHeight + 30

                    ColumnLayout {
                        id: userColumn
                        anchors.fill: parent
                        anchors.margins: 16
                        spacing: 12

                        RowLayout {
                            Layout.fillWidth: true

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 2

                                Label {
                                    text: modelData.display_name + " (" + modelData.username + ")"
                                    color: "#132a39"
                                    font.pixelSize: mobile ? 20 : 19
                                    font.bold: true
                                }

                                Label {
                                    text: "用户 ID: " + modelData.id
                                    color: "#667985"
                                    font.pixelSize: mobile ? 14 : 12
                                }
                            }

                            Rectangle {
                                radius: 16
                                implicitWidth: mobile ? 126 : 146
                                implicitHeight: 34
                                color: modelData.is_active ? "#e6f4ec" : "#f8e5e7"
                                border.color: modelData.is_active ? "#9fceb4" : "#deabb4"
                                border.width: 1

                                Label {
                                    anchors.centerIn: parent
                                    text: modelData.role + (modelData.is_active ? " / active" : " / disabled")
                                    color: modelData.is_active ? "#1f7c4b" : "#8f2231"
                                    font.pixelSize: mobile ? 14 : 12
                                    font.bold: true
                                }
                            }
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            radius: 18
                            color: "#f5f8fa"
                            border.color: "#d5dfe5"
                            border.width: 1
                            implicitHeight: 74

                            Column {
                                anchors.fill: parent
                                anchors.margins: 12
                                spacing: 4

                                Label {
                                    text: "设备授权"
                                    color: "#697a86"
                                    font.pixelSize: mobile ? 15 : 12
                                }

                                Label {
                                    width: parent.width
                                    text: modelData.device_ids.length > 0 ? modelData.device_ids.join(", ") : "尚未分配设备"
                                    wrapMode: Text.Wrap
                                    color: "#173042"
                                    font.pixelSize: mobile ? 16 : 14
                                    font.bold: true
                                }
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 10

                            TextField {
                                id: assignmentField
                                Layout.fillWidth: true
                                text: modelData.device_ids.join(", ")
                                placeholderText: "device ids"
                                font.pixelSize: mobile ? 16 : 14
                            }

                            GridLayout {
                                Layout.fillWidth: true
                                columns: mobile ? 1 : 2
                                columnSpacing: 10
                                rowSpacing: 10

                                Button {
                                    Layout.fillWidth: true
                                    text: "分配设备"
                                    onClicked: root.controller.userAdminService.assignDevices(modelData.id, assignmentField.text)

                                    contentItem: Text {
                                        text: parent.text
                                        color: "#102434"
                                        font: parent.font
                                        horizontalAlignment: Text.AlignHCenter
                                        verticalAlignment: Text.AlignVCenter
                                    }

                                    background: Rectangle {
                                        radius: 20
                                        color: "#f0d8ba"
                                        border.color: "#faecd8"
                                        border.width: 1
                                    }
                                }

                                Button {
                                    Layout.fillWidth: true
                                    text: modelData.is_active ? "禁用用户" : "启用用户"
                                    onClicked: root.controller.userAdminService.setUserActive(modelData.id, !modelData.is_active)

                                    contentItem: Text {
                                        text: parent.text
                                        color: modelData.is_active ? "#7e2431" : "#19452f"
                                        font: parent.font
                                        horizontalAlignment: Text.AlignHCenter
                                        verticalAlignment: Text.AlignVCenter
                                    }

                                    background: Rectangle {
                                        radius: 20
                                        color: modelData.is_active ? "#f6dce0" : "#ddf0e5"
                                        border.color: modelData.is_active ? "#e3b7bf" : "#b7d8c4"
                                        border.width: 1
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
