import QtQuick
import QtQuick.Layouts
import Flux

// Edits a LAMZU button action map ({kind, ...}).
Sheet {
    id: root
    property var device: null
    property int buttonId: 1
    property string buttonName: ""
    property var action: ({ kind: "mouse", code: 1 })
    signal apply(int buttonId, var action)

    title: buttonName
    sheetWidth: 560

    readonly property var categories: buttonId === 20
        ? ["DPI", "Cycles", "Mouse", "Keyboard", "Media", "Macro", "Off"]
        : ["Mouse", "Keyboard", "Media", "DPI", "Macro", "Cycles", "Off"]
    property string cat: "Mouse"

    function catOf(a) {
        switch (a.kind) {
        case "mouse": case "xclick": return "Mouse"
        case "key": return "Keyboard"
        case "media": return "Media"
        case "dpi": return "DPI"
        case "macro": return "Macro"
        case "pollcycle": case "lodcycle": case "hypercycle": case "profilecycle": return "Cycles"
        case "disabled": return "Off"
        }
        return "Mouse"
    }
    function openFor(id, name, act) {
        buttonId = id
        buttonName = name
        action = JSON.parse(JSON.stringify(act))
        cat = catOf(action)
        open()
    }
    function setAction(a) { action = a }

    Segmented {
        model: root.categories
        currentIndex: root.categories.indexOf(root.cat)
        minSegmentWidth: 62
        onActivated: (i) => root.cat = root.categories[i]
    }

    // Current choice preview
    Rectangle {
        width: parent.width
        height: 44
        radius: 10
        color: Theme.tertiaryFill
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 14
            anchors.rightMargin: 14
            FText { text: "Will do"; color: Theme.secondaryLabel; font.pixelSize: Theme.footnote }
            Item { Layout.fillWidth: true }
            FText {
                text: root.device ? root.device.describeAction(root.action) : ""
                font.weight: Font.DemiBold
            }
        }
    }

    // Mouse
    Column {
        visible: root.cat === "Mouse"
        width: parent.width
        spacing: 10
        Flow {
            width: parent.width
            spacing: 8
            Repeater {
                model: [
                    { t: "Left Click", a: { kind: "mouse", code: 1 } }, { t: "Right Click", a: { kind: "mouse", code: 2 } },
                    { t: "Middle Click", a: { kind: "mouse", code: 3 } }, { t: "Back", a: { kind: "mouse", code: 4 } },
                    { t: "Forward", a: { kind: "mouse", code: 5 } }, { t: "Scroll Up", a: { kind: "mouse", code: 16 } },
                    { t: "Scroll Down", a: { kind: "mouse", code: 17 } }, { t: "Double Click", a: { kind: "xclick", button: 1, count: 2, interval: 100 } }
                ]
                delegate: ChoiceChip {
                    required property var modelData
                    text: modelData.t
                    on: JSON.stringify(root.action) === JSON.stringify(modelData.a)
                    onClicked: root.setAction(modelData.a)
                }
            }
        }
        ChoiceChip {
            text: "Rapid fire / multi-click…"
            on: root.action.kind === "xclick" && root.action.count !== 2
            onClicked: root.setAction({ kind: "xclick", button: 1, count: 3, interval: 30 })
        }
        Card {
            visible: root.action.kind === "xclick"
            SettingRow {
                title: "Clicks per press"
                NumberField { value: root.action.count || 3; from: 2; to: 50; onCommitted: (v) => root.setAction(Object.assign({}, root.action, { count: Math.round(v) })) }
            }
            SettingRow {
                title: "Interval"
                NumberField { value: root.action.interval || 30; from: 10; to: 1000; stepSize: 5; suffix: "ms"; onCommitted: (v) => root.setAction(Object.assign({}, root.action, { interval: Math.round(v) })) }
            }
        }
    }

    // Keyboard
    Column {
        visible: root.cat === "Keyboard"
        width: parent.width
        spacing: 10
        FText { text: "Press the key or shortcut you want this button to send."; color: Theme.secondaryLabel; font.pixelSize: Theme.callout }
        KeyCapture {
            width: parent.width
            label: root.action.kind === "key" ? root.device.describeAction(root.action) + "  ·  click to change" : "Click, then press a key or shortcut"
            onCaptured: (usage, mods) => {
                if (HidKeys.isModifierUsage(usage)) root.setAction({ kind: "key", modifiers: HidKeys.modifierBit(usage), usage: 0 })
                else root.setAction({ kind: "key", modifiers: mods, usage: usage })
            }
        }
        Row {
            spacing: 14
            Repeater {
                model: [{ n: "Ctrl", b: 1 }, { n: "Shift", b: 2 }, { n: "Alt", b: 4 }, { n: "Super", b: 8 }]
                delegate: Row {
                    required property var modelData
                    spacing: 6
                    Toggle {
                        checked: root.action.kind === "key" && (root.action.modifiers & modelData.b)
                        onToggled: (v) => {
                            const a = root.action.kind === "key" ? root.action : { kind: "key", modifiers: 0, usage: 0 }
                            root.setAction(Object.assign({}, a, { modifiers: v ? (a.modifiers | modelData.b) : (a.modifiers & ~modelData.b) }))
                        }
                    }
                    FText { text: modelData.n; font.pixelSize: Theme.callout; anchors.verticalCenter: parent.verticalCenter }
                }
            }
        }
    }

    // Media
    Flow {
        visible: root.cat === "Media"
        width: parent.width
        spacing: 8
        Repeater {
            model: HidKeys.consumerKeys()
            delegate: ChoiceChip {
                required property var modelData
                text: modelData.name
                on: root.action.kind === "media" && root.action.usage === modelData.usage
                onClicked: root.setAction({ kind: "media", usage: modelData.usage })
            }
        }
    }

    // DPI
    Column {
        visible: root.cat === "DPI"
        width: parent.width
        spacing: 10
        Flow {
            width: parent.width
            spacing: 8
            Repeater {
                model: [{ t: "DPI Cycle", op: "cycle" }, { t: "DPI Up", op: "up" }, { t: "DPI Down", op: "down" }, { t: "DPI Lock (sniper)", op: "lock" }]
                delegate: ChoiceChip {
                    required property var modelData
                    text: modelData.t
                    on: root.action.kind === "dpi" && root.action.op === modelData.op
                    onClicked: root.setAction(modelData.op === "lock" ? { kind: "dpi", op: "lock", dpi: 400 } : { kind: "dpi", op: modelData.op })
                }
            }
        }
        Card {
            visible: root.action.kind === "dpi" && root.action.op === "lock"
            SettingRow {
                title: "DPI while held"
                subtitle: "Drops to this DPI while you hold the button, for precise aiming."
                NumberField { value: root.action.dpi || 400; from: 50; to: 30000; stepSize: 50; suffix: "DPI"; onCommitted: (v) => root.setAction({ kind: "dpi", op: "lock", dpi: Math.round(v) }) }
            }
        }
    }

    // Macro
    Column {
        visible: root.cat === "Macro"
        width: parent.width
        spacing: 10
        FText {
            visible: !root.device || root.device.macros.length === 0
            text: "No macros on the mouse yet. Create one on the Macros page first."
            color: Theme.secondaryLabel
            font.pixelSize: Theme.callout
        }
        Flow {
            width: parent.width
            spacing: 8
            Repeater {
                model: root.device ? root.device.macros : []
                delegate: ChoiceChip {
                    required property var modelData
                    text: modelData.name
                    on: root.action.kind === "macro" && root.action.id === modelData.id
                    onClicked: root.setAction({ kind: "macro", id: modelData.id, mode: root.action.mode || "times", count: root.action.count || 1 })
                }
            }
        }
        Segmented {
            visible: root.action.kind === "macro"
            model: ["Run N times", "While held", "Toggle"]
            currentIndex: ["times", "hold", "toggle"].indexOf(root.action.mode || "times")
            onActivated: (i) => root.setAction(Object.assign({}, root.action, { mode: ["times", "hold", "toggle"][i] }))
        }
        Card {
            visible: root.action.kind === "macro" && (root.action.mode || "times") === "times"
            SettingRow {
                title: "Repeat"
                NumberField { value: root.action.count || 1; from: 1; to: 255; suffix: "×"; onCommitted: (v) => root.setAction(Object.assign({}, root.action, { count: Math.round(v) })) }
            }
        }
    }

    // Cycles
    Flow {
        visible: root.cat === "Cycles"
        width: parent.width
        spacing: 8
        Repeater {
            model: [
                { t: "Cycle polling rate", k: "pollcycle" }, { t: "Cycle lift-off distance", k: "lodcycle" },
                { t: "Toggle competition mode", k: "hypercycle" }, { t: "Cycle profiles", k: "profilecycle" }
            ]
            delegate: ChoiceChip {
                required property var modelData
                text: modelData.t
                on: root.action.kind === modelData.k
                onClicked: root.setAction({ kind: modelData.k })
            }
        }
    }

    // Off
    Column {
        visible: root.cat === "Off"
        width: parent.width
        spacing: 10
        ChoiceChip {
            text: "Disable this button"
            on: root.action.kind === "disabled"
            onClicked: root.setAction({ kind: "disabled" })
        }
    }

    footer: [
        FButton { text: "Cancel"; onClicked: root.close() },
        FButton { text: "Apply"; style: "filled"; onClicked: { root.apply(root.buttonId, root.action); root.close() } }
    ]
}
