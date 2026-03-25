import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ScrollView {
    id: root
    property var controller
    readonly property var store: controller.realtimeGateway.stateStore
    readonly property bool mobile: controller.androidMode || width < 1080
    readonly property bool phone: width < theme.breakpointCompact || controller.androidMode

    clip: true

    TaskTheme { id: theme }

    component StatCard: Rectangle {
        required property string title
        required property string value
        required property string unit
        required property string note
        Layout.fillWidth: true
        radius: theme.radiusMedium
        color: theme.surfacePrimary
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
                font.pixelSize: theme.labelSize(root.mobile)
            }

            Row {
                spacing: 4

                Label {
                    text: parent.parent.value
                    color: theme.textBody
                    font.pixelSize: root.mobile ? 28 : 30
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
        spacing: theme.sectionGap

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
            implicitHeight: mobile ? 224 : 232

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
                            text: "核心气象"
                            color: theme.textPrimary
                            font.pixelSize: theme.heroTitleSize(mobile)
                            font.bold: true
                        }

                        Label {
                            text: "统一汇总风速、能见度、空气质量和雨滴状态，为控制页和 LCD 提供同样的气象判断依据。"
                            color: "#cfdae2"
                            font.pixelSize: theme.bodySize(mobile)
                            wrapMode: Text.Wrap
                        }
                    }

                    Rectangle {
                        radius: 18
                        implicitWidth: 124
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
                    columns: phone ? 2 : 4
                    columnSpacing: 10
                    rowSpacing: 10

                    StatCard {
                        title: "风速"
                        value: Number(store.windSpeed).toFixed(1)
                        unit: "m/s"
                        note: (store.windDirectionText.length > 0 ? store.windDirectionText + "  " : "")
                              + Number(store.windDirection).toFixed(0) + "°"
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
                        note: "地面气压"
                    }

                    StatCard {
                        title: "能见度"
                        value: Number(store.visibility).toFixed(1)
                        unit: "km"
                        note: "飞行可视条件"
                    }
                }
            }
        }

        GridLayout {
            Layout.fillWidth: true
            columns: phone ? 2 : 4
            columnSpacing: 12
            rowSpacing: 12

            StatCard {
                title: "雨滴"
                value: store.rainLevelText.length > 0 ? store.rainLevelText : (store.rainDetected ? "有雨" : "无雨")
                unit: ""
                note: "湿润度 " + Number(store.rainValue).toFixed(0) + "% · ADC " + store.rainAdcRaw
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
                note: "挥发物与甲醛"
            }
        }

        GridLayout {
            Layout.fillWidth: true
            columns: mobile ? 1 : 2
            columnSpacing: theme.sectionGap
            rowSpacing: theme.sectionGap

            Rectangle {
                Layout.fillWidth: true
                radius: theme.radiusLarge
                color: "#fbfcfd"
                border.color: theme.borderSoft
                border.width: 1
                implicitHeight: 262

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 18
                    spacing: 12

                    Label {
                        text: "适航评估"
                        color: theme.textBody
                        font.pixelSize: theme.pageTitleSize(mobile)
                        font.bold: true
                    }

                    Label {
                        width: parent.width
                        text: theme.flightRuleText(store.windSpeed, store.visibility) === "适航"
                              ? "当前风速与能见度处于适航区间，可继续执行停机坪任务。"
                              : theme.flightRuleText(store.windSpeed, store.visibility) === "谨慎"
                                ? "建议人工确认后再执行动作，重点关注阵风和能见度变化。"
                                : "当前条件不建议继续执行无人机起降或停机坪联动，优先保持安全状态。"
                        wrapMode: Text.Wrap
                        color: theme.textMuted
                        font.pixelSize: theme.bodySize(mobile)
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
                            Label { text: (store.windDirectionText.length > 0 ? store.windDirectionText + "  " : "") + Number(store.windDirection).toFixed(0) + "°"; color: theme.textBody; font.pixelSize: 14; font.bold: true }
                            Label { text: "能见度"; color: theme.textMuted; font.pixelSize: 12 }
                            Label { text: Number(store.visibility).toFixed(1) + " km"; color: theme.textBody; font.pixelSize: 14; font.bold: true }
                            Label { text: "雨滴"; color: theme.textMuted; font.pixelSize: 12 }
                            Label { text: store.rainDetected ? "检测到" : "未检测到"; color: theme.textBody; font.pixelSize: 14; font.bold: true }
                            Label { text: "停机模式"; color: theme.textMuted; font.pixelSize: 12 }
                            Label { text: theme.padModeText(store.padMode); color: theme.textBody; font.pixelSize: 14; font.bold: true }
                            Label { text: "适航结论"; color: theme.textMuted; font.pixelSize: 12 }
                            Label { text: theme.flightRuleText(store.windSpeed, store.visibility); color: theme.flightRuleColor(store.windSpeed, store.visibility); font.pixelSize: 14; font.bold: true }
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
                implicitHeight: 262

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 18
                    spacing: 12

                    Label {
                        text: "传感器原始输入"
                        color: theme.textBody
                        font.pixelSize: theme.pageTitleSize(mobile)
                        font.bold: true
                    }

                    Label {
                        width: parent.width
                        text: "原始 ADC、风传感器报文和雨滴等级在这里集中查看，便于桌面、安卓和本地屏统一联调。"
                        wrapMode: Text.Wrap
                        color: theme.textMuted
                        font.pixelSize: theme.bodySize(mobile)
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

                        GridLayout {
                            anchors.fill: parent
                            anchors.margins: 14
                            columns: 2
                            rowSpacing: 10
                            columnSpacing: 10

                            Label { text: "风速 ADC"; color: "#bfd2de"; font.pixelSize: 12 }
                            Label { text: String(store.windSpeedRaw); color: theme.textPrimary; font.pixelSize: 14; font.bold: true }
                            Label { text: "风向 ADC"; color: "#bfd2de"; font.pixelSize: 12 }
                            Label { text: String(store.windDirectionRaw); color: theme.textPrimary; font.pixelSize: 14; font.bold: true }
                            Label { text: "雨滴 ADC"; color: "#bfd2de"; font.pixelSize: 12 }
                            Label { text: String(store.rainAdcRaw); color: theme.textPrimary; font.pixelSize: 14; font.bold: true }
                            Label { text: "PM2.5"; color: "#bfd2de"; font.pixelSize: 12 }
                            Label { text: Number(store.pm25).toFixed(0) + " ug/m3"; color: theme.textPrimary; font.pixelSize: 14; font.bold: true }
                            Label { text: "TVOC"; color: "#bfd2de"; font.pixelSize: 12 }
                            Label { text: Number(store.tvoc).toFixed(3) + " mg/m3"; color: theme.textPrimary; font.pixelSize: 14; font.bold: true }
                            Label { text: "CH2O"; color: "#bfd2de"; font.pixelSize: 12 }
                            Label { text: Number(store.ch2o).toFixed(3) + " mg/m3"; color: theme.textPrimary; font.pixelSize: 14; font.bold: true }
                        }
                    }
                }
            }
        }
    }
}
