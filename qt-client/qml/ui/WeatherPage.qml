import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ScrollView {
    id: root
    property var controller
    readonly property var store: controller.realtimeGateway.stateStore
    readonly property bool mobile: controller.androidMode || width < 1080

    clip: true

    TaskTheme { id: theme }

    component StatCard: Rectangle {
        required property string title
        required property string value
        required property string unit
        required property string note
        Layout.fillWidth: true
        radius: theme.radiusMedium
        color: theme.glassSoft
        border.color: theme.borderSoft
        border.width: 1
        implicitHeight: 132

        Column {
            anchors.fill: parent
            anchors.margins: 16
            spacing: 6

            Label {
                text: parent.parent.title
                color: theme.textMuted
                font.pixelSize: 12
            }

            Row {
                spacing: 4

                Label {
                    text: parent.parent.value
                    color: theme.textBody
                    font.pixelSize: 28
                    font.bold: true
                }

                Label {
                    anchors.verticalCenter: parent.verticalCenter
                    text: parent.parent.unit
                    color: theme.textMuted
                    font.pixelSize: 14
                }
            }

            Label {
                width: parent.width
                text: parent.parent.note
                wrapMode: Text.Wrap
                color: theme.textMuted
                font.pixelSize: 12
            }
        }
    }

    ColumnLayout {
        width: root.availableWidth
        spacing: 14

        Rectangle {
            Layout.fillWidth: true
            radius: theme.radiusLarge
            gradient: Gradient {
                GradientStop { position: 0.0; color: theme.shellTop }
                GradientStop { position: 0.65; color: theme.shellMid }
                GradientStop { position: 1.0; color: "#294257" }
            }
            border.color: theme.borderStrong
            border.width: 1
            implicitHeight: root.mobile ? 220 : 230

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 20
                spacing: 12

                RowLayout {
                    Layout.fillWidth: true

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 4

                        Label {
                            text: "机场核心气象"
                            color: theme.textPrimary
                            font.pixelSize: root.mobile ? 28 : 34
                            font.bold: true
                        }

                        Label {
                            text: "适航建议由风速、能见度等核心指标实时归纳。"
                            color: "#cfdae2"
                            font.pixelSize: 14
                        }
                    }

                    Rectangle {
                        radius: 18
                        implicitWidth: 120
                        implicitHeight: 40
                        color: theme.flightRuleColor(store.windSpeed, store.visibility)
                        border.color: "#d9e5ea"
                        border.width: 1

                        Label {
                            anchors.centerIn: parent
                            text: theme.flightRuleText(store.windSpeed, store.visibility)
                            color: "white"
                            font.pixelSize: 16
                            font.bold: true
                        }
                    }
                }

                GridLayout {
                    Layout.fillWidth: true
                    columns: root.mobile ? 2 : 4
                    columnSpacing: 10
                    rowSpacing: 10

                    StatCard {
                        title: "风速"
                        value: Number(store.windSpeed).toFixed(1)
                        unit: "m/s"
                        note: "风向 " + Number(store.windDirection).toFixed(0) + "°"
                    }
                    StatCard {
                        title: "温度"
                        value: Number(store.temperature).toFixed(1)
                        unit: "°C"
                        note: "湿度 " + Number(store.humidity).toFixed(0) + "%"
                    }
                    StatCard {
                        title: "气压"
                        value: Number(store.pressure).toFixed(1)
                        unit: "hPa"
                        note: "监测面气压"
                    }
                    StatCard {
                        title: "能见度"
                        value: Number(store.visibility).toFixed(1)
                        unit: "km"
                        note: "起降可视环境"
                    }

                    StatCard {
                        title: "雨滴"
                        value: store.rainDetected ? "有雨" : "无雨"
                        unit: ""
                        note: "雨量值 " + Number(store.rainValue).toFixed(1)
                    }

                    StatCard {
                        title: "PM2.5 / PM10"
                        value: Number(store.pm25).toFixed(0) + " / " + Number(store.pm10).toFixed(0)
                        unit: "ug/m3"
                        note: "颗粒物浓度"
                    }

                    StatCard {
                        title: "CO2"
                        value: Number(store.co2).toFixed(0)
                        unit: "ppm"
                        note: "空气质量核心指标"
                    }

                    StatCard {
                        title: "TVOC / CH2O"
                        value: Number(store.tvoc).toFixed(3) + " / " + Number(store.ch2o).toFixed(3)
                        unit: "mg/m3"
                        note: "挥发物 / 甲醛"
                    }
                }
            }
        }

        GridLayout {
            Layout.fillWidth: true
            columns: root.mobile ? 1 : 2
            columnSpacing: 14
            rowSpacing: 14

            Rectangle {
                Layout.fillWidth: true
                radius: theme.radiusLarge
                color: "#fbfcfd"
                border.color: theme.borderSoft
                border.width: 1
                implicitHeight: 250

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 18
                    spacing: 12

                    Label {
                        text: "适航评估"
                        color: theme.textBody
                        font.pixelSize: 24
                        font.bold: true
                    }

                    Label {
                        width: parent.width
                        text: theme.flightRuleText(store.windSpeed, store.visibility) === "适航"
                              ? "当前风速与能见度处于适航区间，可继续执行停机场任务。"
                              : theme.flightRuleText(store.windSpeed, store.visibility) === "谨慎"
                                ? "建议在人工确认后执行起降任务，注意阵风与视程变化。"
                                : "当前条件不建议执行无人机起降，优先保持停机场安全闭合。"
                        wrapMode: Text.Wrap
                        color: theme.textMuted
                        font.pixelSize: 13
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        radius: theme.radiusMedium
                        color: "#eff4f7"
                        border.color: theme.borderSoft
                        border.width: 1

                        GridLayout {
                            anchors.fill: parent
                            anchors.margins: 14
                            columns: 2
                            rowSpacing: 10
                            columnSpacing: 10

                            Label { text: "风速"; color: theme.textMuted; font.pixelSize: 12 }
                            Label { text: Number(store.windSpeed).toFixed(1) + " m/s"; color: theme.textBody; font.pixelSize: 14; font.bold: true }
                            Label { text: "风向"; color: theme.textMuted; font.pixelSize: 12 }
                            Label { text: Number(store.windDirection).toFixed(0) + "°"; color: theme.textBody; font.pixelSize: 14; font.bold: true }
                            Label { text: "能见度"; color: theme.textMuted; font.pixelSize: 12 }
                            Label { text: Number(store.visibility).toFixed(1) + " km"; color: theme.textBody; font.pixelSize: 14; font.bold: true }
                            Label { text: "雨滴"; color: theme.textMuted; font.pixelSize: 12 }
                            Label { text: store.rainDetected ? "检测到" : "未检测到"; color: theme.textBody; font.pixelSize: 14; font.bold: true }
                            Label { text: "PM2.5"; color: theme.textMuted; font.pixelSize: 12 }
                            Label { text: Number(store.pm25).toFixed(0) + " ug/m3"; color: theme.textBody; font.pixelSize: 14; font.bold: true }
                            Label { text: "PM10"; color: theme.textMuted; font.pixelSize: 12 }
                            Label { text: Number(store.pm10).toFixed(0) + " ug/m3"; color: theme.textBody; font.pixelSize: 14; font.bold: true }
                            Label { text: "CO2"; color: theme.textMuted; font.pixelSize: 12 }
                            Label { text: Number(store.co2).toFixed(0) + " ppm"; color: theme.textBody; font.pixelSize: 14; font.bold: true }
                            Label { text: "TVOC"; color: theme.textMuted; font.pixelSize: 12 }
                            Label { text: Number(store.tvoc).toFixed(3) + " mg/m3"; color: theme.textBody; font.pixelSize: 14; font.bold: true }
                            Label { text: "CH2O"; color: theme.textMuted; font.pixelSize: 12 }
                            Label { text: Number(store.ch2o).toFixed(3) + " mg/m3"; color: theme.textBody; font.pixelSize: 14; font.bold: true }
                            Label { text: "停机场模式"; color: theme.textMuted; font.pixelSize: 12 }
                            Label { text: store.padMode; color: theme.textBody; font.pixelSize: 14; font.bold: true }
                        }
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                radius: theme.radiusLarge
                color: "#fbfcfd"
                border.color: theme.borderSoft
                border.width: 1
                implicitHeight: 250

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 18
                    spacing: 12

                    Label {
                        text: "航线与相机预留"
                        color: theme.textBody
                        font.pixelSize: 24
                        font.bold: true
                    }

                    Label {
                        width: parent.width
                        text: "后续会接入无人机航迹、相机回传、地图等信息层。当前阶段先预留视觉位与数据结构。"
                        wrapMode: Text.Wrap
                        color: theme.textMuted
                        font.pixelSize: 13
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        radius: theme.radiusMedium
                        gradient: Gradient {
                            GradientStop { position: 0.0; color: "#0d1c29" }
                            GradientStop { position: 1.0; color: "#20394c" }
                        }
                        border.color: theme.borderStrong
                        border.width: 1

                        Column {
                            anchors.centerIn: parent
                            spacing: 8

                            Label {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: "Drone Route / Camera Feed"
                                color: theme.textPrimary
                                font.pixelSize: 18
                                font.bold: true
                            }

                            Label {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: "即将接入"
                                color: "#bfd2de"
                                font.pixelSize: 13
                            }
                        }
                    }
                }
            }
        }
    }
}
