import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ScrollView {
    id: root
    property var controller
    readonly property var store: controller.realtimeGateway.stateStore
    readonly property bool mobile: controller.androidMode || width < 1040
    readonly property bool busy: controller.commandPending
    readonly property bool padDevice: store.protocolProfile === "airport_pad_v1"

    clip: true

    TaskTheme { id: theme }

    component CommandButton: Button {
        property color fillColor: theme.glassStrong
        property color labelColor: theme.textBody
        Layout.fillWidth: true
        implicitHeight: root.mobile ? 54 : 50

        contentItem: Text {
            text: parent.text
            color: parent.enabled ? parent.labelColor : "#70808b"
            font.pixelSize: root.mobile ? 17 : 15
            font.bold: true
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }

        background: Rectangle {
            radius: 22
            color: parent.enabled ? parent.fillColor : theme.neutral
            border.color: theme.borderSoft
            border.width: 1
            opacity: parent.down ? 0.88 : 1.0
        }
    }

    component ValueCard: Rectangle {
        required property string title
        required property string value
        required property string caption
        Layout.fillWidth: true
        radius: theme.radiusMedium
        color: theme.glassSoft
        border.color: theme.borderSoft
        border.width: 1
        implicitHeight: 96

        Column {
            anchors.fill: parent
            anchors.margins: 14
            spacing: 4

            Label {
                text: parent.parent.title
                color: theme.textMuted
                font.pixelSize: 12
            }

            Label {
                text: parent.parent.value
                color: theme.textBody
                font.pixelSize: 22
                font.bold: true
            }

            Label {
                text: parent.parent.caption
                color: theme.textMuted
                font.pixelSize: 11
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
            implicitHeight: root.mobile ? 212 : 236

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: root.mobile ? 18 : 24
                spacing: 12

                RowLayout {
                    Layout.fillWidth: true

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 4

                        Label {
                            text: "停机场控制"
                            color: theme.textPrimary
                            font.pixelSize: root.mobile ? 26 : 34
                            font.bold: true
                        }

                        Label {
                            text: store.currentDeviceId.length > 0 ? store.currentDeviceId : "尚未选中设备"
                            color: "#cbd8e0"
                            font.pixelSize: root.mobile ? 15 : 16
                        }
                    }

                    Rectangle {
                        radius: 18
                        implicitWidth: root.mobile ? 104 : 130
                        implicitHeight: root.mobile ? 38 : 40
                        color: store.online ? theme.success : theme.danger
                        border.color: store.online ? "#a4c8bf" : "#d7b2b7"
                        border.width: 1

                        Label {
                            anchors.centerIn: parent
                            text: store.online ? "在线" : "离线"
                            color: "white"
                            font.pixelSize: 15
                            font.bold: true
                        }
                    }
                }

                Label {
                    width: parent.width
                    text: padDevice
                          ? "支持停机场打开、关闭、停止与状态查询，左右舱门状态会分别回传。"
                          : "当前设备仍是工程测试设备。停机场新协议设备接入后，这里会显示正式控制面板。"
                    wrapMode: Text.Wrap
                    color: "#cfdae2"
                    font.pixelSize: 13
                }

                GridLayout {
                    Layout.fillWidth: true
                    columns: root.mobile ? 2 : 4
                    columnSpacing: 10
                    rowSpacing: 10

                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: 38
                        radius: 16
                        color: "#12000000"
                        border.color: "#6d8999"
                        border.width: 1

                        Label {
                            anchors.centerIn: parent
                            text: "左门 " + theme.padStateText(store.padLeftState)
                            color: theme.textPrimary
                            font.pixelSize: 13
                            font.bold: true
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: 38
                        radius: 16
                        color: "#12000000"
                        border.color: "#6d8999"
                        border.width: 1

                        Label {
                            anchors.centerIn: parent
                            text: "右门 " + theme.padStateText(store.padRightState)
                            color: theme.textPrimary
                            font.pixelSize: 13
                            font.bold: true
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: 38
                        radius: 16
                        color: "#12000000"
                        border.color: "#6d8999"
                        border.width: 1

                        Label {
                            anchors.centerIn: parent
                            text: store.padReady ? "允许降落" : "待命"
                            color: theme.textPrimary
                            font.pixelSize: 13
                            font.bold: true
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: 38
                        radius: 16
                        color: "#12000000"
                        border.color: "#6d8999"
                        border.width: 1

                        Label {
                            anchors.centerIn: parent
                            text: theme.occupancyText(store.padOccupied)
                            color: theme.textPrimary
                            font.pixelSize: 13
                            font.bold: true
                        }
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
            implicitHeight: root.mobile ? 330 : 250

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: root.mobile ? 16 : 18
                spacing: 14

                Label {
                    text: "实时操作"
                    color: theme.textBody
                    font.pixelSize: 24
                    font.bold: true
                }

                GridLayout {
                    Layout.fillWidth: true
                    columns: root.mobile ? 1 : 2
                    columnSpacing: 12
                    rowSpacing: 12

                    CommandButton {
                        text: "打开停机场"
                        enabled: padDevice && root.controller.commandConnected && !root.busy
                        fillColor: "#d9eaee"
                        labelColor: theme.accentCyanDeep
                        onClicked: root.controller.padOpen()
                    }

                    CommandButton {
                        text: "关闭停机场"
                        enabled: padDevice && root.controller.commandConnected && !root.busy
                        fillColor: "#ece4dc"
                        labelColor: "#5a4432"
                        onClicked: root.controller.padClose()
                    }

                    CommandButton {
                        text: "停止动作"
                        enabled: padDevice && root.controller.commandConnected && !root.busy
                        fillColor: "#ece1e2"
                        labelColor: "#5b3640"
                        onClicked: root.controller.padStop()
                    }

                    CommandButton {
                        text: "刷新状态"
                        enabled: root.controller.commandConnected && !root.busy
                        fillColor: theme.glassStrong
                        labelColor: theme.textBody
                        onClicked: root.controller.queryPadStatus()
                    }
                }

                Label {
                    visible: !padDevice
                    width: parent.width
                    text: controller.engineeringMode
                          ? "这是继电器工程测试设备。若要继续本地联调，请切换到工程模式页使用旧控制链。"
                          : "当前设备尚未上报停机场新协议字段。接入 airport_pad_v1 后，这里会显示完整控制能力。"
                    wrapMode: Text.Wrap
                    color: theme.textMuted
                    font.pixelSize: 13
                }

                RowLayout {
                    visible: !padDevice && controller.engineeringMode
                    spacing: 10

                    Button {
                        text: "打开工程模式页"
                        onClicked: controller.currentPage = "engineering"
                    }

                    Button {
                        text: "查看串口"
                        onClicked: controller.currentPage = "serial"
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
                implicitHeight: 220

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 18
                    spacing: 12

                    Label {
                        text: "停机场状态"
                        color: theme.textBody
                        font.pixelSize: 24
                        font.bold: true
                    }

                    GridLayout {
                        Layout.fillWidth: true
                        columns: 2
                        columnSpacing: 12
                        rowSpacing: 12

                        ValueCard {
                            title: "左舱门"
                            value: theme.padStateText(store.padLeftState)
                            caption: "机械舱门 A"
                        }

                        ValueCard {
                            title: "右舱门"
                            value: theme.padStateText(store.padRightState)
                            caption: "机械舱门 B"
                        }

                        ValueCard {
                            title: "就绪度"
                            value: store.padReady ? "允许降落" : "待命"
                            caption: theme.occupancyText(store.padOccupied)
                        }

                        ValueCard {
                            title: "控制模式"
                            value: theme.padModeText(store.padMode)
                            caption: "自动 / 手动 / 维护"
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
                implicitHeight: 220

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 18
                    spacing: 12

                    Label {
                        text: "最新回执"
                        color: theme.textBody
                        font.pixelSize: 24
                        font.bold: true
                    }

                    Label {
                        width: parent.width
                        text: store.lastAckSummary.length > 0 ? store.lastAckSummary : "尚未收到新的任务回执"
                        wrapMode: Text.Wrap
                        color: theme.textBody
                        font.pixelSize: 18
                        font.bold: true
                    }

                    Label {
                        width: parent.width
                        text: store.lastError.length > 0
                              ? "错误：" + store.lastError
                              : (root.busy ? "命令处理中，请等待状态回传。" : "控制通道空闲，可继续操作。")
                        wrapMode: Text.Wrap
                        color: store.lastError.length > 0 ? theme.danger : theme.textMuted
                        font.pixelSize: 13
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        radius: theme.radiusMedium
                        color: "#eff4f7"
                        border.color: theme.borderSoft
                        border.width: 1

                        Column {
                            anchors.fill: parent
                            anchors.margins: 14
                            spacing: 8

                            Label {
                                text: "当前气象"
                                color: theme.textBody
                                font.pixelSize: 16
                                font.bold: true
                            }

                            Label {
                                text: "风速 " + Number(store.windSpeed).toFixed(1) + " m/s  ·  能见度 "
                                      + Number(store.visibility).toFixed(1) + " km"
                                color: theme.textMuted
                                font.pixelSize: 12
                            }

                            Label {
                                text: "适航建议：" + theme.flightRuleText(store.windSpeed, store.visibility)
                                color: theme.flightRuleColor(store.windSpeed, store.visibility)
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
