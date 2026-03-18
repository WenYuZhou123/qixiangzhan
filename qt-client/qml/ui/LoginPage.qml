import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ScrollView {
    id: root
    property var controller
    readonly property bool mobile: controller && controller.androidMode
    property bool adminModeSelected: !controller || !controller.authSession.guestMode

    TaskTheme { id: theme }

    function normalizedError() {
        if (!controller || !controller.authSession)
            return ""

        const lastError = controller.authSession.lastError
        if (!lastError || lastError.length === 0)
            return ""
        if (lastError.indexOf("Secure storage") >= 0 || lastError.indexOf("安全存储") >= 0)
            return "当前环境无法使用安全存储，本次登录不会被长期记住。"
        if (lastError.indexOf("Connection closed") >= 0)
            return "连接已关闭，请确认后端服务已经启动。"
        if (lastError.indexOf("Connection refused") >= 0 || lastError.indexOf("refused") >= 0)
            return "连接被拒绝，请检查 API 地址和端口。"
        if (lastError.indexOf("Invalid or missing bearer token") >= 0)
            return "登录态无效，请重新登录。"
        return lastError
    }

    function roleText() {
        if (!controller || !controller.authSession)
            return "访客"
        return theme.roleText(controller.authSession.role)
    }

    function sessionText() {
        if (!controller || !controller.authSession)
            return "未登录"
        if (controller.authSession.guestMode)
            return "游客预览"
        if (controller.authSession.authenticated)
            return "远程会话已建立"
        return "等待登录"
    }

    clip: true

    Component.onCompleted: {
        if (controller && controller.authSession && controller.authSession.guestMode)
            adminModeSelected = false
    }

    Connections {
        target: controller ? controller.authSession : null

        function onSessionChanged() {
            if (!controller || !controller.authSession)
                return
            adminModeSelected = !controller.authSession.guestMode
        }
    }

    component ModeTab: Rectangle {
        required property string label
        required property bool active
        required property var clickHandler
        Layout.fillWidth: true
        implicitHeight: mobile ? 46 : 44
        radius: 20
        color: active ? "#eef4f6" : "#10314959"
        border.color: active ? theme.borderSoft : "#6f8ea1"
        border.width: 1

        Label {
            anchors.centerIn: parent
            text: parent.label
            color: parent.active ? theme.textBody : theme.textPrimary
            font.pixelSize: mobile ? 17 : 15
            font.bold: true
        }

        MouseArea {
            anchors.fill: parent
            onClicked: parent.clickHandler()
        }
    }

    component StatusPill: Rectangle {
        required property string label
        required property string value
        Layout.fillWidth: true
        implicitHeight: 56
        radius: 18
        color: "#0fffffff"
        border.color: "#7692a3"
        border.width: 1

        Column {
            anchors.fill: parent
            anchors.margins: 12
            spacing: 2

            Label {
                text: parent.parent.label
                color: "#bfd0db"
                font.pixelSize: 11
            }

            Label {
                text: parent.parent.value
                color: theme.textPrimary
                font.pixelSize: mobile ? 15 : 15
                font.bold: true
                elide: Text.ElideRight
            }
        }
    }

    ColumnLayout {
        width: root.availableWidth
        spacing: 16

        Rectangle {
            Layout.fillWidth: true
            radius: 30
            implicitHeight: mobile ? 252 : 238
            gradient: Gradient {
                GradientStop { position: 0.0; color: "#0c1c2a" }
                GradientStop { position: 0.55; color: "#173447" }
                GradientStop { position: 1.0; color: "#335166" }
            }
            border.color: "#6f8da0"
            border.width: 1

            Rectangle {
                width: parent.width * 0.34
                height: parent.height * 1.25
                x: parent.width * 0.55
                y: -parent.height * 0.25
                radius: width / 2
                color: "#1fd7e5ed"
            }

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: mobile ? 18 : 22
                spacing: 12

                Label {
                    text: "身份接入"
                    color: theme.textPrimary
                    font.pixelSize: mobile ? 28 : 34
                    font.bold: true
                }

                Label {
                    Layout.fillWidth: true
                    text: "管理员模式连接后端接口与设备控制；游客模式只读预览，不会发起远程命令。"
                    wrapMode: Text.Wrap
                    color: "#dde8ee"
                    font.pixelSize: mobile ? 15 : 14
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10

                    ModeTab {
                        label: "管理员模式"
                        active: root.adminModeSelected
                        clickHandler: function() { root.adminModeSelected = true }
                    }

                    ModeTab {
                        label: "游客模式"
                        active: !root.adminModeSelected
                        clickHandler: function() { root.adminModeSelected = false }
                    }
                }

                GridLayout {
                    Layout.fillWidth: true
                    columns: 2
                    columnSpacing: 10
                    rowSpacing: 10

                    StatusPill {
                        label: "会话状态"
                        value: root.sessionText()
                    }

                    StatusPill {
                        label: "云端链路"
                        value: theme.connectionStateText(controller.runtimeConnectionState)
                    }
                }
            }
        }

        Loader {
            Layout.fillWidth: true
            sourceComponent: adminModeSelected ? adminLoginCard : guestModeCard
        }

        Rectangle {
            Layout.fillWidth: true
            radius: 26
            color: "#fbfcfd"
            border.color: theme.borderSoft
            border.width: 1
            implicitHeight: sessionBody.implicitHeight + 36

            ColumnLayout {
                id: sessionBody
                anchors.fill: parent
                anchors.margins: 18
                spacing: 10

                Label {
                    text: controller.authSession.authenticated ? "当前会话" : "接入说明"
                    color: theme.textBody
                    font.pixelSize: mobile ? 22 : 20
                    font.bold: true
                }

                Label {
                    Layout.fillWidth: true
                    text: controller.authSession.authenticated
                          ? "当前身份：" + controller.authSession.displayName + "  ·  " + root.roleText()
                          : "管理员模式支持远程登录、设备查询和控制；游客模式仅浏览本地缓存和演示状态。"
                    wrapMode: Text.Wrap
                    color: "#435764"
                    font.pixelSize: mobile ? 16 : 14
                }

                Label {
                    Layout.fillWidth: true
                    text: controller.authSession.guestMode
                          ? "游客模式不会请求远程接口，也不会发送任何控制命令。"
                          : "运行链路：" + theme.connectionStateText(controller.runtimeConnectionState)
                    wrapMode: Text.Wrap
                    color: controller.authSession.guestMode ? "#5f786f" : "#587a72"
                    font.pixelSize: mobile ? 16 : 14
                }

                Label {
                    visible: root.normalizedError().length > 0
                    Layout.fillWidth: true
                    text: root.normalizedError()
                    wrapMode: Text.Wrap
                    color: theme.danger
                    font.pixelSize: mobile ? 15 : 13
                }
            }
        }
    }

    Component {
        id: adminLoginCard

        Rectangle {
            Layout.fillWidth: true
            radius: 28
            color: "#ffffff"
            border.color: theme.borderSoft
            border.width: 1
            implicitHeight: adminBody.implicitHeight + 40

            ColumnLayout {
                id: adminBody
                anchors.fill: parent
                anchors.margins: mobile ? 18 : 22
                spacing: 14

                Label {
                    text: "管理员登录"
                    color: theme.textBody
                    font.pixelSize: mobile ? 24 : 22
                    font.bold: true
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Label {
                        text: "接口地址"
                        color: theme.textMuted
                        font.pixelSize: mobile ? 15 : 13
                    }

                    TextField {
                        id: apiField
                        Layout.fillWidth: true
                        text: root.controller.authSession.apiBaseUrl
                        placeholderText: "请输入后端地址"
                        font.pixelSize: mobile ? 18 : 15
                        onEditingFinished: root.controller.authSession.apiBaseUrl = text
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Label {
                        text: "用户名"
                        color: theme.textMuted
                        font.pixelSize: mobile ? 15 : 13
                    }

                    TextField {
                        id: usernameField
                        Layout.fillWidth: true
                        text: root.controller.authSession.username.length > 0
                              ? root.controller.authSession.username
                              : "admin"
                        placeholderText: "请输入管理员用户名"
                        font.pixelSize: mobile ? 18 : 15
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Label {
                        text: "密码"
                        color: theme.textMuted
                        font.pixelSize: mobile ? 15 : 13
                    }

                    TextField {
                        id: passwordField
                        Layout.fillWidth: true
                        echoMode: TextInput.Password
                        text: "admin123"
                        placeholderText: "请输入密码"
                        font.pixelSize: mobile ? 18 : 15
                        onAccepted: root.controller.authSession.login(usernameField.text, passwordField.text)
                    }
                }

                GridLayout {
                    Layout.fillWidth: true
                    columns: mobile ? 1 : 2
                    columnSpacing: 12
                    rowSpacing: 12

                    Button {
                        Layout.fillWidth: true
                        text: controller.authSession.authenticated ? "重新登录" : "登录管理员"
                        enabled: !controller.authSession.busy
                        font.pixelSize: mobile ? 18 : 15
                        onClicked: controller.authSession.login(usernameField.text, passwordField.text)

                        contentItem: Text {
                            text: parent.text
                            color: "#102434"
                            font: parent.font
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }

                        background: Rectangle {
                            radius: 22
                            color: parent.enabled ? "#dce7ec" : "#d5dde2"
                            border.color: "#c4d2da"
                            border.width: 1
                        }
                    }

                    Button {
                        Layout.fillWidth: true
                        text: "退出登录"
                        enabled: controller.authSession.authenticated
                        font.pixelSize: mobile ? 18 : 15
                        onClicked: controller.authSession.logout()

                        contentItem: Text {
                            text: parent.text
                            color: parent.enabled ? "#173042" : "#6a7882"
                            font: parent.font
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }

                        background: Rectangle {
                            radius: 22
                            color: parent.enabled ? "#eef3f5" : "#d8e0e5"
                            border.color: "#c9d4db"
                            border.width: 1
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    radius: 20
                    color: "#f4f8fa"
                    border.color: theme.borderSoft
                    border.width: 1
                    implicitHeight: mobile ? 88 : 76

                    Label {
                        anchors.fill: parent
                        anchors.margins: 14
                        text: "管理员模式用于远程登录、设备查询和控制。Access / Refresh Token 会优先写入系统安全存储。"
                        wrapMode: Text.Wrap
                        color: "#51636f"
                        font.pixelSize: mobile ? 15 : 13
                    }
                }
            }
        }
    }

    Component {
        id: guestModeCard

        Rectangle {
            Layout.fillWidth: true
            radius: 28
            color: "#ffffff"
            border.color: theme.borderSoft
            border.width: 1
            implicitHeight: guestBody.implicitHeight + 40

            ColumnLayout {
                id: guestBody
                anchors.fill: parent
                anchors.margins: mobile ? 18 : 22
                spacing: 14

                Label {
                    text: "游客模式"
                    color: theme.textBody
                    font.pixelSize: mobile ? 24 : 22
                    font.bold: true
                }

                Label {
                    Layout.fillWidth: true
                    text: "游客模式用于演示和只读浏览，不会请求远程接口，也不会下发停机场或工程调试命令。"
                    wrapMode: Text.Wrap
                    color: "#5a6b77"
                    font.pixelSize: mobile ? 16 : 14
                }

                Rectangle {
                    Layout.fillWidth: true
                    radius: 20
                    color: "#f4f8fa"
                    border.color: theme.borderSoft
                    border.width: 1
                    implicitHeight: mobile ? 124 : 108

                    Column {
                        anchors.fill: parent
                        anchors.margins: 14
                        spacing: 6

                        Label {
                            text: "游客模式包含"
                            color: theme.textBody
                            font.pixelSize: mobile ? 18 : 16
                            font.bold: true
                        }

                        Label {
                            width: parent.width
                            text: "1. 浏览本地缓存的设备状态与历史\n2. 查看当前界面框架与演示数据\n3. 不触发任何远程认证和控制请求"
                            wrapMode: Text.Wrap
                            color: "#51636f"
                            font.pixelSize: mobile ? 15 : 13
                        }
                    }
                }

                GridLayout {
                    Layout.fillWidth: true
                    columns: mobile ? 1 : 2
                    columnSpacing: 12
                    rowSpacing: 12

                    Button {
                        Layout.fillWidth: true
                        text: controller.authSession.guestMode ? "游客模式已启用" : "进入游客模式"
                        enabled: !controller.authSession.busy && !controller.authSession.guestMode
                        font.pixelSize: mobile ? 18 : 15
                        onClicked: controller.authSession.loginAsGuest()

                        contentItem: Text {
                            text: parent.text
                            color: parent.enabled ? "#102434" : "#6a7882"
                            font: parent.font
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }

                        background: Rectangle {
                            radius: 22
                            color: parent.enabled ? "#dce7ec" : "#d8e0e5"
                            border.color: "#c4d2da"
                            border.width: 1
                        }
                    }

                    Button {
                        Layout.fillWidth: true
                        text: "切换管理员模式"
                        font.pixelSize: mobile ? 18 : 15
                        onClicked: root.adminModeSelected = true

                        contentItem: Text {
                            text: parent.text
                            color: "#173042"
                            font: parent.font
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }

                        background: Rectangle {
                            radius: 22
                            color: "#eef3f5"
                            border.color: "#c9d4db"
                            border.width: 1
                        }
                    }
                }
            }
        }
    }
}
