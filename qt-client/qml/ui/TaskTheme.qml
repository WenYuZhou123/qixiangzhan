import QtQuick

QtObject {
    readonly property color shellTop: "#0b1722"
    readonly property color shellMid: "#132634"
    readonly property color shellBottom: "#304451"
    readonly property color glassStrong: "#e6edf1"
    readonly property color glassSoft: "#f6fafc"
    readonly property color glassTint: "#2b4154"
    readonly property color borderStrong: "#6b8798"
    readonly property color borderSoft: "#c8d5dd"
    readonly property color textPrimary: "#f3f1eb"
    readonly property color textBody: "#162b39"
    readonly property color textMuted: "#6b7d8a"
    readonly property color accentCyan: "#78bfd0"
    readonly property color accentCyanDeep: "#2c5b70"
    readonly property color accentOrange: "#c98f5c"
    readonly property color accentOrangeSoft: "#ead9ca"
    readonly property color success: "#597f77"
    readonly property color warning: "#9d754f"
    readonly property color danger: "#8c5f66"
    readonly property color neutral: "#d5dee3"
    readonly property color pageSurface: "#eef3f5"

    readonly property real radiusLarge: 30
    readonly property real radiusMedium: 22
    readonly property real radiusSmall: 16

    function stateColor(state) {
        switch (state) {
        case "open":
            return success
        case "opening":
            return accentCyanDeep
        case "closing":
            return warning
        case "stopped":
            return accentOrange
        case "fault":
            return danger
        default:
            return "#627786"
        }
    }

    function readinessColor(ready) {
        return ready ? success : warning
    }

    function occupancyText(occupied) {
        return occupied ? "已占位" : "空闲"
    }

    function connectionStateText(state) {
        switch (state) {
        case "Connected":
            return "已连接"
        case "Connecting":
            return "连接中"
        case "Disconnected":
            return "未连接"
        case "API idle":
            return "接口空闲"
        case "Polling":
            return "轮询同步"
        case "Remote ready":
            return "远程就绪"
        case "游客预览":
            return "游客预览"
        default:
            return state && state.length > 0 ? state : "未连接"
        }
    }

    function roleText(role) {
        switch (role) {
        case "admin":
            return "管理员"
        case "guest":
            return "游客"
        case "operator":
            return "值守员"
        case "maintenance":
            return "维护员"
        default:
            return role && role.length > 0 ? role : "访客"
        }
    }

    function padStateText(state) {
        switch (state) {
        case "open":
            return "已打开"
        case "opening":
            return "开启中"
        case "closed":
            return "已关闭"
        case "closing":
            return "关闭中"
        case "stopped":
            return "已停止"
        case "fault":
            return "故障"
        default:
            return state && state.length > 0 ? state : "未知"
        }
    }

    function padModeText(mode) {
        switch (mode) {
        case "auto":
            return "自动"
        case "manual":
            return "手动"
        case "maintenance":
            return "维护"
        default:
            return mode && mode.length > 0 ? mode : "未知"
        }
    }

    function flightRuleText(windSpeed, visibility) {
        if (windSpeed > 15 || visibility < 2)
            return "禁飞"
        if (windSpeed > 10 || visibility < 5)
            return "谨慎"
        return "适航"
    }

    function flightRuleColor(windSpeed, visibility) {
        const rule = flightRuleText(windSpeed, visibility)
        if (rule === "禁飞")
            return danger
        if (rule === "谨慎")
            return warning
        return success
    }
}
