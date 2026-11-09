import QtQuick
import QtQuick.Layouts
import Flux

ScrollPage {
    id: page
    title: "Keymap"
    subtitle: "Click a key to change what it types. Fn layers hold your media and lighting shortcuts."

    property int layerIdx: device && device.macMode ? 2 : 0
    property int editing: -1

    headerTrailing: Segmented {
        model: page.device ? page.device.layerNames : []
        currentIndex: page.layerIdx
        onActivated: (i) => page.layerIdx = i
    }

    Rectangle {
        width: parent.width
        height: kb.implicitHeight + 36
        radius: 16
        color: "#141416"
        border.color: Qt.rgba(1, 1, 1, 0.05)
        KeyboardView {
            id: kb
            anchors.fill: parent
            anchors.margins: 18
            device: page.device
            multiSelect: false
            selection: page.editing >= 0 ? [page.editing] : []
            labelFor: (i) => {
                if (!page.device) return ""
                const code = page.device.keymap[page.layerIdx][i]
                return page.device.keycodeShort(code)
            }
            fillFor: (i) => {
                if (!page.device || page.layerIdx === 0) return undefined
                const code = page.device.keymap[page.layerIdx][i]
                return code === 1 ? undefined : Theme.indigo
            }
            onKeyClicked: (i) => {
                page.editing = i
                picker.current = page.device.keymap[page.layerIdx][i]
                picker.fnCode = page.layerIdx >= 2 ? 0x5223 : 0x5221
                picker.heading = "Key " + page.device.keyName(i) + " · " + page.device.layerNames[page.layerIdx]
                picker.open()
            }
        }
    }

    Item { width: 1; height: 8 }
    FText {
        width: parent.width
        text: page.layerIdx === 0 || page.layerIdx === 2
              ? "Base layer. Hold Fn to reach the Fn layer."
              : "Fn layer. ▽ means the key falls through to the base layer. Tinted keys do something different on this layer."
        color: Theme.tertiaryLabel
        font.pixelSize: Theme.footnote
        wrapMode: Text.WordWrap
        elide: Text.ElideNone
    }

    SectionHeader { text: "Selected key"; visible: page.editing >= 0 }
    Card {
        visible: page.editing >= 0
        SettingRow {
            title: page.device && page.editing >= 0 ? page.device.keycodeLabel(page.device.keymap[page.layerIdx][page.editing]) : ""
            subtitle: "Position row " + Math.floor(page.editing / 14) + ", column " + (page.editing % 14)
            Row {
                spacing: 6
                FButton { compact: true; text: "Change…"; onClicked: picker.open() }
                FButton {
                    compact: true
                    style: "plain"
                    text: page.layerIdx % 2 ? "Make transparent" : "Disable"
                    onClicked: page.device.setKeycode(page.layerIdx, page.editing, page.layerIdx % 2 ? 1 : 0)
                }
            }
        }
    }

    KeycodePicker {
        id: picker
        objectName: "keycodePicker"
        device: page.device
        onPicked: (code) => page.device.setKeycode(page.layerIdx, page.editing, code)
    }
}
