import QtQuick
import QtQuick.Layouts
import Flux

ScrollPage {
    id: page
    title: "Devices"
    subtitle: Devices.devices.length > 0 ? "Pick a device in the sidebar." : "Plug in your mouse receiver or keyboard."

    SectionHeader {
        visible: Devices.blocked.length > 0
        text: "Permission needed"
    }
    Card {
        visible: Devices.blocked.length > 0
        SettingRow {
            icon: "lock"
            iconTint: Theme.orange
            title: Devices.blocked.join(", ") + " found, but Flux can't talk to it yet"
            subtitle: "Linux only lets root open raw HID devices by default. Install the Flux udev rule once (it grants access to the logged-in user only), then replug the device."
        }
        Item {
            width: parent.width
            height: cmdBox.height + 28
            Rectangle {
                id: cmdBox
                x: 16
                y: 4
                width: parent.width - 32
                height: cmdText.implicitHeight + 20
                radius: 8
                color: Qt.rgba(0, 0, 0, 0.3)
                border.color: Theme.hairline
                FText {
                    id: cmdText
                    anchors.fill: parent
                    anchors.margins: 10
                    anchors.rightMargin: 84
                    text: Devices.udevInstallCommand
                    font.family: "monospace"
                    font.pixelSize: Theme.footnote
                    wrapMode: Text.WrapAnywhere
                    elide: Text.ElideNone
                    color: Theme.secondaryLabel
                }
                FButton {
                    anchors.right: parent.right
                    anchors.rightMargin: 8
                    anchors.verticalCenter: parent.verticalCenter
                    compact: true
                    icon: "copy"
                    text: "Copy"
                    onClicked: {
                        App.copyToClipboard(Devices.udevInstallCommand)
                        win.showToast("Command copied", false)
                    }
                }
            }
        }
    }

    SectionHeader { text: "Connected" }
    Card {
        Repeater {
            model: Devices.devices
            delegate: SettingRow {
                required property var modelData
                icon: modelData.kind === "mouse" ? "mouse" : "keyboard"
                iconTint: modelData.kind === "mouse" ? Theme.accent : Theme.indigo
                title: modelData.displayName || (modelData.kind === "mouse" ? "Mouse" : "Keyboard")
                subtitle: modelData.connection + (modelData.firmware ? " · Firmware " + modelData.firmware : "")
                showChevron: true
                onClicked: win.go(modelData.kind + "/overview")
            }
        }
        SettingRow {
            visible: Devices.devices.length === 0
            icon: "plug"
            iconTint: Theme.gray
            title: "No supported devices connected"
            subtitle: "Flux supports the LAMZU INCA 8K (receiver or cable) and the MADLIONS MAD LIGHT 60 HE."
        }
    }

    Item { width: 1; height: 18 }
    FButton {
        text: "Rescan"
        icon: "refresh-cw"
        onClicked: Devices.rescan()
    }
}
