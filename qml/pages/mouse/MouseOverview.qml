import QtQuick
import QtQuick.Layouts
import Flux

ScrollPage {
    id: page
    title: device ? device.displayName : "Mouse"
    subtitle: device ? device.connection + (device.wireless && device.receiverFirmware ? " · receiver " + device.receiverFirmware : "") : ""

    // Hero
    Rectangle {
        width: parent.width
        height: 248
        radius: 16
        gradient: Gradient {
            GradientStop { position: 0; color: "#2f3036" }
            GradientStop { position: 1; color: "#232327" }
        }
        border.color: Qt.rgba(1, 1, 1, 0.05)

        RowLayout {
            anchors.fill: parent
            anchors.margins: 24
            spacing: 28

            MouseIllustration {
                Layout.preferredWidth: 140
                Layout.maximumWidth: 140
                Layout.fillHeight: true
                interactive: false
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignLeft | Qt.AlignVCenter
                spacing: 14

                RowLayout {
                    spacing: 12
                    BatteryGlyph {
                        level: page.device && page.device.batteryLevel >= 0 ? page.device.batteryLevel : 0
                        charging: page.device ? page.device.charging : false
                        glyphScale: 1.6
                    }
                    FText {
                        text: page.device && page.device.batteryLevel >= 0 ? page.device.batteryLevel + "%" : "—"
                        font.pixelSize: 34
                        font.weight: Font.Bold
                    }
                    FText {
                        visible: page.device && page.device.charging
                        text: "Charging"
                        color: Theme.green
                        font.pixelSize: Theme.callout
                        font.weight: Font.DemiBold
                    }
                }

                GridLayout {
                    columns: 3
                    columnSpacing: 26
                    rowSpacing: 4
                    Repeater {
                        model: page.device ? [
                            { k: "DPI", v: page.device.stages.length ? page.device.stages[page.device.activeStage - 1].x : "—" },
                            { k: "Polling", v: page.device.pollingRate >= 1000 ? (page.device.pollingRate / 1000) + "K Hz" : page.device.pollingRate + " Hz" },
                            { k: "Lift-off", v: page.device.lod + " mm" }
                        ] : []
                        delegate: ColumnLayout {
                            spacing: 1
                            FText { text: modelData.k; color: Theme.secondaryLabel; font.pixelSize: Theme.footnote }
                            FText { text: String(modelData.v); font.pixelSize: Theme.title3; font.weight: Font.DemiBold }
                        }
                    }
                }

                FText {
                    text: page.device ? page.device.sensorName + " · firmware " + (page.device.mouseFirmware || "—") : ""
                    color: Theme.tertiaryLabel
                    font.pixelSize: Theme.footnote
                }
            }
        }
    }

    SectionHeader { text: "On-board profile" }
    Card {
        SettingRow {
            icon: "layers"
            iconTint: Theme.indigo
            title: "Active profile"
            subtitle: "The mouse stores three complete setups. Everything on the other pages edits the active one."
            Segmented {
                model: ["1", "2", "3"]
                currentIndex: page.device ? page.device.profile - 1 : 0
                onActivated: (i) => page.device.setProfile(i + 1)
            }
        }
    }

    SectionHeader { text: "Jump to" }
    Card {
        SettingRow { icon: "gauge"; iconTint: Theme.accent; title: "DPI & polling rate"; showChevron: true; onClicked: win.go("mouse/performance") }
        SettingRow { icon: "crosshair"; iconTint: Theme.teal; title: "Sensor tuning"; subtitle: "Lift-off distance, Motion Sync, angle snapping"; showChevron: true; onClicked: win.go("mouse/sensor") }
        SettingRow { icon: "mouse-pointer-click"; iconTint: Theme.orange; title: "Button remapping"; showChevron: true; onClicked: win.go("mouse/buttons") }
    }
}
