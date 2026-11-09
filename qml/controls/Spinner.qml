import QtQuick
import Flux

// Apple-style activity indicator (12 fading spokes).
Item {
    id: root
    property int size: 22
    property color color: Theme.secondaryLabel
    property bool running: visible
    implicitWidth: size
    implicitHeight: size

    property int step: 0
    Timer {
        interval: 80
        repeat: true
        running: root.running
        onTriggered: root.step = (root.step + 1) % 12
    }
    Repeater {
        model: 12
        Rectangle {
            width: Math.max(2, root.size * 0.09)
            height: root.size * 0.28
            radius: width / 2
            color: root.color
            x: (root.size - width) / 2
            y: 0
            opacity: 0.18 + 0.82 * (((index - root.step + 12) % 12) / 11)
            transform: Rotation {
                origin.x: width / 2
                origin.y: root.size / 2
                angle: index * 30
            }
        }
    }
}
