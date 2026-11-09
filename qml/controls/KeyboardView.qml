import QtQuick
import Flux

// Renders the board from device.layout. Supports click / shift-click /
// rubber-band selection and several overlay modes.
Item {
    id: root
    property var device: null
    property var selection: []              // key indices
    property bool selectable: true
    property bool multiSelect: true
    property var labelFor: null             // function(index) -> string (main legend)
    property var detailFor: null            // function(index) -> string (small legend under)
    property var fillFor: null              // function(index) -> color (cap tint) or undefined
    property var travel: null               // array of mm for live fill
    property double travelMax: 3.5
    property var ledColors: null            // array of colors drawn as underglow
    property real fillOpacity: 0.55
    property bool dimUnselected: false
    signal keyClicked(int index)

    readonly property var keys: device ? device.layout : []
    readonly property real unitsW: {
        let w = 0
        for (const k of keys) w = Math.max(w, k.x + k.w)
        return Math.max(w, 1)
    }
    readonly property real unitsH: {
        let h = 0
        for (const k of keys) h = Math.max(h, k.y + k.h)
        return Math.max(h, 1)
    }
    readonly property real u: Math.min(width / unitsW, 64)
    implicitHeight: unitsH * u + 1

    function isSelected(i) { return selection.indexOf(i) >= 0 }
    function setSelection(list) { selection = list }
    function toggle(i, additive) {
        if (!multiSelect || !additive) { selection = [i]; return }
        const s = selection.slice()
        const at = s.indexOf(i)
        if (at >= 0) s.splice(at, 1); else s.push(i)
        selection = s
    }
    function selectAll() { selection = keys.map((k) => k.index) }
    function clearSelection() { selection = [] }

    Item {
        id: board
        width: root.unitsW * root.u
        height: root.unitsH * root.u
        anchors.horizontalCenter: parent.horizontalCenter

        Repeater {
            model: root.keys
            delegate: Item {
                id: cap
                required property var modelData
                readonly property int idx: modelData.index
                readonly property bool sel: root.isSelected(idx)
                readonly property real pad: Math.max(2, root.u * 0.06)
                x: modelData.x * root.u + pad / 2
                y: modelData.y * root.u + pad / 2
                width: modelData.w * root.u - pad
                height: modelData.h * root.u - pad
                opacity: root.dimUnselected && root.selection.length > 0 && !sel ? 0.45 : 1
                Behavior on opacity { NumberAnimation { duration: Motion.normal } }
                scale: tap.pressed ? 0.94 : 1
                Behavior on scale { SpringPress {} }

                // LED underglow
                Rectangle {
                    visible: root.ledColors !== null
                    anchors.fill: parent
                    anchors.margins: -2
                    radius: 8
                    color: root.ledColors && root.ledColors[cap.idx] !== undefined ? root.ledColors[cap.idx] : "black"
                    opacity: 0.9
                }

                // Key body (side) + top face
                Rectangle {
                    anchors.fill: parent
                    radius: Math.max(4, root.u * 0.12)
                    color: "#121214"
                }
                Rectangle {
                    id: face
                    anchors.fill: parent
                    anchors.leftMargin: 2
                    anchors.rightMargin: 2
                    anchors.topMargin: 1
                    anchors.bottomMargin: Math.max(3, root.u * 0.08)
                    radius: Math.max(3, root.u * 0.10)
                    readonly property color tint: root.fillFor ? (root.fillFor(cap.idx) || "transparent") : "transparent"
                    gradient: Gradient {
                        GradientStop { position: 0; color: cap.sel ? Qt.lighter(Theme.accent, 1.1) : (hover.hovered ? "#3c3c41" : "#333337") }
                        GradientStop { position: 1; color: cap.sel ? Theme.accent : (hover.hovered ? "#323236" : "#2a2a2e") }
                    }
                    border.width: 1
                    border.color: cap.sel ? Qt.lighter(Theme.accent, 1.3) : Qt.rgba(1, 1, 1, 0.07)

                    Rectangle {
                        anchors.fill: parent
                        radius: parent.radius
                        color: face.tint
                        visible: !cap.sel && face.tint !== "transparent"
                        opacity: root.fillOpacity
                    }

                    // Live travel fill (from the top down, like the key moving)
                    Rectangle {
                        visible: root.travel !== null
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        anchors.margins: 1
                        radius: parent.radius - 1
                        readonly property real frac: root.travel && root.travel[cap.idx] !== undefined ? Math.min(1, root.travel[cap.idx] / root.travelMax) : 0
                        height: (parent.height - 2) * frac
                        color: frac > 0.92 ? Theme.orange : Theme.green
                        opacity: 0.85
                        Behavior on height { NumberAnimation { duration: 60 } }
                    }

                    Column {
                        anchors.centerIn: parent
                        spacing: 0
                        FText {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: root.labelFor ? root.labelFor(cap.idx) : (root.device ? root.device.keyName(cap.idx) : "")
                            font.pixelSize: Math.max(9, Math.min(13, root.u * 0.25))
                            font.weight: Font.Medium
                            color: cap.sel ? "white" : Theme.label
                            width: Math.min(implicitWidth, face.width - 6)
                            horizontalAlignment: Text.AlignHCenter
                        }
                        FText {
                            visible: text !== ""
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: root.detailFor ? root.detailFor(cap.idx) : ""
                            font.pixelSize: Math.max(8, Math.min(11, root.u * 0.19))
                            color: cap.sel ? Qt.rgba(1, 1, 1, 0.85) : Theme.secondaryLabel
                        }
                    }
                }

                HoverHandler { id: hover; enabled: root.selectable; cursorShape: root.selectable ? Qt.PointingHandCursor : Qt.ArrowCursor }
                TapHandler {
                    id: tap
                    enabled: root.selectable
                    onTapped: (point) => {
                        root.toggle(cap.idx, point.modifiers & (Qt.ShiftModifier | Qt.ControlModifier))
                        root.keyClicked(cap.idx)
                    }
                }
            }
        }

        // Rubber-band selection
        Rectangle {
            id: band
            visible: dragSel.active
            color: Theme.alpha(Theme.accent, 0.15)
            border.color: Theme.alpha(Theme.accent, 0.7)
            radius: 4
        }
        DragHandler {
            id: dragSel
            enabled: root.selectable && root.multiSelect
            target: null
            dragThreshold: 6
            property point start
            onActiveChanged: if (active) start = centroid.pressPosition
            onCentroidChanged: {
                if (!active) return
                const x1 = Math.min(start.x, centroid.position.x), y1 = Math.min(start.y, centroid.position.y)
                const x2 = Math.max(start.x, centroid.position.x), y2 = Math.max(start.y, centroid.position.y)
                band.x = x1; band.y = y1; band.width = x2 - x1; band.height = y2 - y1
                const sel = []
                for (const k of root.keys) {
                    const kx1 = k.x * root.u, ky1 = k.y * root.u, kx2 = (k.x + k.w) * root.u, ky2 = (k.y + k.h) * root.u
                    if (kx2 > x1 && kx1 < x2 && ky2 > y1 && ky1 < y2) sel.push(k.index)
                }
                root.selection = sel
            }
        }
    }
}
