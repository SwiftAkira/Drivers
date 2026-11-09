import QtQuick
import Flux

ScrollPage {
    id: page
    title: "Keyboard Settings"
    subtitle: device ? "Firmware " + (device.firmware || "—") : ""

    SectionHeader { text: "Performance" }
    Card {
        SettingRow {
            icon: "flame"
            iconTint: Theme.red
            title: "Competitive mode"
            subtitle: "Unlocks 0.01 mm steps for actuation and Rapid Trigger."
            Toggle { checked: page.device ? page.device.gameMode : false; onToggled: (v) => page.device.setGameMode(v) }
        }
        SettingRow {
            icon: "arrow-down-to-line"
            iconTint: Theme.accent
            title: "Always trigger at bottom-out"
            subtitle: "A fully pressed key always counts as pressed, even if Rapid Trigger released it."
            Toggle { checked: page.device ? page.device.bottomOptimize : false; onToggled: (v) => page.device.setBottomOptimize(v) }
        }
        SettingRow {
            icon: "keyboard"
            iconTint: Theme.indigo
            title: "N-key rollover"
            subtitle: "Turn off only for old BIOSes or KVM switches that need 6-key mode."
            Toggle { checked: page.device ? page.device.nkro : true; onToggled: (v) => page.device.setNkro(v) }
        }
    }

    SectionHeader { text: "Layout" }
    Card {
        SettingRow {
            icon: "lock"
            iconTint: Theme.orange
            title: "Lock the Super key"
            subtitle: "Stops the Windows/Super key from opening the launcher mid-game."
            Toggle { checked: page.device ? page.device.winLock : false; onToggled: (v) => page.device.setWinLock(v) }
        }
        SettingRow {
            icon: "arrow-left-right"
            iconTint: Theme.teal
            title: "Swap WASD and arrow keys"
            Toggle { checked: page.device ? page.device.wasdSwap : false; onToggled: (v) => page.device.setWasdSwap(v) }
        }
        SettingRow {
            icon: "command"
            iconTint: Theme.gray
            title: "Mac mode"
            subtitle: "Switches to the Mac layers (swaps Alt and Super)."
            Toggle { checked: page.device ? page.device.macMode : false; onToggled: (v) => page.device.setMacMode(v) }
        }
    }

    SectionHeader { text: "Reset" }
    Card {
        SettingRow {
            title: "Reload from keyboard"
            FButton { compact: true; icon: "refresh-cw"; text: "Reload"; onClicked: page.device.refresh() }
        }
        SettingRow {
            icon: "keyboard"
            iconTint: Theme.gray
            title: "Reset keymap"
            subtitle: "Restores the default key layout on all four layers."
            FButton { compact: true; style: "destructive"; text: "Reset…"; onClicked: { confirm.mode = "keymap"; confirm.open() } }
        }
        SettingRow {
            icon: "rotate-ccw"
            iconTint: Theme.red
            title: "Restore factory settings"
            subtitle: "Resets actuation, Rapid Trigger, advanced keys, macros and lighting."
            FButton { compact: true; style: "destructive"; text: "Reset…"; onClicked: { confirm.mode = "factory"; confirm.open() } }
        }
    }

    Sheet {
        id: confirm
        property string mode: "factory"
        title: mode === "factory" ? "Restore factory settings?" : "Reset the keymap?"
        FText {
            width: parent.width
            text: "This can't be undone. Save a profile first under Profiles if you want to keep your current setup."
            wrapMode: Text.WordWrap
            color: Theme.secondaryLabel
            font.pixelSize: Theme.callout
        }
        footer: [
            FButton { text: "Cancel"; onClicked: confirm.close() },
            FButton {
                text: "Reset"
                style: "destructive"
                onClicked: {
                    if (confirm.mode === "factory") page.device.factoryReset(); else page.device.resetKeymap()
                    confirm.close()
                }
            }
        ]
    }
}
