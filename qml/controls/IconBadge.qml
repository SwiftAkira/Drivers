import QtQuick
import Flux

// Colored rounded-square with a white glyph (System Settings sidebar style).
Rectangle {
    id: root
    property string icon
    property color tint: Theme.accent
    property int size: 22

    width: size
    height: size
    radius: size * 0.26
    gradient: Gradient {
        GradientStop { position: 0.0; color: Qt.lighter(root.tint, 1.12) }
        GradientStop { position: 1.0; color: root.tint }
    }
    Icon {
        anchors.centerIn: parent
        name: root.icon
        size: Math.round(root.size * 0.64)
        color: "white"
    }
}
