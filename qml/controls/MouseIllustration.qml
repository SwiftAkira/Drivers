import QtQuick
import QtQuick.Shapes
import Flux

// Top-down line drawing of a symmetrical mouse with clickable button zones.
// Button ids follow the LAMZU protocol: 1 L, 2 R, 3 wheel, 4 back, 5 forward, 20 DPI.
Item {
    id: root
    property int selected: -1
    property int hovered: -1
    property bool interactive: true
    property color bodyColor: "#26262a"
    signal picked(int id)

    implicitWidth: 220
    implicitHeight: 340

    readonly property real s: Math.min(width / 220, height / 340)
    readonly property real ox: (width - 220 * s) / 2
    readonly property real oy: (height - 340 * s) / 2

    function zoneFill(id) {
        if (id === selected) return Theme.alpha(Theme.accent, 0.55)
        if (id === hovered) return Theme.alpha(Theme.accent, 0.22)
        return "transparent"
    }

    Item {
        x: root.ox
        y: root.oy
        width: 220
        height: 340
        scale: root.s
        transformOrigin: Item.TopLeft

        // Body
        Shape {
            anchors.fill: parent
            preferredRendererType: Shape.CurveRenderer
            ShapePath {
                strokeColor: Theme.alpha(Theme.label, 0.55)
                strokeWidth: 2
                fillGradient: LinearGradient {
                    x1: 0; y1: 0; x2: 0; y2: 340
                    GradientStop { position: 0; color: Qt.lighter(root.bodyColor, 1.25) }
                    GradientStop { position: 1; color: root.bodyColor }
                }
                startX: 110; startY: 12
                PathCubic { x: 196; y: 120; control1X: 168; control1Y: 12; control2X: 196; control2Y: 60 }
                PathCubic { x: 186; y: 270; control1X: 196; control1Y: 180; control2X: 194; control2Y: 230 }
                PathCubic { x: 110; y: 330; control1X: 176; control1Y: 316; control2X: 146; control2Y: 330 }
                PathCubic { x: 34; y: 270; control1X: 74; control1Y: 330; control2X: 44; control2Y: 316 }
                PathCubic { x: 24; y: 120; control1X: 26; control1Y: 230; control2X: 24; control2Y: 180 }
                PathCubic { x: 110; y: 12; control1X: 24; control1Y: 60; control2X: 52; control2Y: 12 }
            }
        }

        // Left / right button zones (fill only)
        Shape {
            anchors.fill: parent
            preferredRendererType: Shape.CurveRenderer
            ShapePath {
                strokeColor: "transparent"
                fillColor: root.zoneFill(1)
                startX: 106; startY: 14
                PathLine { x: 106; y: 140 }
                PathLine { x: 26; y: 140 }
                PathCubic { x: 106; y: 14; control1X: 25; control1Y: 70; control2X: 54; control2Y: 15 }
            }
            ShapePath {
                strokeColor: "transparent"
                fillColor: root.zoneFill(2)
                startX: 114; startY: 14
                PathLine { x: 114; y: 140 }
                PathLine { x: 194; y: 140 }
                PathCubic { x: 114; y: 14; control1X: 195; control1Y: 70; control2X: 166; control2Y: 15 }
            }
        }

        // Seam lines
        Shape {
            anchors.fill: parent
            preferredRendererType: Shape.CurveRenderer
            ShapePath {
                strokeColor: Theme.alpha(Theme.label, 0.35)
                strokeWidth: 1.5
                fillColor: "transparent"
                startX: 110; startY: 14
                PathLine { x: 110; y: 140 }
            }
            ShapePath {
                strokeColor: Theme.alpha(Theme.label, 0.22)
                strokeWidth: 1.5
                fillColor: "transparent"
                startX: 25; startY: 140
                PathCubic { x: 195; y: 140; control1X: 80; control1Y: 152; control2X: 140; control2Y: 152 }
            }
        }

        // Wheel
        Rectangle {
            x: 101; y: 52; width: 18; height: 46; radius: 9
            color: root.selected === 3 ? Theme.accent : (root.hovered === 3 ? Theme.alpha(Theme.accent, 0.4) : "#3a3a3f")
            border.color: Theme.alpha(Theme.label, 0.5)
            border.width: 1.5
            Repeater {
                model: 5
                Rectangle { x: 4; y: 8 + index * 7; width: 10; height: 1.5; color: Theme.alpha(Theme.label, 0.3) }
            }
        }
        // DPI button (behind the wheel)
        Rectangle {
            x: 103; y: 110; width: 14; height: 20; radius: 5
            color: root.selected === 20 ? Theme.accent : (root.hovered === 20 ? Theme.alpha(Theme.accent, 0.4) : "#3a3a3f")
            border.color: Theme.alpha(Theme.label, 0.5)
            border.width: 1.5
        }
        // Side buttons (left side): forward (front) and back (rear)
        Rectangle {
            x: 14; y: 150; width: 14; height: 40; radius: 6
            color: root.selected === 5 ? Theme.accent : (root.hovered === 5 ? Theme.alpha(Theme.accent, 0.4) : "#3a3a3f")
            border.color: Theme.alpha(Theme.label, 0.5)
            border.width: 1.5
        }
        Rectangle {
            x: 16; y: 196; width: 14; height: 40; radius: 6
            color: root.selected === 4 ? Theme.accent : (root.hovered === 4 ? Theme.alpha(Theme.accent, 0.4) : "#3a3a3f")
            border.color: Theme.alpha(Theme.label, 0.5)
            border.width: 1.5
        }

        // Hit areas
        Repeater {
            model: root.interactive ? [
                { id: 3, x: 96, y: 48, w: 28, h: 54 },
                { id: 20, x: 98, y: 106, w: 24, h: 28 },
                { id: 1, x: 26, y: 14, w: 80, h: 126 },
                { id: 2, x: 114, y: 14, w: 80, h: 126 },
                { id: 5, x: 4, y: 146, w: 34, h: 46 },
                { id: 4, x: 4, y: 194, w: 34, h: 46 }
            ] : []
            delegate: MouseArea {
                x: modelData.x; y: modelData.y; width: modelData.w; height: modelData.h
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onContainsMouseChanged: if (containsMouse) root.hovered = modelData.id; else if (root.hovered === modelData.id) root.hovered = -1
                onClicked: root.picked(modelData.id)
            }
        }
    }
}
