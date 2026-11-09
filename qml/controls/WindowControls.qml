import QtQuick
import Flux

// macOS traffic lights. Glyphs appear when hovering the group.
Row {
    id: root
    required property var window
    spacing: 8

    HoverHandler { id: groupHover }

    Repeater {
        model: [
            { color: "#ff5f57", glyph: "x", action: "close" },
            { color: "#febc2e", glyph: "minus", action: "minimize" },
            { color: "#28c840", glyph: "plus", action: "zoom" }
        ]
        delegate: Rectangle {
            width: 12
            height: 12
            radius: 6
            color: root.window.active || groupHover.hovered ? modelData.color : Theme.fill
            border.width: 0.5
            border.color: Qt.rgba(0, 0, 0, 0.2)
            scale: dot.pressed ? 0.88 : 1
            Behavior on scale { SpringPress {} }
            Icon {
                anchors.centerIn: parent
                name: modelData.glyph
                size: 9
                color: Qt.rgba(0, 0, 0, 0.6)
                opacity: groupHover.hovered ? 1 : 0
                Behavior on opacity { NumberAnimation { duration: Motion.fast } }
            }
            MouseArea {
                id: dot
                anchors.fill: parent
                anchors.margins: -3
                onClicked: {
                    if (modelData.action === "close") root.window.close()
                    else if (modelData.action === "minimize") root.window.showMinimized()
                    else root.window.visibility === Window.Maximized ? root.window.showNormal() : root.window.showMaximized()
                }
            }
        }
    }
}
