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
        implicitHeight: 128

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

    component SectionCard: Rectangle {
        Layout.fillWidth: true
        radius: theme.radiusLarge
        color: "#fbfcfd"
        border.color: theme.borderSoft
        border.width: 1
    }

    component StatePill: Rectangle {
        required property string textLabel
        required property color fill

        implicitHeight: 34
        implicitWidth: textItem.implicitWidth + 28
        radius: 17
        color: fill
        border.color: "#dbe5ea"
        border.width: 1

        Label {
            id: textItem
            anchors.centerIn: parent
            text: parent.textLabel
            color: "white"
            font.pixelSize: 13
            font.bold: true
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
            implicitHeight: mobile ? 222 : 234

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 20
                spacing: 12

                Label {
                    text: "核心气象"
                    color: theme.textPrimary
                    font.pixelSize: theme.heroTitleSize(mobile)
                    font.bold: true
                }

                Label {
                    width: parent.width
                    text: "这里集中展示风场、空气与雨滴数据，只保留当前板卡已经接入的真实测量项。"
                    color: "#cfdae2"
                    font.pixelSize: theme.bodySize(mobile)
                    wrapMode: Text.Wrap
                }

                RowLayout {
                    spacing: 10

                    StatePill {
                        visible: store.windCapability
                        textLabel: "风场已接入"
                        fill: theme.accentCyanDeep
                    }

                    StatePill {
                        visible: store.airCapability
                        textLabel: "空气已接入"
                        fill: theme.success
                    }

                    StatePill {
                        visible: store.rainCapability
                        textLabel: store.rainDetected ? "雨滴已检测" : "雨滴在线"
                        fill: store.rainDetected ? theme.warning : theme.accentOrange
                    }
                }

                GridLayout {
                    Layout.fillWidth: true
                    columns: phone ? 2 : 4
                    columnSpacing: 10
                    rowSpacing: 10

                    StatCard {
                        visible: store.windCapability
                        title: "风速"
                        value: Number(store.windSpeed).toFixed(1)
                        unit: "m/s"
                        note: "主控与 LCD 共用同一风速口径"
                    }

                    StatCard {
                        visible: store.windCapability
                        title: "风向"
                        value: Number(store.windDirection).toFixed(0)
                        unit: "°"
                        note: store.windDirectionText.length > 0 ? store.windDirectionText : "风向文本待回传"
                    }

                    StatCard {
                        visible: store.airCapability
                        title: "温度"
                        value: Number(store.temperature).toFixed(1)
                        unit: "°C"
                        note: "空气模块温度"
                    }

                    StatCard {
                        visible: store.airCapability
                        title: "湿度"
                        value: Number(store.humidity).toFixed(0)
                        unit: "%"
                        note: "空气模块湿度"
                    }
                }
            }
        }

        SectionCard {
            visible: store.airCapability
            implicitHeight: airBody.implicitHeight + 36

            ColumnLayout {
                id: airBody
                anchors.fill: parent
                anchors.margins: 18
                spacing: 12

                Label {
                    text: "空气"
                    color: theme.textBody
                    font.pixelSize: theme.pageTitleSize(mobile)
                    font.bold: true
                }

                Label {
                    width: parent.width
                    text: "颗粒物、二氧化碳和挥发性气体统一归到空气组，减少主页面的信息噪声。"
                    wrapMode: Text.Wrap
                    color: theme.textMuted
                    font.pixelSize: theme.bodySize(mobile)
                }

                GridLayout {
                    Layout.fillWidth: true
                    columns: mobile ? 1 : 3
                    columnSpacing: 12
                    rowSpacing: 12

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
                        note: "挥发性有机物与甲醛"
                    }
                }
            }
        }

        SectionCard {
            implicitHeight: rainBody.implicitHeight + 36

            ColumnLayout {
                id: rainBody
                anchors.fill: parent
                anchors.margins: 18
                spacing: 12

                Label {
                    text: "雨滴与原始输入"
                    color: theme.textBody
                    font.pixelSize: theme.pageTitleSize(mobile)
                    font.bold: true
                }

                GridLayout {
                    Layout.fillWidth: true
                    columns: mobile ? 1 : 2
                    columnSpacing: 12
                    rowSpacing: 12

                    Rectangle {
                        Layout.fillWidth: true
                        radius: theme.radiusMedium
                        color: theme.surfaceSecondary
                        border.color: theme.borderSoft
                        border.width: 1
                        implicitHeight: 210

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 16
                            spacing: 10

                            Label {
                                text: "雨滴状态"
                                color: theme.textBody
                                font.pixelSize: 18
                                font.bold: true
                            }

                            Label {
                                text: store.rainCapability
                                      ? (store.rainDetected ? "已检测到雨滴" : "未检测到雨滴")
                                      : "当前设备未接入雨滴模块"
                                color: theme.textBody
                                font.pixelSize: 22
                                font.bold: true
                                wrapMode: Text.Wrap
                            }

                            Label {
                                text: store.rainCapability
                                      ? "湿润度 " + Number(store.rainValue).toFixed(0) + "% · ADC " + store.rainAdcRaw
                                      : "主页面已自动隐藏未接入的测量项"
                                color: theme.textMuted
                                font.pixelSize: 13
                                wrapMode: Text.Wrap
                            }

                            Label {
                                text: store.rainLevelText.length > 0 ? store.rainLevelText : "雨滴等级文本待更新"
                                color: theme.textMuted
                                font.pixelSize: 12
                            }
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        radius: theme.radiusMedium
                        gradient: Gradient {
                            GradientStop { position: 0.0; color: "#0d1c29" }
                            GradientStop { position: 1.0; color: "#20394c" }
                        }
                        border.color: theme.borderStrong
                        border.width: 1
                        implicitHeight: 210

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
                            Label { text: "风向文本"; color: "#bfd2de"; font.pixelSize: 12 }
                            Label {
                                text: store.windDirectionText.length > 0 ? store.windDirectionText : "--"
                                color: theme.textPrimary
                                font.pixelSize: 14
                                font.bold: true
                            }
                            Label { text: "雨滴等级"; color: "#bfd2de"; font.pixelSize: 12 }
                            Label {
                                text: store.rainLevelText.length > 0 ? store.rainLevelText : "--"
                                color: theme.textPrimary
                                font.pixelSize: 14
                                font.bold: true
                            }
                        }
                    }
                }
            }
        }
    }
}
