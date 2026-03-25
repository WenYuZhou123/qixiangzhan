import QtQuick

QtObject {
    readonly property color shellTop: "#0c1823"
    readonly property color shellMid: "#132a39"
    readonly property color shellBottom: "#2a4352"
    readonly property color pageSurface: "#edf3f5"
    readonly property color surfacePrimary: "#fbfdfe"
    readonly property color surfaceSecondary: "#f2f7f9"
    readonly property color surfaceTint: "#dfeaf0"
    readonly property color glassStrong: "#eef4f6"
    readonly property color glassSoft: "#f7fbfc"
    readonly property color borderStrong: "#6b8798"
    readonly property color borderSoft: "#cad7df"
    readonly property color textPrimary: "#f2f6f8"
    readonly property color textBody: "#17303f"
    readonly property color textMuted: "#67808f"
    readonly property color accentCyan: "#7bc1cf"
    readonly property color accentCyanDeep: "#295b6d"
    readonly property color accentOrange: "#c8915f"
    readonly property color accentSand: "#ead9ca"
    readonly property color success: "#4e7d72"
    readonly property color warning: "#9b754f"
    readonly property color danger: "#8d5d64"
    readonly property color neutral: "#d7e1e6"

    readonly property int breakpointCompact: 420
    readonly property int breakpointTablet: 600
    readonly property int touchTarget: 50
    readonly property int pageMarginMobile: 14
    readonly property int pageMarginDesktop: 18
    readonly property int cardGap: 12
    readonly property int sectionGap: 14

    readonly property real radiusLarge: 30
    readonly property real radiusMedium: 22
    readonly property real radiusSmall: 16

    function heroTitleSize(mobile) {
        return mobile ? 28 : 36
    }

    function pageTitleSize(mobile) {
        return mobile ? 24 : 22
    }

    function bodySize(mobile) {
        return mobile ? 14 : 13
    }

    function labelSize(mobile) {
        return mobile ? 13 : 12
    }

    function actionSize(mobile) {
        return mobile ? 16 : 14
    }

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
            return "#617c8a"
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
            return "响应解析错误"
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
