import QtQuick
import QtQuick.Layouts
import Flux

ScrollPage {
    id: page
    title: "Profiles"
    subtitle: "Save the current settings of a device under a name, and bring them back with one click."

    property string pendingKind: ""
    property string pendingName: ""

    function devFor(kind) { return kind === "mouse" ? Devices.mouse : Devices.keyboard }

    SectionHeader { text: "Presets"; detail: "Tuned starting points, applied instantly" }
    Card {
        SettingRow {
            icon: "target"
            iconTint: Theme.pink
            title: "osu! mode · keyboard"
            subtitle: "0.15 mm actuation, Rapid Trigger 0.05 / 0.08 mm and competitive mode on " + (Devices.keyboard ? Devices.keyboard.osuKeysText : "your tap keys") + ". Switching it off puts your previous settings back. Also in the tray menu."
            enabled: Devices.keyboard !== null && Devices.keyboard.ready
            Row {
                spacing: 8
                FButton { compact: true; style: "plain"; text: "Keys…"; onClicked: osuKeysSheet.open() }
                Toggle {
                    anchors.verticalCenter: parent.verticalCenter
                    checked: Devices.keyboard ? Devices.keyboard.osuMode : false
                    onToggled: (v) => Devices.keyboard.osuMode = v
                }
            }
        }
        SettingRow {
            icon: "mouse"
            iconTint: Theme.pink
            title: "osu! · mouse"
            subtitle: "Highest polling rate, raw sensor (no Motion Sync or angle snapping), 0.7 mm lift-off, competition mode, 2 ms debounce. Your DPI stays as it is."
            enabled: Devices.mouse !== null && Devices.mouse.ready && Devices.mouse.online
            FButton { compact: true; style: "filled"; text: "Apply"; onClicked: Devices.mouse.applyOsuPreset() }
        }
    }

    Repeater {
        model: [
            { kind: "mouse", label: "Mouse", icon: "mouse" },
            { kind: "keyboard", label: "Keyboard", icon: "keyboard" }
        ]
        delegate: Column {
            id: section
            required property var modelData
            readonly property var dev: page.devFor(modelData.kind)
            readonly property var items: { Profiles.revision; return Profiles.list(modelData.kind) }
            width: parent.width

            SectionHeader {
                text: section.modelData.label
                detail: section.dev ? section.dev.displayName : "not connected"
                FButton {
                    compact: true
                    style: "plain"
                    icon: "folder-open"
                    text: "Show Folder"
                    onClicked: Profiles.openFolder(section.modelData.kind)
                }
                FButton {
                    compact: true
                    icon: "save"
                    text: "Save Current…"
                    enabled: section.dev !== null && section.dev.ready
                    onClicked: {
                        saveSheet.kind = section.modelData.kind
                        saveSheet.open()
                    }
                }
            }
            Card {
                Repeater {
                    model: section.items
                    delegate: SettingRow {
                        required property var modelData
                        icon: section.modelData.icon
                        iconTint: Theme.indigo
                        title: modelData.name
                        subtitle: "Saved " + modelData.modified
                        Row {
                            spacing: 6
                            FButton {
                                compact: true
                                style: "filled"
                                text: "Apply"
                                enabled: section.dev !== null && section.dev.ready
                                onClicked: {
                                    section.dev.importSettings(Profiles.load(section.modelData.kind, modelData.name))
                                    win.showToast("Applied “" + modelData.name + "”", false)
                                }
                            }
                            FButton {
                                compact: true
                                icon: "trash-2"
                                style: "destructive"
                                onClicked: {
                                    page.pendingKind = section.modelData.kind
                                    page.pendingName = modelData.name
                                    deleteSheet.open()
                                }
                            }
                        }
                    }
                }
                SettingRow {
                    visible: section.items.length === 0
                    title: "No saved profiles yet"
                    subtitle: section.dev ? "Use “Save Current…” to snapshot everything on the " + section.modelData.label.toLowerCase() + "." : "Connect the " + section.modelData.label.toLowerCase() + " to save its settings."
                }
            }
        }
    }

    Sheet {
        id: saveSheet
        property string kind: "mouse"
        title: "Save " + (kind === "mouse" ? "Mouse" : "Keyboard") + " Profile"
        onOpened: { nameField.text = ""; nameField.forceActiveFocus() }
        FText {
            text: "Everything currently on the device is stored, including remaps, macros and lighting."
            color: Theme.secondaryLabel
            font.pixelSize: Theme.callout
            wrapMode: Text.WordWrap
            width: parent.width
        }
        FTextField {
            id: nameField
            width: parent.width
            placeholderText: "Profile name, e.g. Valorant"
            onAccepted: saveBtn.clicked()
        }
        footer: [
            FButton { text: "Cancel"; onClicked: saveSheet.close() },
            FButton {
                id: saveBtn
                text: "Save"
                style: "filled"
                enabled: nameField.text.trim().length > 0
                onClicked: {
                    const dev = page.devFor(saveSheet.kind)
                    if (dev && Profiles.save(saveSheet.kind, nameField.text, dev.exportSettings()))
                        win.showToast("Saved “" + nameField.text.trim() + "”", false)
                    saveSheet.close()
                }
            }
        ]
    }

    Sheet {
        id: deleteSheet
        title: "Delete “" + page.pendingName + "”?"
        FText {
            text: "The saved profile file is removed. Your device keeps its current settings."
            color: Theme.secondaryLabel
            font.pixelSize: Theme.callout
            wrapMode: Text.WordWrap
            width: parent.width
        }
        footer: [
            FButton { text: "Cancel"; onClicked: deleteSheet.close() },
            FButton {
                text: "Delete"
                style: "destructive"
                onClicked: {
                    Profiles.remove(page.pendingKind, page.pendingName)
                    deleteSheet.close()
                }
            }
        ]
    }

    Sheet {
        id: osuKeysSheet
        title: "osu! tap keys"
        sheetWidth: 720
        onOpened: osuKb.selection = Devices.keyboard ? Devices.keyboard.osuKeys : []
        FText { text: "Select the keys you tap with."; color: Theme.secondaryLabel; font.pixelSize: Theme.callout }
        KeyboardView {
            id: osuKb
            width: parent.width
            device: Devices.keyboard
            dimUnselected: true
        }
        Row {
            spacing: 8
            FButton { compact: true; text: "A D"; onClicked: osuKb.selection = Devices.keyboard.indicesForKeycodes([0x04, 0x07]) }
            FButton { compact: true; text: "Z X"; onClicked: osuKb.selection = Devices.keyboard.indicesForKeycodes([0x1D, 0x1B]) }
            FButton { compact: true; text: "mania 4K"; onClicked: osuKb.selection = Devices.keyboard.indicesForKeycodes([0x07, 0x09, 0x0D, 0x0E]) }
            FButton { compact: true; text: "mania 7K"; onClicked: osuKb.selection = Devices.keyboard.indicesForKeycodes([0x16, 0x07, 0x09, 0x2C, 0x0D, 0x0E, 0x0F]) }
        }
        footer: [
            FButton { text: "Cancel"; onClicked: osuKeysSheet.close() },
            FButton {
                text: "Use These Keys"
                style: "filled"
                enabled: osuKb.selection.length > 0
                onClicked: { Devices.keyboard.setOsuKeys(osuKb.selection); osuKeysSheet.close() }
            }
        ]
    }
}
