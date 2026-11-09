import QtQuick
import Flux

// Gallery of effect cards with an animated palette swatch.
Item {
    id: root
    property var effects: []
    property string current: ""
    signal picked(string id)

    readonly property int columns: Math.max(2, Math.floor((width + 10) / 190))
    readonly property real cardW: (width - (columns - 1) * 10) / columns
    implicitHeight: flow.implicitHeight

    Flow {
        id: flow
        width: parent.width
        spacing: 10
        Repeater {
            model: root.effects
            delegate: Rectangle {
                id: card
                required property var modelData
                readonly property bool on: root.current === modelData.id
                width: root.cardW
                height: 108
                radius: 14
                color: on ? Theme.alpha(Theme.accent, 0.16) : (hov.hovered ? Theme.cardHover : Theme.card)
                border.width: on ? 2 : 1
                border.color: on ? Theme.accent : Qt.rgba(1, 1, 1, 0.05)
                scale: tap.pressed ? 0.96 : (hov.hovered ? 1.015 : 1)
                Behavior on scale { SpringPress {} }
                Behavior on color { ColorAnimation { duration: Motion.fast } }

                // Animated swatch: palette gradient sliding sideways
                Rectangle {
                    id: swatch
                    x: 12; y: 12
                    width: parent.width - 24
                    height: 34
                    radius: 8
                    clip: true
                    color: "#000"
                    Row {
                        id: strip
                        height: parent.height
                        x: -swatch.width * shift
                        property real shift: 0
                        NumberAnimation on shift { from: 0; to: 1; duration: 4000; loops: Animation.Infinite; running: card.on || hov.hovered }
                        Repeater {
                            model: 2
                            Rectangle {
                                width: swatch.width
                                height: swatch.height
                                gradient: Gradient {
                                    orientation: Gradient.Horizontal
                                    GradientStop { position: 0.0; color: card.modelData.palette[0] }
                                    GradientStop { position: 0.33; color: card.modelData.palette[1] }
                                    GradientStop { position: 0.66; color: card.modelData.palette[2] }
                                    GradientStop { position: 1.0; color: card.modelData.palette[0] }
                                }
                            }
                        }
                    }
                    Rectangle {
                        visible: card.modelData.reactive || card.modelData.audio
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.margins: 5
                        width: 20; height: 20; radius: 10
                        color: Qt.rgba(0, 0, 0, 0.45)
                        Icon { anchors.centerIn: parent; name: card.modelData.audio ? "music" : "zap"; size: 12; color: "white" }
                    }
                }
                FText {
                    x: 12; y: 54
                    width: parent.width - 24
                    text: card.modelData.name
                    font.pixelSize: Theme.callout
                    font.weight: Font.DemiBold
                }
                FText {
                    x: 12; y: 72
                    width: parent.width - 24
                    height: 30
                    text: card.modelData.blurb
                    color: Theme.secondaryLabel
                    font.pixelSize: Theme.caption
                    wrapMode: Text.WordWrap
                    elide: Text.ElideRight
                    maximumLineCount: 2
                    verticalAlignment: Text.AlignTop
                }
                HoverHandler { id: hov; cursorShape: Qt.PointingHandCursor }
                TapHandler { id: tap; onTapped: root.picked(card.modelData.id) }
            }
        }
    }
}
