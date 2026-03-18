import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "ui"

ApplicationWindow {
    id: window
    width: 1520
    height: 940
    minimumWidth: 1180
    minimumHeight: 760
    visible: true
    title: "机场任务终端"
    font.family: Qt.platform.os === "android" ? "Noto Sans CJK SC" : "Microsoft YaHei UI"

    TaskTheme { id: theme }

    readonly property bool androidMode: Qt.platform.os === "android"
    readonly property bool compactDesktop: !androidMode && width < 1360

    property string currentPage: "overview"
    property var navItems: [
        { key: "overview", label: "总览", badge: "OV" },
        { key: "pad", label: "停机场", badge: "PD" },
        { key: "weather", label: "气象", badge: "WX" },
        { key: "history", label: "历史", badge: "HS" },
        { key: "alarms", label: "告警", badge: "AL" },
        { key: "settings", label: "设置", badge: "CF" },
        { key: "users", label: "用户", badge: "US", adminOnly: true },
        { key: "engineering", label: "工程", badge: "EG", engineeringOnly: true },
        { key: "serial", label: "串口", badge: "UA", engineeringOnly: true },
        { key: "logs", label: "日志", badge: "LG", engineeringOnly: true },
        { key: "login", label: "登录", badge: "AU" }
    ]
    property var mobileNavItems: [
        { key: "overview", label: "总览", badge: "OV" },
        { key: "pad", label: "停机场", badge: "PD" },
        { key: "weather", label: "气象", badge: "WX" },
        { key: "history", label: "历史", badge: "HS" },
        { key: "settings", label: "设置", badge: "CF" }
    ]
    property var mobileNavItemsLoggedOut: [
        { key: "login", label: "登录", badge: "AU" },
        { key: "overview", label: "总览", badge: "OV" },
        { key: "pad", label: "停机场", badge: "PD" },
        { key: "weather", label: "气象", badge: "WX" },
        { key: "settings", label: "设置", badge: "CF" }
    ]

    function pageIndex() {
        switch (currentPage) {
        case "overview":
            return 0
        case "pad":
            return 1
        case "weather":
            return 2
        case "history":
            return 3
        case "alarms":
            return 4
        case "settings":
            return 5
        case "users":
            return 6
        case "engineering":
            return 7
        case "serial":
            return 8
        case "logs":
            return 9
        case "login":
            return 10
        default:
            return 0
        }
    }

    function navItemVisible(item) {
        if (item.adminOnly && !appController.authSession.admin)
            return false
        if (item.engineeringOnly && !appController.engineeringMode)
            return false
        return true
    }

    function mobileNavigationModel() {
        return appController.authSession.authenticated ? mobileNavItems : mobileNavItemsLoggedOut
    }

    function authChipText() {
        if (appController.authSession.guestMode)
            return "游客模式"
        if (appController.authSession.authenticated)
            return appController.authSession.displayName.length > 0
                   ? appController.authSession.displayName
                   : "已登录"
        return "未登录"
    }

    function statusChipText() {
        if (appController.authSession.guestMode)
            return "预览"
        return appController.runtimeConnected ? "在线" : "离线"
    }

    function statusChipColor() {
        if (appController.authSession.guestMode)
            return theme.warning
        return appController.runtimeConnected ? theme.success : theme.danger
    }

    Component.onCompleted: {
        appController.androidMode = androidMode
        if (androidMode && !appController.authSession.authenticated)
            currentPage = "login"
        appController.currentPage = currentPage
    }

    onCurrentPageChanged: {
        const engineeringOnlyPages = ["engineering", "serial", "logs"]
        if (engineeringOnlyPages.indexOf(currentPage) >= 0 && !appController.engineeringMode)
            currentPage = "overview"
        if (currentPage === "users" && !appController.authSession.admin)
            currentPage = "overview"
        appController.currentPage = currentPage
    }

    Connections {
        target: appController.authSession

        function onSessionChanged() {
            if (!androidMode)
                return

            if (!appController.authSession.authenticated) {
                if (currentPage !== "login" && currentPage !== "settings")
                    currentPage = "login"
                return
            }

            if (currentPage === "login")
                currentPage = "overview"
        }
    }

    background: Item {
        Rectangle {
            anchors.fill: parent
            gradient: Gradient {
                GradientStop { position: 0.0; color: theme.shellTop }
                GradientStop { position: 0.35; color: theme.shellMid }
                GradientStop { position: 1.0; color: theme.shellBottom }
            }
        }

        Rectangle {
            width: parent.width * 0.34
            height: parent.height * 0.2
            x: parent.width * 0.44
            y: -height * 0.25
            radius: width / 2
            color: "#24d6e5ed"
        }

        Rectangle {
            width: parent.width * 0.48
            height: parent.height * 0.22
            x: -width * 0.18
            y: parent.height * 0.72
            radius: height / 2
            rotation: -4
            color: "#1fd6c8b3"
        }
    }

    header: Rectangle {
        implicitHeight: androidMode ? 108 : 132
        gradient: Gradient {
            GradientStop { position: 0.0; color: theme.shellTop }
            GradientStop { position: 0.6; color: theme.shellMid }
            GradientStop { position: 1.0; color: "#214055" }
        }
        border.color: theme.borderStrong
        border.width: 1

        ColumnLayout {
            anchors.fill: parent
            anchors.leftMargin: androidMode ? 14 : 24
            anchors.rightMargin: androidMode ? 14 : 24
            anchors.topMargin: androidMode ? 10 : 18
            anchors.bottomMargin: androidMode ? 10 : 14
            spacing: androidMode ? 8 : 12

            RowLayout {
                Layout.fillWidth: true
                spacing: androidMode ? 8 : 12

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2

                    Label {
                        text: "机场任务终端"
                        color: theme.textPrimary
                        font.pixelSize: androidMode ? 18 : 30
                        font.bold: true
                        elide: Text.ElideRight
                    }

                    Label {
                        text: androidMode
                              ? "高原气象 / 停机场联动 / 无人机预留"
                              : "高原气象监测 / 停机场联动 / 无人机任务预留"
                        color: "#c7d6df"
                        font.pixelSize: androidMode ? 10 : 14
                        elide: Text.ElideRight
                    }
                }

                Rectangle {
                    id: authChip
                    radius: androidMode ? 18 : 22
                    Layout.preferredWidth: androidMode ? 118 : 240
                    Layout.preferredHeight: androidMode ? 40 : 46
                    color: appController.authSession.authenticated ? "#274e60" : "#314c5a"
                    border.color: appController.authSession.authenticated ? theme.accentCyan : theme.borderStrong
                    border.width: 1

                    Label {
                        anchors.centerIn: parent
                        width: parent.width - 18
                        text: window.authChipText()
                        color: theme.textPrimary
                        font.pixelSize: androidMode ? 13 : 15
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                        elide: Text.ElideRight
                    }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: window.currentPage = "login"
                    }
                }

                Rectangle {
                    radius: androidMode ? 18 : 22
                    Layout.preferredWidth: androidMode ? 84 : 148
                    Layout.preferredHeight: androidMode ? 40 : 46
                    color: window.statusChipColor()
                    border.color: appController.runtimeConnected ? "#a7c7bd" : "#d7b0b6"
                    border.width: 1

                    Label {
                        anchors.centerIn: parent
                        text: window.statusChipText()
                        color: "white"
                        font.pixelSize: androidMode ? 13 : 15
                        font.bold: true
                    }
                }
            }

            Label {
                Layout.fillWidth: true
                text: appController.deviceRepository.currentDeviceId.length > 0
                      ? "当前设备  " + appController.deviceRepository.currentDeviceId
                      : "等待设备上线并同步状态"
                color: "#d9e4eb"
                font.pixelSize: androidMode ? 12 : 14
                elide: Text.ElideRight
            }
        }
    }

    footer: Item {
        visible: androidMode
        implicitHeight: 94

        Rectangle {
            anchors.fill: parent
            color: theme.shellTop
        }

        Rectangle {
            anchors.fill: parent
            anchors.margins: 10
            radius: 26
            color: "#e8112131"
            border.color: "#37576b"
            border.width: 1

            RowLayout {
                anchors.fill: parent
                anchors.margins: 8
                spacing: 8

                Repeater {
                    model: window.mobileNavigationModel()

                    delegate: Item {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        readonly property bool active: window.currentPage === modelData.key

                        Rectangle {
                            anchors.fill: parent
                            radius: 18
                            color: active ? "#eef4f6" : "#10223342"
                            border.color: active ? theme.accentCyan : "#3b5b6f"
                            border.width: 1
                            opacity: navTap.pressed ? 0.9 : 1.0

                            Column {
                                anchors.centerIn: parent
                                spacing: 4

                                Rectangle {
                                    width: 34
                                    height: 34
                                    radius: 17
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    color: active ? theme.accentCyanDeep : "#173243"

                                    Label {
                                        anchors.centerIn: parent
                                        text: modelData.badge
                                        color: "#eef5f8"
                                        font.pixelSize: 11
                                        font.bold: true
                                        font.letterSpacing: 1
                                    }
                                }

                                Label {
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    text: modelData.label
                                    color: active ? theme.textBody : "#e4edf2"
                                    font.pixelSize: 13
                                    font.bold: true
                                }
                            }
                        }

                        MouseArea {
                            id: navTap
                            anchors.fill: parent
                            onClicked: window.currentPage = modelData.key
                        }
                    }
                }
            }
        }
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: androidMode ? 0 : 18
        spacing: androidMode ? 0 : 18

        Item {
            visible: !androidMode
            Layout.preferredWidth: compactDesktop ? 220 : 252
            Layout.fillHeight: true

            Rectangle {
                anchors.fill: parent
                radius: 32
                color: "#d5122432"
                border.color: "#4a687b"
                border.width: 1
            }

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 16
                spacing: 10

                ColumnLayout {
                    spacing: 2

                    Label {
                        text: "任务导航"
                        color: theme.textPrimary
                        font.pixelSize: 24
                        font.bold: true
                    }

                    Label {
                        text: "高原机场联动控制"
                        color: "#b7cad6"
                        font.pixelSize: 12
                    }
                }

                Repeater {
                    model: window.navItems

                    delegate: Item {
                        visible: window.navItemVisible(modelData)
                        Layout.fillWidth: true
                        Layout.preferredHeight: visible ? 58 : 0

                        Rectangle {
                            anchors.fill: parent
                            radius: 22
                            color: window.currentPage === modelData.key ? theme.glassStrong : "#10000000"
                            border.color: window.currentPage === modelData.key ? theme.accentCyan : "#3d5b6c"
                            border.width: 1
                            scale: navHover.containsMouse ? 1.01 : 1.0

                            Behavior on scale {
                                NumberAnimation { duration: 120 }
                            }

                            RowLayout {
                                anchors.fill: parent
                                anchors.margins: 12
                                spacing: 10

                                Rectangle {
                                    Layout.preferredWidth: 38
                                    Layout.preferredHeight: 38
                                    radius: 14
                                    color: window.currentPage === modelData.key ? theme.accentCyanDeep : "#173243"

                                    Label {
                                        anchors.centerIn: parent
                                        text: modelData.badge
                                        color: "#eef5f8"
                                        font.pixelSize: 10
                                        font.bold: true
                                        font.letterSpacing: 1.1
                                    }
                                }

                                Label {
                                    Layout.fillWidth: true
                                    text: modelData.label
                                    color: window.currentPage === modelData.key ? theme.textBody : theme.textPrimary
                                    font.pixelSize: 16
                                    font.bold: true
                                    verticalAlignment: Text.AlignVCenter
                                }
                            }
                        }

                        MouseArea {
                            id: navHover
                            anchors.fill: parent
                            hoverEnabled: true
                            onClicked: window.currentPage = modelData.key
                        }
                    }
                }

                Item { Layout.fillHeight: true }

                Rectangle {
                    Layout.fillWidth: true
                    radius: 24
                    color: "#14000000"
                    border.color: "#3f5f72"
                    border.width: 1
                    implicitHeight: 138

                    Column {
                        anchors.fill: parent
                        anchors.margins: 14
                        spacing: 8

                        Label {
                            text: appController.engineeringMode ? "工程模式已启用" : "远程任务模式"
                            color: theme.textPrimary
                            font.pixelSize: 16
                            font.bold: true
                        }

                        Label {
                            width: parent.width
                            text: appController.engineeringMode
                                  ? "桌面端保留本地 MQTT、继电器与串口调试能力。"
                                  : "普通运行优先走云端接口与远程同步。"
                            wrapMode: Text.Wrap
                            color: "#bfd1dc"
                            font.pixelSize: 12
                        }

                        Label {
                            width: parent.width
                            text: "本地数据库\n" + appController.localDatabasePath
                            wrapMode: Text.WrapAnywhere
                            color: "#d6e4eb"
                            font.pixelSize: 11
                        }
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: androidMode ? 0 : 34
            color: androidMode ? theme.pageSurface : "#f1f6f8"
            border.color: androidMode ? "transparent" : theme.borderSoft
            border.width: androidMode ? 0 : 1
            clip: true

            StackLayout {
                anchors.fill: parent
                anchors.margins: androidMode ? 12 : 18
                currentIndex: window.pageIndex()

                DeviceOverviewPage { controller: appController }
                DevicePage { controller: appController }
                WeatherPage { controller: appController }
                CommandHistoryPage { controller: appController }
                AlarmCenterPage { controller: appController }
                SettingsPage { controller: appController }
                UserManagementPage { controller: appController }
                EngineeringPage { controller: appController }
                SerialConsolePage {
                    controller: appController
                    visible: !androidMode && appController.engineeringMode
                }
                LogPanel { controller: appController }
                LoginPage { controller: appController }
            }
        }
    }
}
