import QtQuick
import QtQuick.Layouts
import Flux

ScrollPage {
    id: page
    title: "Advanced Keys"
    subtitle: "SOCD / Snap Tap, Dynamic Keystroke, Mod-Tap and Toggle keys. Up to 20 in total."

    Component.onCompleted: if (device && !device.advancedLoaded) device.loadAdvanced()

    readonly property var socd: device ? device.socdSlots : []
    readonly property var dks: device ? device.dksSlots : []
    readonly property var td: device ? device.tapDances : []
    readonly property var base: device ? device.keymap[0] : []

    readonly property var modeNames: ["", "Rappy Snappy (deeper key wins)", "Last input wins (Snap Tap)", "First key always wins", "Second key always wins", "Neutral (cancel both)", "On release"]

    function keyLabel(i) { return device ? device.keyName(i) : "" }
    function socdUsed() { return socd.filter((s) => s.mode > 0) }
    function dksBound() {
        const out = []
        for (let i = 0; i < base.length; ++i) if (base[i] >= 0x7E05 && base[i] < 0x7E05 + 20) out.push({ index: i, slot: base[i] - 0x7E05 })
        return out
    }
    function tdBound(lo, hi) {
        const out = []
        for (let l = 0; l < 4; ++l) {
            const km = device ? device.keymap[l] : []
            for (let i = 0; i < km.length; ++i) {
                const c = km[i]
                if ((c >> 8) === 0x57 && (c & 0xFF) >= lo && (c & 0xFF) < hi) out.push({ layer: l, index: i, slot: c & 0xFF })
            }
        }
        return out
    }
    readonly property int total: socdUsed().length + dksBound().length + tdBound(0, 20).length + tdBound(20, 40).length

    Rectangle {
        visible: page.device && !page.device.advancedLoaded
        width: parent.width
        height: 50
        radius: 12
        color: Theme.card
        Row { anchors.centerIn: parent; spacing: 10; Spinner { size: 18 } FText { text: "Reading advanced keys…"; color: Theme.secondaryLabel } }
    }

    // ── SOCD ─────────────────────────────────────────────────────────────
    SectionHeader {
        text: "SOCD & Snap Tap"
        detail: "Two keys that fight, like A/D strafing"
        FButton { compact: true; icon: "plus"; text: "Add Pair"; enabled: page.total < 20; onClicked: socdSheet.openNew() }
    }
    Card {
        SettingRow {
            visible: page.socdUsed().length === 0
            icon: "arrow-left-right"
            iconTint: Theme.teal
            title: "No pairs yet"
            subtitle: "With Snap Tap, pressing D while holding A instantly switches direction, with no neutral gap."
        }
        Repeater {
            model: page.socd
            delegate: SettingRow {
                required property var modelData
                required property int index
                visible: modelData.mode > 0
                icon: "arrow-left-right"
                iconTint: Theme.teal
                title: page.keyLabel(modelData.key1) + "  ⇄  " + page.keyLabel(modelData.key2)
                subtitle: page.modeNames[modelData.mode] + (modelData.globalApc ? "" : " · " + modelData.level.toFixed(2) + " mm")
                Row {
                    spacing: 6
                    FButton { compact: true; text: "Edit"; onClicked: socdSheet.openEdit(index, modelData) }
                    FButton { compact: true; style: "destructive"; icon: "trash-2"; onClicked: page.device.setSocd(index, { mode: 0 }) }
                }
            }
        }
    }

    // ── DKS ──────────────────────────────────────────────────────────────
    SectionHeader {
        text: "Dynamic Keystroke"
        detail: "Different actions at different depths"
        FButton { compact: true; icon: "plus"; text: "Add"; enabled: page.total < 20; onClicked: dksSheet.openNew() }
    }
    Card {
        SettingRow {
            visible: page.dksBound().length === 0
            icon: "layers-3"
            iconTint: Theme.purple
            title: "No DKS keys yet"
            subtitle: "Example: tap W lightly to walk, press it all the way to also hold Shift and sprint."
        }
        Repeater {
            model: page.dksBound()
            delegate: SettingRow {
                required property var modelData
                icon: "layers-3"
                iconTint: Theme.purple
                title: "DKS on key at row " + Math.floor(modelData.index / 14) + ", col " + (modelData.index % 14)
                subtitle: (page.dks[modelData.slot] || []).filter((a) => a.keycode).map((a) => page.device.keycodeShort(a.keycode)).join(" · ")
                Row {
                    spacing: 6
                    FButton { compact: true; text: "Edit"; onClicked: dksSheet.openEdit(modelData.index, modelData.slot) }
                    FButton { compact: true; style: "destructive"; icon: "trash-2"; onClicked: page.device.setKeycode(0, modelData.index, (page.dks[modelData.slot] || [{ keycode: 0 }])[0].keycode || 0) }
                }
            }
        }
    }

    // ── Mod-Tap & Toggle ─────────────────────────────────────────────────
    SectionHeader {
        text: "Mod-Tap & Toggle"
        detail: "Tap for one key, hold for another"
        FButton { compact: true; icon: "plus"; text: "Mod-Tap"; enabled: page.total < 20; onClicked: mtSheet.openNew(false) }
        FButton { compact: true; icon: "plus"; text: "Toggle"; enabled: page.total < 20; onClicked: mtSheet.openNew(true) }
    }
    Card {
        SettingRow {
            visible: page.tdBound(0, 40).length === 0
            icon: "git-merge"
            iconTint: Theme.orange
            title: "None yet"
            subtitle: "Example: Caps Lock taps Esc, but acts as Ctrl when held."
        }
        Repeater {
            model: page.tdBound(0, 40)
            delegate: SettingRow {
                required property var modelData
                readonly property var t: page.td[modelData.slot] || { tap: 0, hold: 0, term: 200 }
                readonly property bool toggle: modelData.slot >= 20
                icon: toggle ? "lock" : "git-merge"
                iconTint: toggle ? Theme.yellow : Theme.orange
                title: toggle ? "Toggle " + page.device.keycodeShort(t.tap) : "Tap " + page.device.keycodeShort(t.tap) + " · hold " + page.device.keycodeShort(t.hold)
                subtitle: (toggle ? "Press once to hold it down, again to release" : "Hold time " + t.term + " ms") + " · layer " + modelData.layer
                FButton { compact: true; style: "destructive"; icon: "trash-2"; onClicked: page.device.setKeycode(modelData.layer, modelData.index, t.tap) }
            }
        }
    }

    // ── Sheets ───────────────────────────────────────────────────────────
    Sheet {
        id: socdSheet
        title: slot >= 0 && editing ? "Edit Pair" : "New SOCD Pair"
        sheetWidth: 720
        property int slot: -1
        property bool editing: false
        property var keys: []
        property int mode: 2
        property bool follow: true
        property double level: 1.0
        function openNew() {
            slot = page.socd.findIndex((s) => s.mode === 0)
            editing = false; keys = []; mode = 2; follow = true; level = 1.0
            socdKb.selection = []
            if (slot >= 0) open()
        }
        function openEdit(i, s) {
            slot = i; editing = true; keys = [s.key1, s.key2]; mode = s.mode; follow = s.globalApc; level = s.level || 1.0
            socdKb.selection = keys
            open()
        }
        FText { text: "Select exactly two keys (click, then shift-click)."; color: Theme.secondaryLabel; font.pixelSize: Theme.callout }
        KeyboardView {
            id: socdKb
            width: parent.width
            device: page.device
            dimUnselected: true
            onSelectionChanged: {
                if (selection.length > 2) { selection = selection.slice(-2); return }
                socdSheet.keys = selection.slice()
            }
        }
        Dropdown {
            model: [1, 2, 3, 4, 5].map((m) => page.modeNames[m])
            currentIndex: socdSheet.mode - 1
            onActivated: (i) => socdSheet.mode = i + 1
        }
        RowLayout {
            width: parent.width
            FText { text: "Use the keys' own actuation"; Layout.fillWidth: true }
            Toggle { checked: socdSheet.follow; onToggled: (v) => socdSheet.follow = v }
        }
        SliderRow {
            visible: !socdSheet.follow
            title: "Actuation for this pair"
            from: 0.1; to: 3.5; stepSize: 0.02
            value: socdSheet.level
            format: (v) => v.toFixed(2) + " mm"
            onCommitted: (v) => socdSheet.level = v
        }
        footer: [
            FButton { text: "Cancel"; onClicked: socdSheet.close() },
            FButton {
                text: "Save"
                style: "filled"
                enabled: socdSheet.keys.length === 2 && socdSheet.keys[0] !== socdSheet.keys[1]
                onClicked: {
                    page.device.setSocd(socdSheet.slot, { mode: socdSheet.mode, key1: socdSheet.keys[0], key2: socdSheet.keys[1],
                                                          globalApc: socdSheet.follow, useRt: false, level: socdSheet.follow ? 0 : socdSheet.level, layer: 0 })
                    socdSheet.close()
                }
            }
        ]
    }

    Sheet {
        id: dksSheet
        title: "Dynamic Keystroke"
        sheetWidth: 720
        property int keyIndex: -1
        property int slot: 0
        property var rows: []
        readonly property var patterns: [
            { name: "Off", mask: 0 },
            { name: "Tap on press", mask: 0x001 }, { name: "Hold: press → bottom", mask: 0x003 },
            { name: "Hold: press → lift", mask: 0x01F }, { name: "Hold: press → release", mask: 0x0FF },
            { name: "Tap at bottom", mask: 0x008 }, { name: "Hold: bottom → lift", mask: 0x018 },
            { name: "Hold: bottom → release", mask: 0x0F8 }, { name: "Tap on lift", mask: 0x040 },
            { name: "Hold: lift → release", mask: 0x0C0 }, { name: "Tap on release", mask: 0x200 }
        ]
        function freeSlot() {
            const used = page.dksBound().map((b) => b.slot)
            for (let s = 0; s < 20; ++s) if (used.indexOf(s) < 0) return s
            return -1
        }
        function openNew() {
            keyIndex = -1; slot = freeSlot()
            rows = [{ keycode: 0, mask: 0x0FF }, { keycode: 0, mask: 0 }, { keycode: 0, mask: 0 }, { keycode: 0, mask: 0 }]
            if (slot >= 0) open()
        }
        function openEdit(idx, s) {
            keyIndex = idx; slot = s
            rows = JSON.parse(JSON.stringify(page.dks[s] || []))
            while (rows.length < 4) rows.push({ keycode: 0, mask: 0 })
            open()
        }
        function setRow(i, patch) { const r = rows.slice(); r[i] = Object.assign({}, r[i], patch); rows = r }

        FText { text: "Pick the physical key, then up to four actions."; color: Theme.secondaryLabel; font.pixelSize: Theme.callout }
        KeyboardView {
            width: parent.width
            device: page.device
            multiSelect: false
            selection: dksSheet.keyIndex >= 0 ? [dksSheet.keyIndex] : []
            onKeyClicked: (i) => dksSheet.keyIndex = i
            dimUnselected: true
        }
        Repeater {
            model: 4
            delegate: RowLayout {
                required property int index
                width: parent.width
                spacing: 10
                FText { text: (index + 1) + "."; color: Theme.tertiaryLabel; Layout.preferredWidth: 18 }
                FButton {
                    Layout.preferredWidth: 150
                    text: dksSheet.rows[index] && dksSheet.rows[index].keycode ? page.device.keycodeShort(dksSheet.rows[index].keycode) : "Choose key…"
                    onClicked: { dksPicker.row = index; dksPicker.current = dksSheet.rows[index].keycode; dksPicker.open() }
                }
                Dropdown {
                    Layout.fillWidth: true
                    model: dksSheet.patterns.map((p) => p.name)
                    currentIndex: Math.max(0, dksSheet.patterns.findIndex((p) => dksSheet.rows[index] && p.mask === dksSheet.rows[index].mask))
                    onActivated: (i) => dksSheet.setRow(index, { mask: dksSheet.patterns[i].mask })
                }
            }
        }
        footer: [
            FButton { text: "Cancel"; onClicked: dksSheet.close() },
            FButton {
                text: "Save"
                style: "filled"
                enabled: dksSheet.keyIndex >= 0 && dksSheet.rows.some((r) => r.keycode && r.mask)
                onClicked: {
                    page.device.setDks(dksSheet.slot, dksSheet.rows)
                    page.device.setKeycode(0, dksSheet.keyIndex, 0x7E05 + dksSheet.slot)
                    dksSheet.close()
                }
            }
        ]
        KeycodePicker {
            id: dksPicker
            property int row: 0
            device: page.device
            onPicked: (code) => dksSheet.setRow(row, { keycode: code & 0xFF })
        }
    }

    Sheet {
        id: mtSheet
        property bool toggle: false
        property int keyIndex: -1
        property int slot: 0
        property int tap: 0
        property int hold: 0
        property int term: 200
        property string picking: "tap"
        title: toggle ? "New Toggle Key" : "New Mod-Tap Key"
        sheetWidth: 720
        function openNew(isToggle) {
            toggle = isToggle
            const used = page.tdBound(isToggle ? 20 : 0, isToggle ? 40 : 20).map((b) => b.slot)
            slot = -1
            for (let s = isToggle ? 20 : 0; s < (isToggle ? 40 : 20); ++s) if (used.indexOf(s) < 0) { slot = s; break }
            keyIndex = -1; tap = 0; hold = 0; term = 200
            if (slot >= 0) open()
        }
        FText { text: "Pick the key to change."; color: Theme.secondaryLabel; font.pixelSize: Theme.callout }
        KeyboardView {
            width: parent.width
            device: page.device
            multiSelect: false
            selection: mtSheet.keyIndex >= 0 ? [mtSheet.keyIndex] : []
            onKeyClicked: (i) => { mtSheet.keyIndex = i; if (!mtSheet.tap) mtSheet.tap = page.base[i] & 0xFF }
            dimUnselected: true
        }
        RowLayout {
            width: parent.width
            spacing: 10
            FText { text: mtSheet.toggle ? "Key to toggle" : "On tap"; Layout.preferredWidth: 90 }
            FButton { text: mtSheet.tap ? page.device.keycodeShort(mtSheet.tap) : "Choose…"; onClicked: { mtSheet.picking = "tap"; mtPicker.current = mtSheet.tap; mtPicker.open() } }
            Item { Layout.fillWidth: true }
            FText { visible: !mtSheet.toggle; text: "On hold"; Layout.preferredWidth: 60 }
            FButton { visible: !mtSheet.toggle; text: mtSheet.hold ? page.device.keycodeShort(mtSheet.hold) : "Choose…"; onClicked: { mtSheet.picking = "hold"; mtPicker.current = mtSheet.hold; mtPicker.open() } }
        }
        SliderRow {
            visible: !mtSheet.toggle
            title: "Hold time"
            from: 10; to: 1000; stepSize: 10
            value: mtSheet.term
            format: (v) => Math.round(v) + " ms"
            onCommitted: (v) => mtSheet.term = Math.round(v)
        }
        footer: [
            FButton { text: "Cancel"; onClicked: mtSheet.close() },
            FButton {
                text: "Save"
                style: "filled"
                enabled: mtSheet.keyIndex >= 0 && mtSheet.tap && (mtSheet.toggle || mtSheet.hold)
                onClicked: {
                    page.device.setTapDance(mtSheet.slot, mtSheet.tap, mtSheet.toggle ? 0 : mtSheet.hold, mtSheet.toggle ? 200 : mtSheet.term)
                    page.device.setKeycode(0, mtSheet.keyIndex, 0x5700 | mtSheet.slot)
                    mtSheet.close()
                }
            }
        ]
        KeycodePicker {
            id: mtPicker
            device: page.device
            onPicked: (code) => { if (mtSheet.picking === "tap") mtSheet.tap = code & 0xFF; else mtSheet.hold = code & 0xFF }
        }
    }
}
