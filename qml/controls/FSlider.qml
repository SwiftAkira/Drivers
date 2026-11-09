import QtQuick
import QtQuick.Templates as T
import Flux

// Thin macOS slider. `committed` fires once on release (or keyboard step),
// which is when we write to the device.
T.Slider {
    id: root
    property color tint: Theme.accent
    property int tickCount: 0          // >1 draws tick marks
    signal committed(real value)

    implicitWidth: 220
    implicitHeight: 26
    padding: 0
    leftPadding: 10
    rightPadding: 10
    opacity: enabled ? 1 : 0.4
    wheelEnabled: false

    onPressedChanged: if (!pressed) committed(value)
    Keys.onReleased: (e) => {
        if (e.key === Qt.Key_Left || e.key === Qt.Key_Right || e.key === Qt.Key_Up || e.key === Qt.Key_Down)
            committed(value)
    }

    background: Item {
        x: root.leftPadding
        width: root.availableWidth
        height: root.height
        Rectangle {
            id: track
            width: parent.width
            height: 4
            radius: 2
            anchors.verticalCenter: parent.verticalCenter
            color: Theme.fill
        }
        Rectangle {
            height: 4
            radius: 2
            anchors.verticalCenter: parent.verticalCenter
            width: root.visualPosition * parent.width
            color: root.tint
        }
        Repeater {
            model: root.tickCount > 1 ? root.tickCount : 0
            Rectangle {
                width: 2
                height: 6
                radius: 1
                color: Theme.tertiaryLabel
                x: index / (root.tickCount - 1) * (parent.width - 2)
                y: parent.height / 2 + 6
            }
        }
    }

    handle: Item {
        x: root.leftPadding + root.visualPosition * root.availableWidth - width / 2
        y: (root.height - height) / 2
        width: 20
        height: 20
        Rectangle {
            anchors.fill: parent
            anchors.topMargin: 1
            radius: width / 2
            color: Qt.rgba(0, 0, 0, 0.3)
        }
        Rectangle {
            anchors.fill: parent
            radius: width / 2
            color: "white"
            scale: root.pressed ? 1.14 : (hover.hovered ? 1.05 : 1)
            Behavior on scale { SpringPress {} }
        }
        HoverHandler { id: hover; cursorShape: Qt.PointingHandCursor }
    }
}
