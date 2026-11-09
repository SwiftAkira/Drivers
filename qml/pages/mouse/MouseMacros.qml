import QtQuick
import Flux

ScrollPage {
    id: page
    title: "Macros"
    subtitle: "Macros are stored on the mouse and work on any computer. Bind them on the Buttons page."

    headerTrailing: FButton {
        text: "New Macro"
        icon: "plus"
        style: "filled"
        enabled: page.device && page.device.macrosLoaded && page.device.freeMacroId() > 0
        onClicked: {
            const id = page.device.freeMacroId()
            editor.openWith(id, "Macro " + id, [])
        }
    }

    SectionHeader { text: "On the mouse"; detail: page.device ? page.device.macros.length + " of 30" : "" }
    Card {
        SettingRow {
            visible: page.device && !page.device.macrosLoaded
            title: "Reading macros from the mouse…"
            Spinner { size: 18 }
        }
        SettingRow {
            visible: page.device && page.device.macrosLoaded && page.device.macros.length === 0
            icon: "workflow"
            iconTint: Theme.purple
            title: "No macros yet"
            subtitle: "Record a sequence of keys, clicks and pauses, then bind it to a side button."
        }
        Repeater {
            model: page.device ? page.device.macros : []
            delegate: SettingRow {
                required property var modelData
                icon: "workflow"
                iconTint: Theme.purple
                title: modelData.name
                subtitle: modelData.events.length + " steps · " + modelData.bytes + " bytes"
                showChevron: true
                onClicked: editor.openWith(modelData.id, modelData.name, modelData.events)
                FButton {
                    compact: true
                    style: "destructive"
                    icon: "trash-2"
                    onClicked: page.device.deleteMacro(modelData.id)
                }
            }
        }
    }

    MacroEditor {
        id: editor
        kind: "mouse"
        onSave: (id, name, steps) => page.device.saveMacro(id, name, steps)
    }
}
