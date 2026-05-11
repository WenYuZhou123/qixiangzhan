import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "ui"

ApplicationWindow {
    id: window
    width: 1440
    height: 900
    minimumWidth: androidMode ? 360 : 1024
    minimumHeight: androidMode ? 640 : 700
    visible: true
    title: "气象站边缘主站"
    font.family: Qt.platform.os === "android" ? "Noto Sans CJK SC" : "Microsoft YaHei UI"

    TaskTheme { id: theme }

    readonly property bool androidMode: Qt.platform.os === "android"
    readonly property bool compactDesktop: !androidMode && width < 1280

    property string currentPage: "overview"
    property var navItems: [
        { key: "overview", label: "概览", badge: "OV" },
        { key: "pad", label: "控制", badge: "CT" },
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
        { key: "overview", label: "概览", badge: "OV" },
        { key: "pad", label: "控制", badge: "CT" },
        { key: "weather", label: "气象", badge: "WX" },
        { key: "alarms", label: "告警", badge: "AL" },
        { key: "more", label: "更多", badge: "MO" }
    ]
    property var mobileNavItemsLoggedOut: [
        { key: "login", label: "登录", badge: "AU" },
        { key: "overview", label: "概览", badge: "OV" },
        { key: "weather", label: "气象", badge: "WX" },
        { key: "settings", label: "设置", badge: "CF" },
        { key: "more", label: "更多", badge: "MO" }
    ]

    function pageIndex() {
        switch (currentPage) {
        case "overview": return 0
        case "pad": return 1
        case "weather": return 2
        case "history": return 3
        case "alarms": return 4
        case "settings": return 5
        case "users": return 6
        case "engineering": return 7
        case "serial": return 8
        case "logs": return 9
        case "login": return 10
        default: return 0
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
            return "游客预览"
        if (appController.authSession.authenticated)
            return appController.authSession.displayName.length > 0
                    ? appController.authSession.displayName
                    : appController.authSession.username
        return "未登录"
    }

    function statusChipText() {
        return theme.connectionStateText(appController.runtimeConnectionState)
    }

    function statusChipColor() {
        if (appController.authSession.guestMode)
            return theme.warning
        return appController.runtimeConnected ? theme.success : theme.danger
    }

    function clampPageForCurrentMode() {
        if (!androidMode)
            return

        const desktopOnlyPages = ["users", "engineering", "serial", "logs"]
        if (desktopOnlyPages.indexOf(currentPage) >= 0)
            currentPage = "overview"
    }

    Component.onCompleted: {
        appController.androidMode = androidMode
        if (androidMode && !appController.authSession.authenticated)
            currentPage = "login"
        appController.currentPage = currentPage
    }

    onActiveChanged: {
        if (active && appController.authSession.authenticated)
            appController.remoteSyncService.refreshVisibleData()
    }

    onCurrentPageChanged: {
        const engineeringOnlyPages = ["engineering", "serial", "logs"]
        if (engineeringOnlyPages.indexOf(currentPage) >= 0 && !appController.engineeringMode)
            currentPage = "overview"
        if (currentPage === "users" && !appController.authSession.admin)
            currentPage = "overview"
        clampPageForCurrentMode()
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

    background: Rectangle {
        color: androidMode ? theme.pageSurface : theme.shellTop
    }

    header: Rectangle {
        implicitHeight: androidMode ? 82 : 72
        color: theme.navSurface
        border.color: "#253241"
        border.width: 1

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: androidMode ? 12 : 18
            anchors.rightMargin: androidMode ? 12 : 18
            spacing: 10

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2

                Label {
                    Layout.fillWidth: true
                    text: "气象站边缘主站"
                    color: theme.textPrimary
                    font.pixelSize: androidMode ? 20 : 24
                    font.bold: true
                    elide: Text.ElideRight
                }

                Label {
                    Layout.fillWidth: true
                    text: appController.deviceRepository.currentDeviceId.length > 0
                          ? "当前设备 " + appController.deviceRepository.currentDeviceId
                          : "等待设备上线并同步状态"
                    color: "#b9c7d1"
                    font.pixelSize: androidMode ? 12 : 13
                    elide: Text.ElideRight
                }
            }

            StatusPill {
                textLabel: window.authChipText()
                fill: appController.authSession.authenticated ? theme.pending : theme.offline

                MouseArea {
                    anchors.fill: parent
                    onClicked: window.currentPage = "login"
                }
            }

            StatusPill {
                textLabel: window.statusChipText()
                fill: window.statusChipColor()
            }
        }
    }

    footer: Item {
        visible: androidMode
        implicitHeight: 86

        Rectangle {
            anchors.fill: parent
            color: theme.navSurface
            border.color: "#253241"
            border.width: 1
        }

        RowLayout {
            anchors.fill: parent
            anchors.margins: 8
            spacing: 6

            Repeater {
                model: window.mobileNavigationModel()

                delegate: Item {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    readonly property bool active: window.currentPage === modelData.key
                                                   || (modelData.key === "more" && ["history", "settings"].indexOf(window.currentPage) >= 0)

                    Rectangle {
                        anchors.fill: parent
                        radius: theme.radiusMedium
                        color: active ? theme.glassStrong : "transparent"
                        border.color: active ? theme.accentCyan : "#334251"
                        border.width: 1

                        Column {
                            anchors.centerIn: parent
                            spacing: 4

                            Label {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: modelData.badge
                                color: active ? theme.accentCyanDeep : "#d8e1e7"
                                font.pixelSize: 11
                                font.bold: true
                            }

                            Label {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: modelData.label
                                color: active ? theme.textBody : "#d8e1e7"
                                font.pixelSize: 12
                                font.bold: true
                            }
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            if (modelData.key === "more")
                                morePopup.open()
                            else
                                window.currentPage = modelData.key
                        }
                    }
                }
            }
        }

        Popup {
            id: morePopup
            modal: true
            focus: true
            y: -contentItem.implicitHeight - 12
            x: Math.max(12, parent.width - width - 12)
            width: Math.min(260, parent.width - 24)
            padding: 10

            background: Rectangle {
                radius: theme.radiusMedium
                color: theme.surfacePrimary
                border.color: theme.borderSoft
                border.width: 1
            }

            ColumnLayout {
                spacing: 8

                OpsButton {
                    Layout.fillWidth: true
                    text: "命令历史"
                    onClicked: {
                        morePopup.close()
                        window.currentPage = "history"
                    }
                }

                OpsButton {
                    Layout.fillWidth: true
                    text: "平台设置"
                    onClicked: {
                        morePopup.close()
                        window.currentPage = "settings"
                    }
                }

                OpsButton {
                    Layout.fillWidth: true
                    text: appController.authSession.authenticated ? "账号" : "登录"
                    onClicked: {
                        morePopup.close()
                        window.currentPage = "login"
                    }
                }
            }
        }
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: androidMode ? 0 : 14
        spacing: androidMode ? 0 : 14

        Rectangle {
            visible: !androidMode
            Layout.preferredWidth: compactDesktop ? 188 : 218
            Layout.fillHeight: true
            radius: theme.radiusLarge
            color: theme.navSurface
            border.color: "#253241"
            border.width: 1

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 8

                Label {
                    Layout.fillWidth: true
                    text: "任务导航"
                    color: theme.textPrimary
                    font.pixelSize: 18
                    font.bold: true
                }

                Repeater {
                    model: window.navItems

                    delegate: Item {
                        visible: window.navItemVisible(modelData)
                        Layout.fillWidth: true
                        Layout.preferredHeight: visible ? 44 : 0

                        Rectangle {
                            anchors.fill: parent
                            radius: theme.radiusSmall
                            color: window.currentPage === modelData.key ? theme.glassStrong : "transparent"
                            border.color: window.currentPage === modelData.key ? theme.accentCyan : "#2e3c4b"
                            border.width: 1

                            RowLayout {
                                anchors.fill: parent
                                anchors.margins: 9
                                spacing: 8

                                Label {
                                    text: modelData.badge
                                    color: window.currentPage === modelData.key ? theme.accentCyanDeep : "#b9c7d1"
                                    font.pixelSize: 11
                                    font.bold: true
                                }

                                Label {
                                    Layout.fillWidth: true
                                    text: modelData.label
                                    color: window.currentPage === modelData.key ? theme.textBody : theme.textPrimary
                                    font.pixelSize: 14
                                    font.bold: true
                                    elide: Text.ElideRight
                                }
                            }
                        }

                        MouseArea {
                            anchors.fill: parent
                            onClicked: window.currentPage = modelData.key
                        }
                    }
                }

                Item { Layout.fillHeight: true }

                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: 108
                    radius: theme.radiusMedium
                    color: "#101923"
                    border.color: "#314252"
                    border.width: 1

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 10
                        spacing: 6

                        Label {
                            Layout.fillWidth: true
                            text: appController.engineeringMode ? "工程模式" : "远程运维"
                            color: theme.textPrimary
                            font.pixelSize: 14
                            font.bold: true
                        }

                        Label {
                            Layout.fillWidth: true
                            text: appController.remoteSyncService.healthDeviceSummary
                            color: "#b9c7d1"
                            font.pixelSize: 12
                            elide: Text.ElideRight
                        }

                        Label {
                            Layout.fillWidth: true
                            text: "本地缓存 " + appController.localDatabasePath
                            wrapMode: Text.WrapAnywhere
                            maximumLineCount: 2
                            color: "#9fb0bc"
                            font.pixelSize: 10
                        }
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: androidMode ? 0 : theme.radiusLarge
            color: theme.pageSurface
            border.color: androidMode ? "transparent" : theme.borderSoft
            border.width: androidMode ? 0 : 1
            clip: true

            StackLayout {
                anchors.fill: parent
                anchors.margins: androidMode ? 10 : 14
                currentIndex: window.pageIndex()

                DeviceOverviewPage { controller: appController }
                ControlPage { controller: appController }
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
