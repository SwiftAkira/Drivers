import QtQuick
import QtQuick.Layouts
import QtQuick.Templates as T
import Flux

// Shared macro editor. kind = "mouse" (LAMZU) or "keyboard" (MAD).
// Steps: {type:"key",usage,down} {type:"tap",usage} {type:"delay",ms}
//        {type:"mouse",button,down} {type:"wheel",up} {type:"text",text}
Sheet {
    id: root
    property string kind: "mouse"
    property int macroId: 0
    property string name: ""
    property var steps: []
    property bool recording: false
    property bool recordDelays: true
    property int defaultDelay: 30
    signal save(int id, string name, var steps)

    title: "Edit Macro"
    sheetWidth: 600

    function openWith(id, nm, evs) {
        macroId = id
        name = nm
        steps = JSON.parse(JSON.stringify(evs || []))
        nameField.text = nm
        recording = false
        open()
    }
    function push(step) {
        const s = steps.slice()
        s.push(step)
        steps = s
        Qt.callLater(() => list.positionViewAtEnd())
    }
    function removeAt(i) { const s = steps.slice(); s.splice(i, 1); steps = s }
    function setAt(i, step) { const s = steps.slice(); s[i] = step; steps = s }
    function label(st) {
        switch (st.type) {
        case "key": return (st.down ? "Press " : "Release ") + HidKeys.hidName(st.usage)
        case "tap": return "Tap " + HidKeys.hidName(st.usage)
        case "delay": return "Wait"
        case "mouse": return (st.down ? "Press " : "Release ") + ["", "left button", "right button", "middle button", "back button", "forward button"][st.button]
        case "wheel": return st.up ? "Scroll up" : "Scroll down"
        case "text": return "Type “" + st.text + "”"
        }
        return st.type
    }
    function iconFor(st) {
        switch (st.type) {
        case "key": return st.down ? "arrow-down-to-line" : "upload"
        case "tap": return "keyboard"
        case "delay": return "timer"
        case "mouse": return "mouse-pointer-click"
        case "wheel": return "arrow-up-down"
        case "text": return "pencil"
        }
        return "circle-dot"
    }

    property double lastEventTime: 0
    function recordEvent(step) {
        const now = Date.now()
        if (steps.length > 0 && lastEventTime > 0) {
            const gap = recordDelays ? Math.min(60000, now - lastEventTime) : defaultDelay
            if (gap > 0) push({ type: "delay", ms: gap })
        }
        lastEventTime = now
        push(step)
    }

    RowLayout {
        width: parent.width
        spacing: 10
        FTextField {
            id: nameField
            Layout.fillWidth: true
            placeholderText: "Macro name"
            onTextChanged: root.name = text
        }
        FButton {
            text: root.recording ? "Stop" : "Record"
            icon: root.recording ? "square" : "circle-dot"
            style: root.recording ? "destructive" : "gray"
            onClicked: {
                root.recording = !root.recording
                root.lastEventTime = 0
                if (root.recording) recorder.forceActiveFocus()
            }
        }
    }

    // Recording surface: captures keys (and mouse clicks for mouse macros).
    Rectangle {
        id: recorder
        width: parent.width
        height: root.recording ? 70 : 0
        visible: height > 0
        radius: 10
        color: Theme.alpha(Theme.red, 0.10)
        border.color: Theme.alpha(Theme.red, 0.5)
        clip: true
        focus: root.recording
        Behavior on height { SpringStiff {} }
        FText {
            anchors.centerIn: parent
            text: root.kind === "mouse" ? "Recording: type keys, or click / scroll inside this box" : "Recording: type on your keyboard"
            color: Theme.secondaryLabel
            font.pixelSize: Theme.callout
        }
        Keys.onPressed: (e) => {
            if (!root.recording || e.isAutoRepeat) return
            e.accepted = true
            const u = HidKeys.hidFromScanCode(e.nativeScanCode)
            if (u > 0) root.recordEvent({ type: "key", usage: u, down: true })
        }
        Keys.onReleased: (e) => {
            if (!root.recording || e.isAutoRepeat) return
            e.accepted = true
            const u = HidKeys.hidFromScanCode(e.nativeScanCode)
            if (u > 0) root.recordEvent({ type: "key", usage: u, down: false })
        }
        MouseArea {
            anchors.fill: parent
            enabled: root.recording && root.kind === "mouse"
            acceptedButtons: Qt.AllButtons
            function btn(b) { return b === Qt.LeftButton ? 1 : b === Qt.RightButton ? 2 : b === Qt.MiddleButton ? 3 : b === Qt.BackButton ? 4 : b === Qt.ForwardButton ? 5 : 0 }
            onPressed: (m) => { recorder.forceActiveFocus(); const b = btn(m.button); if (b) root.recordEvent({ type: "mouse", button: b, down: true }) }
            onReleased: (m) => { const b = btn(m.button); if (b) root.recordEvent({ type: "mouse", button: b, down: false }) }
            onWheel: (w) => root.recordEvent({ type: "wheel", up: w.angleDelta.y > 0 })
        }
    }

    RowLayout {
        width: parent.width
        FText { text: "Record pauses as delays"; font.pixelSize: Theme.footnote; color: Theme.secondaryLabel }
        Toggle { checked: root.recordDelays; onToggled: (v) => root.recordDelays = v }
        Item { Layout.fillWidth: true }
        FText { text: root.steps.length + " steps"; font.pixelSize: Theme.footnote; color: Theme.tertiaryLabel }
    }

    Rectangle {
        width: parent.width
        height: 260
        radius: 10
        color: Theme.card
        border.color: Qt.rgba(1, 1, 1, 0.05)
        ListView {
            id: list
            anchors.fill: parent
            anchors.margins: 4
            clip: true
            model: root.steps
            boundsBehavior: Flickable.StopAtBounds
            T.ScrollBar.vertical: ThinScrollBar {}
            add: Transition {
                ParallelAnimation {
                    NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Motion.normal }
                    SpringAnimation { property: "scale"; from: 0.9; to: 1; spring: Motion.springStiff; damping: Motion.springDampStiff }
                }
            }
            displaced: Transition { SpringAnimation { property: "y"; spring: Motion.springStiff; damping: Motion.springDampStiff } }
            delegate: Item {
                id: stepRow
                required property var modelData
                required property int index
                width: list.width
                height: 38
                Rectangle {
                    anchors.fill: parent
                    anchors.margins: 2
                    radius: 7
                    color: rowHover.hovered ? Theme.quaternaryFill : "transparent"
                }
                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 10
                    anchors.rightMargin: 6
                    spacing: 10
                    FText { text: (stepRow.index + 1) + "."; color: Theme.tertiaryLabel; font.pixelSize: Theme.footnote; Layout.preferredWidth: 24 }
                    Icon { name: root.iconFor(stepRow.modelData); size: 15; color: stepRow.modelData.type === "delay" ? Theme.orange : Theme.accent }
                    FText { text: root.label(stepRow.modelData); Layout.fillWidth: true; font.pixelSize: Theme.callout }
                    NumberField {
                        visible: stepRow.modelData.type === "delay"
                        value: stepRow.modelData.ms || 0
                        from: 0; to: 60000; stepSize: 10
                        suffix: "ms"
                        fieldWidth: 64
                        onCommitted: (v) => root.setAt(stepRow.index, { type: "delay", ms: Math.round(v) })
                    }
                    FButton { compact: true; style: "plain"; icon: "x"; onClicked: root.removeAt(stepRow.index) }
                }
                HoverHandler { id: rowHover }
            }
            FText {
                anchors.centerIn: parent
                visible: root.steps.length === 0
                text: "No steps yet. Press Record, or add steps below."
                color: Theme.tertiaryLabel
                font.pixelSize: Theme.callout
            }
        }
    }

    Flow {
        width: parent.width
        spacing: 6
        FText { text: "Add:"; color: Theme.secondaryLabel; font.pixelSize: Theme.footnote; height: 26; verticalAlignment: Text.AlignVCenter }
        KeyCapture {
            implicitHeight: 26
            label: "Key tap…"
            onCaptured: (u, m) => {
                if (root.kind === "keyboard") root.push({ type: "tap", usage: u })
                else { root.push({ type: "key", usage: u, down: true }); root.push({ type: "delay", ms: root.defaultDelay }); root.push({ type: "key", usage: u, down: false }) }
            }
        }
        FButton { compact: true; icon: "timer"; text: "Delay"; onClicked: root.push({ type: "delay", ms: 50 }) }
        FButton { visible: root.kind === "mouse"; compact: true; icon: "mouse-pointer-click"; text: "Left click"; onClicked: { root.push({ type: "mouse", button: 1, down: true }); root.push({ type: "delay", ms: root.defaultDelay }); root.push({ type: "mouse", button: 1, down: false }) } }
        FButton { visible: root.kind === "mouse"; compact: true; icon: "arrow-up-down"; text: "Scroll"; onClicked: root.push({ type: "wheel", up: false }) }
        FButton { visible: root.kind === "keyboard"; compact: true; icon: "pencil"; text: "Text"; onClicked: textSheet.open() }
        FButton { compact: true; style: "plain"; icon: "trash-2"; text: "Clear"; enabled: root.steps.length > 0; onClicked: root.steps = [] }
    }

    footer: [
        FButton { text: "Cancel"; onClicked: { root.recording = false; root.close() } },
        FButton {
            text: "Save to " + (root.kind === "mouse" ? "Mouse" : "Keyboard")
            style: "filled"
            enabled: root.steps.length > 0
            onClicked: {
                root.recording = false
                root.save(root.macroId, root.name || ("Macro " + (root.kind === "mouse" ? root.macroId : root.macroId + 1)), root.steps)
                root.close()
            }
        }
    ]

    Sheet {
        id: textSheet
        title: "Type text"
        FTextField { id: textField; width: parent.width; placeholderText: "Text to type (ASCII)" }
        footer: [
            FButton { text: "Cancel"; onClicked: textSheet.close() },
            FButton { text: "Add"; style: "filled"; onClicked: { if (textField.text) root.push({ type: "text", text: textField.text }); textField.text = ""; textSheet.close() } }
        ]
    }
}
