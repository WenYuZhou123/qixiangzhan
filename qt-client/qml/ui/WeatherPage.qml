import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ScrollView {
    id: root
    property var controller
    readonly property var store: controller.realtimeGateway.stateStore
    readonly property bool mobile: controller.androidMode || width < 980
    readonly property bool phone: width < theme.breakpointCompact || controller.androidMode

    clip: true

    TaskTheme { id: theme }

    component SectionCard: Rectangle {
        default property alias content: body.data
        property string title: ""
        property string subtitle: ""

        Layout.fillWidth: true
        radius: theme.radiusLarge
        color: theme.surfacePrimary
        border.color: theme.borderSoft
        border.width: 1
        implicitHeight: body.implicitHeight + 24

        ColumnLayout {
            id: body
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 12
            spacing: 10

            Label {
                Layout.fillWidth: true
                text: parent.parent.title
                color: theme.textBody
                font.pixelSize: theme.pageTitleSize(root.mobile)
                font.bold: true
            }

            Label {
                visible: parent.parent.subtitle.length > 0
                Layout.fillWidth: true
                text: parent.parent.subtitle
                color: theme.textMuted
                font.pixelSize: theme.bodySize(root.mobile)
                wrapMode: Text.Wrap
            }
        }
    }

    ColumnLayout {
        width: root.availableWidth
        spacing: theme.sectionGap

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: root.mobile ? 142 : 132
            radius: theme.radiusLarge
            color: theme.navSurface
            border.color: "#2f3d4c"
            border.width: 1

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 16
                spacing: 10

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 3

                        Label {
                            Layout.fillWidth: true
                            text: "气象监测"
                            color: theme.textPrimary
                            font.pixelSize: theme.heroTitleSize(root.mobile)
                            font.bold: true
                            elide: Text.ElideRight
                        }

                        Label {
                            Layout.fillWidth: true
                            text: store.currentDeviceId.length > 0 ? store.currentDeviceId : "尚未选择设备"
                            color: "#b9c7d1"
                            font.pixelSize: 13
                            elide: Text.ElideRight
                        }
                    }

                    StatusPill {
                        textLabel: store.windSensorOnline && store.airSensorOnline && store.rainSensorOnline ? "传感器正常" : "传感器异常"
                        fill: store.windSensorOnline && store.airSensorOnline && store.rainSensorOnline ? theme.success : theme.danger
                    }
                }

                GridLayout {
                    Layout.fillWidth: true
                    columns: root.phone ? 2 : 5
                    columnSpacing: 8
                    rowSpacing: 8

                    StatusPill {
                        visible: store.windCapability
                        textLabel: "风 " + Number(store.windSpeed).toFixed(1) + " m/s"
                        fill: theme.pending
                    }

                    StatusPill {
                        visible: store.rainCapability
                        textLabel: store.rainDetected ? "雨滴已检测" : "无雨滴"
                        fill: store.rainDetected ? theme.warning : theme.success
                    }

                    StatusPill {
                        visible: store.airCapability
                        textLabel: "PM2.5 " + Number(store.pm25).toFixed(0)
                        fill: store.pm25 > 75 ? theme.warning : theme.success
                    }

                    StatusPill {
                        visible: store.airCapability
                        textLabel: "CO2 " + Number(store.co2).toFixed(0)
                        fill: store.co2 > 1000 ? theme.warning : theme.success
                    }

                    StatusPill {
                        textLabel: "最近 " + (store.timestamp.length > 0 ? store.timestamp : "--")
                        fill: theme.offline
                    }
                }
            }
        }

        GridLayout {
            Layout.fillWidth: true
            columns: root.mobile ? 1 : 2
            columnSpacing: theme.sectionGap
            rowSpacing: theme.sectionGap

            SectionCard {
                visible: store.windCapability
                title: "风场"
                subtitle: "用于现场风速判断和停机坪动作前的环境参考。"

                GridLayout {
                    Layout.fillWidth: true
                    columns: root.phone ? 1 : 2
                    columnSpacing: 10
                    rowSpacing: 10

                    MetricTile {
                        mobile: root.mobile
                        title: "风速"
                        value: Number(store.windSpeed).toFixed(1)
                        unit: "m/s"
                        caption: "原始值 " + store.windSpeedRaw
                        fillRatio: Math.min(1, store.windSpeed / 20.0)
                        accent: store.windSpeed > 12 ? theme.warning : theme.pending
                    }

                    MetricTile {
                        mobile: root.mobile
                        title: "风向"
                        value: Number(store.windDirection).toFixed(0)
                        unit: "deg"
                        caption: store.windDirectionText.length > 0 ? store.windDirectionText : "风向文本待更新"
                        fillRatio: Math.min(1, store.windDirection / 360.0)
                        accent: theme.accentCyanDeep
                    }
                }
            }

            SectionCard {
                visible: store.rainCapability
                title: "雨滴"
                subtitle: "雨滴状态用于现场告警和控制动作前的快速判断。"

                GridLayout {
                    Layout.fillWidth: true
                    columns: root.phone ? 1 : 2
                    columnSpacing: 10
                    rowSpacing: 10

                    MetricTile {
                        mobile: root.mobile
                        title: "雨滴状态"
                        value: store.rainDetected ? "已检测" : "未检测"
                        caption: store.rainLevelText.length > 0 ? store.rainLevelText : "雨滴等级文本待更新"
                        fillRatio: store.rainDetected ? 1 : 0
                        accent: store.rainDetected ? theme.warning : theme.success
                    }

                    MetricTile {
                        mobile: root.mobile
                        title: "湿润度"
                        value: Number(store.rainValue).toFixed(0)
                        unit: "%"
                        caption: "ADC " + store.rainAdcRaw
                        fillRatio: Math.min(1, store.rainValue / 100.0)
                        accent: store.rainDetected ? theme.warning : theme.accentCyanDeep
                    }
                }
            }
        }

        SectionCard {
            visible: store.airCapability
            title: "空气质量"
            subtitle: "颗粒物、CO2、TVOC 和甲醛集中显示，便于判断现场空气状态。"

            GridLayout {
                Layout.fillWidth: true
                columns: root.phone ? 1 : 4
                columnSpacing: 10
                rowSpacing: 10

                MetricTile {
                    mobile: root.mobile
                    title: "PM2.5"
                    value: Number(store.pm25).toFixed(0)
                    unit: "ug/m3"
                    caption: "PM10 " + Number(store.pm10).toFixed(0) + " ug/m3"
                    fillRatio: Math.min(1, store.pm25 / 150.0)
                    accent: store.pm25 > 75 ? theme.warning : theme.success
                }

                MetricTile {
                    mobile: root.mobile
                    title: "CO2"
                    value: Number(store.co2).toFixed(0)
                    unit: "ppm"
                    caption: store.co2 > 1000 ? "建议通风或巡检" : "浓度正常"
                    fillRatio: Math.min(1, store.co2 / 2000.0)
                    accent: store.co2 > 1000 ? theme.warning : theme.success
                }

                MetricTile {
                    mobile: root.mobile
                    title: "TVOC"
                    value: Number(store.tvoc).toFixed(3)
                    unit: "mg/m3"
                    caption: "挥发性有机物"
                    fillRatio: Math.min(1, store.tvoc / 1.0)
                    accent: theme.pending
                }

                MetricTile {
                    mobile: root.mobile
                    title: "CH2O"
                    value: Number(store.ch2o).toFixed(3)
                    unit: "mg/m3"
                    caption: "甲醛"
                    fillRatio: Math.min(1, store.ch2o / 0.2)
                    accent: store.ch2o > 0.1 ? theme.warning : theme.success
                }
            }
        }

        SectionCard {
            title: "环境与诊断"
            subtitle: "业务页面默认看结论，原始输入和传感器错误集中放在这里。"

            GridLayout {
                Layout.fillWidth: true
                columns: root.phone ? 1 : 2
                columnSpacing: 10
                rowSpacing: 10

                MetricTile {
                    mobile: root.mobile
                    visible: store.pressureCapability
                    title: "气压"
                    value: Number(store.pressure).toFixed(1)
                    unit: "hPa"
                    caption: "来自气象扩展模块"
                    accent: theme.pending
                }

                MetricTile {
                    mobile: root.mobile
                    visible: store.visibilityCapability
                    title: "能见度"
                    value: Number(store.visibility).toFixed(0)
                    unit: "m"
                    caption: "来自气象扩展模块"
                    accent: theme.pending
                }
            }

            GridLayout {
                Layout.fillWidth: true
                columns: root.phone ? 1 : 3
                columnSpacing: 10
                rowSpacing: 10

                SensorHealthRow {
                    mobile: root.mobile
                    title: "风速/风向"
                    online: store.windSensorOnline
                    detail: "raw " + store.windSpeedRaw + " / " + store.windDirectionRaw
                }

                SensorHealthRow {
                    mobile: root.mobile
                    title: "空气质量"
                    online: store.airSensorOnline
                    detail: "PM2.5 " + Number(store.pm25).toFixed(0) + " / CO2 " + Number(store.co2).toFixed(0)
                }

                SensorHealthRow {
                    mobile: root.mobile
                    title: "雨滴"
                    online: store.rainSensorOnline
                    detail: "ADC " + store.rainAdcRaw + " / " + (store.rainLevelText.length > 0 ? store.rainLevelText : "--")
                }
            }

            MetricTile {
                mobile: root.mobile
                title: "连续失败"
                value: String(store.sensorFailureCount)
                caption: store.sensorLastError.length > 0 ? store.sensorLastError : "没有活动传感器错误"
                accent: store.sensorFailureCount > 0 ? theme.danger : theme.success
            }
        }
    }
}
