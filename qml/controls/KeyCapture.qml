import QtQuick
import Flux

// "Press a key…" field. Emits captured(usage, modifierMask) using HID usages.
Rectangle {
    id: root
    property bool capturing: false
    property bool allowModifiersOnly: true
    property string label: "Press a key…"
    signal captured(int usage, int modifiers)

    implicitWidth: Math.max(160, txt.implicitWidth + 40)
    implicitHeight: 32
    radius: 8
    color: capturing ? Theme.alpha(Theme.accent, 0.18) : Theme.tertiaryFill
    border.width: capturing ? 2 : 1
    border.color: capturing ? Theme.accent : Theme.hairline
    Behavior on color { ColorAnimation { duration: Motion.fast } }

    property int heldMods: 0

    function modsFrom(qtMods) {
        let m = 0
        if (qtMods & Qt.ControlModifier) m |= 0x01
        if (qtMods & Qt.ShiftModifier) m |= 0x02
        if (qtMods & Qt.AltModifier) m |= 0x04
        if (qtMods & Qt.MetaModifier) m |= 0x08
        return m
    }

    Row {
        anchors.centerIn: parent
        spacing: 7
        Rectangle {
            visible: root.capturing
            width: 8; height: 8; radius: 4
            color: Theme.red
            anchors.verticalCenter: parent.verticalCenter
            SequentialAnimation on opacity {
                running: root.capturing
                loops: Animation.Infinite
                NumberAnimation { to: 0.25; duration: 500 }
                NumberAnimation { to: 1; duration: 500 }
            }
        }
        FText {
            id: txt
            text: root.capturing ? "Listening… press a key" : root.label
            color: root.capturing ? Theme.accent : Theme.label
            font.pixelSize: Theme.callout
            font.weight: Font.Medium
        }
    }

    TapHandler {
        onTapped: {
            root.capturing = true
            root.heldMods = 0
            root.forceActiveFocus()
        }
    }
    HoverHandler { cursorShape: Qt.PointingHandCursor }

    onActiveFocusChanged: if (!activeFocus) capturing = false

    Keys.onPressed: (e) => {
        if (!capturing) return
        e.accepted = true
        if (e.key === Qt.Key_Escape && heldMods === 0 && e.modifiers === Qt.NoModifier) { capturing = false; return }
        const usage = HidKeys.hidFromScanCode(e.nativeScanCode)
        if (HidKeys.isModifierUsage(usage)) {
            heldMods |= HidKeys.modifierBit(usage) & 0x0F ? HidKeys.modifierBit(usage) : (HidKeys.modifierBit(usage) >> 4)
            return
        }
        if (usage > 0) {
            capturing = false
            root.captured(usage, modsFrom(e.modifiers))
        }
    }
    Keys.onReleased: (e) => {
        if (!capturing) return
        e.accepted = true
        const usage = HidKeys.hidFromScanCode(e.nativeScanCode)
        if (HidKeys.isModifierUsage(usage) && allowModifiersOnly && heldMods) {
            // A lone modifier press/release binds the modifier itself.
            capturing = false
            root.captured(usage, 0)
        }
    }
}
