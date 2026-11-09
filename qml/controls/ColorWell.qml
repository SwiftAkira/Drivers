import QtQuick
import QtQuick.Templates as T
import QtQuick.Layouts
import Flux

// Round swatch that opens an HSV picker popover with Apple preset colors.
Item {
    id: root
    property color color: "#ff0000"
    property int size: 24
    signal picked(color color)

    implicitWidth: size
    implicitHeight: size

    Rectangle {
        anchors.fill: parent
        radius: width / 2
        color: "transparent"
        border.width: 2
        border.color: popup.opened ? Theme.accent : Theme.hairline
    }
    Rectangle {
        anchors.fill: parent
        anchors.margins: 3
        radius: width / 2
        color: root.color
        scale: tap.pressed ? 0.9 : 1
        Behavior on scale { SpringPress {} }
    }
    TapHandler { id: tap; onTapped: popup.open() }
    HoverHandler { cursorShape: Qt.PointingHandCursor }

    T.Popup {
        id: popup
        y: root.height + 6
        x: Math.min(0, -(width - root.width) / 2)
        width: 248
        padding: 14
        implicitHeight: implicitContentHeight + topPadding + bottomPadding
        transformOrigin: T.Popup.Top
        focus: true

        property real h: 0
        property real s: 1
        property real v: 1
        function syncFrom(c) {
            h = Math.max(0, c.hsvHue); s = c.hsvSaturation; v = c.hsvValue
        }
        function emitColor() { root.picked(Qt.hsva(h, s, v, 1)) }
        onAboutToShow: syncFrom(root.color)

        enter: Transition {
            ParallelAnimation {
                NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Motion.fast }
                SpringAnimation { property: "scale"; from: 0.9; to: 1; spring: Motion.springStiff; damping: Motion.springDampStiff; epsilon: Motion.epsilon }
            }
        }
        exit: Transition { NumberAnimation { property: "opacity"; to: 0; duration: Motion.fast } }

        background: Rectangle { radius: 12; color: Theme.popover; border.color: Theme.hairline }

        contentItem: ColumnLayout {
            spacing: 12

            // Saturation / value square
            Rectangle {
                id: sv
                Layout.fillWidth: true
                Layout.preferredHeight: 140
                radius: 8
                color: Qt.hsva(popup.h, 1, 1, 1)
                Rectangle {
                    anchors.fill: parent
                    radius: 8
                    gradient: Gradient {
                        orientation: Gradient.Horizontal
                        GradientStop { position: 0; color: "white" }
                        GradientStop { position: 1; color: Qt.rgba(1, 1, 1, 0) }
                    }
                }
                Rectangle {
                    anchors.fill: parent
                    radius: 8
                    gradient: Gradient {
                        GradientStop { position: 0; color: Qt.rgba(0, 0, 0, 0) }
                        GradientStop { position: 1; color: "black" }
                    }
                }
                Rectangle {
                    width: 16; height: 16; radius: 8
                    x: popup.s * sv.width - 8
                    y: (1 - popup.v) * sv.height - 8
                    color: Qt.hsva(popup.h, popup.s, popup.v, 1)
                    border.width: 2
                    border.color: "white"
                }
                MouseArea {
                    anchors.fill: parent
                    function upd(m) {
                        popup.s = Math.max(0, Math.min(1, m.x / width))
                        popup.v = Math.max(0, Math.min(1, 1 - m.y / height))
                    }
                    onPressed: (m) => upd(m)
                    onPositionChanged: (m) => upd(m)
                    onReleased: popup.emitColor()
                }
            }

            // Hue strip
            Rectangle {
                id: hue
                Layout.fillWidth: true
                Layout.preferredHeight: 14
                radius: 7
                gradient: Gradient {
                    orientation: Gradient.Horizontal
                    GradientStop { position: 0.000; color: "#ff0000" }
                    GradientStop { position: 0.167; color: "#ffff00" }
                    GradientStop { position: 0.333; color: "#00ff00" }
                    GradientStop { position: 0.500; color: "#00ffff" }
                    GradientStop { position: 0.667; color: "#0000ff" }
                    GradientStop { position: 0.833; color: "#ff00ff" }
                    GradientStop { position: 1.000; color: "#ff0000" }
                }
                Rectangle {
                    width: 18; height: 18; radius: 9
                    y: -2
                    x: popup.h * hue.width - 9
                    color: Qt.hsva(popup.h, 1, 1, 1)
                    border.width: 2
                    border.color: "white"
                }
                MouseArea {
                    anchors.fill: parent
                    anchors.margins: -4
                    function upd(m) { popup.h = Math.max(0, Math.min(0.999, (m.x - 4) / hue.width)) }
                    onPressed: (m) => upd(m)
                    onPositionChanged: (m) => upd(m)
                    onReleased: popup.emitColor()
                }
            }

            // Presets
            Flow {
                Layout.fillWidth: true
                spacing: 8
                Repeater {
                    model: ["#ff453a", "#ff9f0a", "#ffd60a", "#30d158", "#63e6e2", "#64d2ff",
                            "#0a84ff", "#5e5ce6", "#bf5af2", "#ff375f", "#ffffff", "#8e8e93"]
                    delegate: Rectangle {
                        width: 18; height: 18; radius: 9
                        color: modelData
                        border.width: 1
                        border.color: Theme.hairline
                        scale: presetTap.pressed ? 0.85 : 1
                        Behavior on scale { SpringPress {} }
                        TapHandler {
                            id: presetTap
                            onTapped: { popup.syncFrom(Qt.color(modelData)); popup.emitColor() }
                        }
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                FText { text: "Hex"; color: Theme.secondaryLabel; font.pixelSize: Theme.footnote }
                FTextField {
                    Layout.fillWidth: true
                    implicitHeight: 26
                    text: Qt.hsva(popup.h, popup.s, popup.v, 1).toString().toUpperCase()
                    validator: RegularExpressionValidator { regularExpression: /#?[0-9a-fA-F]{6}/ }
                    onEditingFinished: {
                        const t = text.startsWith("#") ? text : "#" + text
                        popup.syncFrom(Qt.color(t)); popup.emitColor()
                    }
                }
            }
        }
    }
}
