import QtQuick
import Flux

ScrollPage {
    id: page
    title: "Macros"
    subtitle: "Stored on the keyboard. To use one, put it on a key in Keymap (it shows up under Macros)."

    SectionHeader {
        text: "Slots"
        detail: page.device ? page.device.macroSlots + " slots · " + page.device.macroCapacity + " bytes" : ""
    }
    Card {
        SettingRow {
            visible: page.device && page.device.macroSlots === 0
            title: "This firmware reports no macro slots"
        }
        Repeater {
            model: page.device ? page.device.macros : []
            delegate: SettingRow {
                required property var modelData
                icon: "workflow"
                iconTint: modelData.empty ? Theme.gray : Theme.pink
                title: modelData.empty ? "M" + (modelData.id + 1) + " · empty" : modelData.name
                subtitle: modelData.empty ? "Click to record" : modelData.events.length + " steps · key code M" + (modelData.id + 1)
                showChevron: true
                onClicked: editor.openWith(modelData.id, modelData.empty ? "Macro " + (modelData.id + 1) : modelData.name, modelData.events)
                FButton {
                    visible: !modelData.empty
                    compact: true
                    style: "destructive"
                    icon: "trash-2"
                    onClicked: page.device.deleteMacro(modelData.id)
                }
            }
        }
    }

    FText {
        width: parent.width
        topPadding: 10
        text: "Keep single pauses under 255 ms where you can. Longer waits are split into several steps automatically."
        color: Theme.tertiaryLabel
        font.pixelSize: Theme.footnote
        wrapMode: Text.WordWrap
        elide: Text.ElideNone
    }

    MacroEditor {
        id: editor
        kind: "keyboard"
        onSave: (id, name, steps) => page.device.saveMacro(id, name, steps)
    }
}
