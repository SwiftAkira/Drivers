import QtQuick
import Flux

// Apple-style battery glyph.
Item {
    id: root
    property int level: 100      // -1 = unknown
    property bool charging: false
    property real glyphScale: 1.0

    implicitWidth: 27 * glyphScale
    implicitHeight: 13 * glyphScale

    readonly property color fillColor: charging ? Theme.green
                                     : level >= 0 && level <= 10 ? Theme.red
                                     : level >= 0 && level <= 20 ? Theme.orange
                                     : Theme.label

    Rectangle {
        id: body
        width: 24 * root.glyphScale
        height: root.height
        radius: 3.6 * root.glyphScale
        color: "transparent"
        border.width: 1
        border.color: Theme.alpha(Theme.label, 0.45)
    }
    Rectangle {
        x: body.width + 1
        width: 2 * root.glyphScale
        height: 4.5 * root.glyphScale
        radius: 1
        anchors.verticalCenter: body.verticalCenter
        color: Theme.alpha(Theme.label, 0.45)
    }
    Rectangle {
        x: 2 * root.glyphScale
        y: 2 * root.glyphScale
        height: body.height - 4 * root.glyphScale
        width: Math.max(1.5, (body.width - 4 * root.glyphScale) * Math.max(0, root.level) / 100)
        radius: 2 * root.glyphScale
        color: root.fillColor
        Behavior on width { SpringSoft {} }
        Behavior on color { ColorAnimation { duration: Motion.normal } }
    }
    Icon {
        visible: root.charging
        anchors.centerIn: body
        name: "zap"
        size: 10 * root.glyphScale
        color: "white"
    }
}
