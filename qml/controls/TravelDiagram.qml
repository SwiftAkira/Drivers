import QtQuick
import Flux

// Side view of a key's travel: actuation point, rapid-trigger window and
// dead zones, with an optional live position marker.
Item {
    id: root
    property double travelMax: 3.5
    property double actuation: 1.2
    property bool rtOn: false
    property double rtPress: 0.3
    property double rtRelease: 0.3
    property double deadTop: 0
    property double deadBottom: 0
    property double live: -1

    implicitWidth: 150
    implicitHeight: 220

    readonly property real railTop: 34
    readonly property real railBottom: height - 18
    function yFor(mm) { return railTop + (railBottom - railTop) * Math.max(0, Math.min(1, mm / travelMax)) }

    // Keycap (moves with live travel)
    Rectangle {
        id: capTop
        x: 34
        width: root.width - 68
        height: 18
        radius: 5
        y: (root.live >= 0 ? root.yFor(root.live) : root.railTop) - height - 4
        color: "#3a3a3f"
        border.color: Theme.hairline
        Behavior on y { SpringStiff {} }
    }
    // Stem
    Rectangle {
        x: root.width / 2 - 3
        width: 6
        y: capTop.y + capTop.height
        height: root.railBottom - y
        radius: 3
        color: "#2a2a2e"
    }
    // Travel rail
    Rectangle {
        x: 22
        y: root.railTop
        width: 4
        height: root.railBottom - root.railTop
        radius: 2
        color: Theme.fill
    }
    // Dead zones
    Rectangle {
        x: 22; width: 4; radius: 2
        y: root.railTop
        height: root.yFor(root.deadTop) - root.railTop
        color: Theme.gray
        Behavior on height { SpringStiff {} }
    }
    Rectangle {
        x: 22; width: 4; radius: 2
        y: root.yFor(root.travelMax - root.deadBottom)
        height: root.railBottom - y
        color: Theme.gray
        Behavior on y { SpringStiff {} }
    }
    // RT window around the actuation point
    Rectangle {
        visible: root.rtOn
        x: 18
        width: 12
        radius: 3
        y: root.yFor(Math.max(0, root.actuation - root.rtRelease))
        height: root.yFor(Math.min(root.travelMax, root.actuation + root.rtPress)) - y
        color: Theme.alpha(Theme.purple, 0.45)
        Behavior on y { SpringStiff {} }
        Behavior on height { SpringStiff {} }
    }
    // Actuation marker
    Item {
        y: root.yFor(root.actuation)
        width: root.width
        Behavior on y { SpringStiff {} }
        Rectangle { x: 14; y: -1; width: root.width - 28; height: 2; color: Theme.accent; radius: 1 }
        Rectangle { x: 18; y: -6; width: 12; height: 12; radius: 6; color: Theme.accent; border.color: "white"; border.width: 2 }
        FText {
            x: root.width - 54
            y: -18
            text: root.actuation.toFixed(2) + " mm"
            color: Theme.accent
            font.pixelSize: Theme.footnote
            font.weight: Font.DemiBold
        }
    }
    // Live marker
    Rectangle {
        visible: root.live >= 0
        x: 14
        y: root.yFor(Math.max(0, root.live)) - 1
        width: 20
        height: 3
        radius: 1.5
        color: root.live >= root.actuation ? Theme.green : Theme.secondaryLabel
    }
    FText { x: 0; y: root.railTop - 8; text: "0"; color: Theme.tertiaryLabel; font.pixelSize: Theme.caption }
    FText { x: 0; y: root.railBottom - 8; text: root.travelMax.toFixed(1); color: Theme.tertiaryLabel; font.pixelSize: Theme.caption }
}
