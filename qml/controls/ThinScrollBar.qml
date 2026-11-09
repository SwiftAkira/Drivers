import QtQuick
import QtQuick.Templates as T
import Flux

// Overlay scrollbar that only shows while scrolling or hovered.
T.ScrollBar {
    id: root
    implicitWidth: 12
    padding: 3
    minimumSize: 0.08
    policy: T.ScrollBar.AsNeeded

    contentItem: Rectangle {
        implicitWidth: root.hovered || root.pressed ? 7 : 5
        radius: width / 2
        color: Theme.alpha(Theme.label, root.pressed ? 0.5 : 0.32)
        opacity: root.active || root.hovered ? 1 : 0
        Behavior on opacity { NumberAnimation { duration: Motion.slow } }
        Behavior on implicitWidth { NumberAnimation { duration: Motion.fast } }
    }
}
