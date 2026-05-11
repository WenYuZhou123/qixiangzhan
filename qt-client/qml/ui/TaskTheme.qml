import QtQuick

QtObject {
    readonly property color shellTop: "#111821"
    readonly property color shellMid: "#172333"
    readonly property color shellBottom: "#202b37"
    readonly property color navSurface: "#151f2b"
    readonly property color pageSurface: "#eef2f5"
    readonly property color surfacePrimary: "#ffffff"
    readonly property color surfaceSecondary: "#f6f8fa"
    readonly property color surfaceTint: "#e8eef2"
    readonly property color glassStrong: "#e7eef3"
    readonly property color glassSoft: "#f8fafb"
    readonly property color borderStrong: "#526675"
    readonly property color borderSoft: "#ccd6dd"
    readonly property color textPrimary: "#f3f7fa"
    readonly property color textBody: "#182631"
    readonly property color textMuted: "#637381"
    readonly property color accentCyan: "#58a6b4"
    readonly property color accentCyanDeep: "#256a7a"
    readonly property color accentOrange: "#b97837"
    readonly property color accentSand: "#e7ded2"
    readonly property color success: "#227a55"
    readonly property color warning: "#b7791f"
    readonly property color danger: "#b23a48"
    readonly property color offline: "#7a8792"
    readonly property color pending: "#2f6fb2"
    readonly property color neutral: "#dce4e9"

    readonly property int breakpointCompact: 520
    readonly property int breakpointTablet: 820
    readonly property int touchTarget: 50
    readonly property int pageMarginMobile: 14
    readonly property int pageMarginDesktop: 18
    readonly property int cardGap: 12
    readonly property int sectionGap: 14

    readonly property real radiusLarge: 10
    readonly property real radiusMedium: 8
    readonly property real radiusSmall: 6

    function heroTitleSize(mobile) {
        return mobile ? 24 : 28
    }

    function pageTitleSize(mobile) {
        return mobile ? 20 : 19
    }

    function bodySize(mobile) {
        return mobile ? 14 : 13
    }

    function labelSize(mobile) {
        return mobile ? 12 : 12
    }

    function actionSize(mobile) {
        return mobile ? 15 : 14
    }

    function statusColor(status) {
        const normalized = (status || "").toString().toLowerCase()
        if (normalized === "ok" || normalized === "online" || normalized === "connected" ||
            normalized === "success" || normalized === "ack" || normalized === "ack_success")
            return success
        if (normalized === "warning" || normalized === "degraded" || normalized === "queued" ||
            normalized === "sent" || normalized === "pending")
            return normalized === "pending" || normalized === "queued" || normalized === "sent" ? pending : warning
        if (normalized === "critical" || normalized === "error" || normalized === "failed" ||
            normalized === "timeout" || normalized === "ack_timeout" || normalized === "offline")
            return normalized === "offline" ? offline : danger
        return offline
    }

    function stateColor(state) {
        switch (state) {
        case "open":
        case "closed":
            return success
        case "opening":
        case "closing":
            return pending
        case "stopped":
            return warning
        case "fault":
            return danger
        default:
            return offline
        }
    }

    function readinessColor(ready) {
        return ready ? success : warning
    }

    function relayStateText(enabled) {
        return enabled ? "开启" : "关闭"
    }

    function occupancyText(occupied) {
        return occupied ? "占用" : "空闲"
    }

    function connectionStateText(state) {
        switch (state) {
        case "Connected":
        case "Cloud Live":
            return "实时在线"
        case "Connecting":
            return "连接中"
        case "Disconnected":
            return "未连接"
        case "Cloud Ready":
        case "Remote ready":
            return "接口就绪"
        case "Cloud Fallback":
        case "Polling":
            return "轮询同步"
        case "Cloud Error":
            return "接口异常"
        case "Cloud Parse Error":
            return "解析错误"
        case "Authentication Required":
            return "需要登录"
        case "API idle":
            return "接口空闲"
        case "Guest Preview":
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
            return "操作员"
        case "maintenance":
            return "维护员"
        default:
            return role && role.length > 0 ? role : "访客"
        }
    }

    function protocolText(profile) {
        return profile === "airport_pad_v1" ? "停机坪协议" : "继电器协议"
    }

    function padStateText(state) {
        switch (state) {
        case "open":
            return "已开启"
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
}
