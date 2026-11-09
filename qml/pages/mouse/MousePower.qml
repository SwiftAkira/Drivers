import QtQuick
import QtQuick.Layouts
import Flux

ScrollPage {
    id: page
    title: "Power"
    subtitle: device && device.wireless ? "Battery and sleep" : "Connected by cable"

    readonly property var sleepLabels: ({ 10: "10 s", 30: "30 s", 60: "1 min", 300: "5 min", 600: "10 min", 1800: "30 min" })

    SectionHeader { text: "Battery" }
    Card {
        Item {
            width: parent.width
            height: 96
            RowLayout {
                anchors.fill: parent
                anchors.margins: 18
                spacing: 18
                BatteryGlyph {
                    level: page.device && page.device.batteryLevel >= 0 ? page.device.batteryLevel : 0
                    charging: page.device ? page.device.charging : false
                    glyphScale: 2.2
                }
                ColumnLayout {
                    spacing: 2
                    FText {
                        text: page.device && page.device.batteryLevel >= 0 ? page.device.batteryLevel + "%" : "Unknown"
                        font.pixelSize: Theme.largeTitle
                        font.weight: Font.Bold
                    }
                    FText {
                        text: page.device && page.device.charging ? "Charging over USB-C" : "On battery"
                        color: page.device && page.device.charging ? Theme.green : Theme.secondaryLabel
                        font.pixelSize: Theme.callout
                    }
                }
                Item { Layout.fillWidth: true }
            }
        }
        SettingRow {
            icon: "bell"
            iconTint: Theme.red
            title: "Low battery notification"
            subtitle: "Below " + App.lowBatteryThreshold + "%, then again at 10% and 5%. Change the level in Flux Settings."
            Toggle { checked: App.batteryAlerts; onToggled: (v) => App.batteryAlerts = v }
        }
    }

    SectionHeader { text: "Sleep" }
    Card {
        SettingRow {
            icon: "moon"
            iconTint: Theme.indigo
            title: "Sleep after"
            subtitle: "Idle time before the mouse powers down. It wakes on movement or a click."
            Dropdown {
                model: page.device ? page.device.sleepOptions.map((s) => page.sleepLabels[s] || (s + " s")) : []
                currentIndex: page.device ? page.device.sleepOptions.indexOf(page.device.sleepSeconds) : 0
                onActivated: (i) => page.device.setSleepSeconds(page.device.sleepOptions[i])
            }
        }
    }

    SectionHeader { text: "Device" }
    Card {
        SettingRow {
            title: "Firmware"
            FText {
                text: (page.device ? page.device.mouseFirmware || "—" : "—") + (page.device && page.device.receiverFirmware ? "  ·  receiver " + page.device.receiverFirmware : "")
                color: Theme.secondaryLabel
                font.pixelSize: Theme.callout
            }
        }
        SettingRow {
            title: "Reload from mouse"
            FButton { compact: true; icon: "refresh-cw"; text: "Reload"; onClicked: page.device.refresh() }
        }
        SettingRow {
            icon: "rotate-ccw"
            iconTint: Theme.red
            title: "Restore factory settings"
            subtitle: "Resets every profile, DPI stage, button and macro on the mouse" + (page.device && page.device.wireless ? " and the receiver." : ".")
            FButton { compact: true; style: "destructive"; text: "Reset…"; onClicked: resetSheet.open() }
        }
    }

    Sheet {
        id: resetSheet
        title: "Restore factory settings?"
        FText {
            width: parent.width
            text: "This can't be undone. Save a profile first under Profiles if you want to keep your current setup."
            wrapMode: Text.WordWrap
            color: Theme.secondaryLabel
            font.pixelSize: Theme.callout
        }
        footer: [
            FButton { text: "Cancel"; onClicked: resetSheet.close() },
            FButton { text: "Reset Mouse"; style: "destructive"; onClicked: { page.device.factoryReset(); resetSheet.close() } }
        ]
    }
}
