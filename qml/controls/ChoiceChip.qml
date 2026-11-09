import QtQuick
import Flux

Rectangle {
    id: root
    property string text
    property bool on: false
    signal clicked()

    implicitWidth: label.implicitWidth + 26
    implicitHeight: 32
    radius: 16
    color: on ? Theme.accent : (hov.hovered ? Theme.secondaryFill : Theme.tertiaryFill)
    border.color: on ? Qt.lighter(Theme.accent, 1.2) : "transparent"
    Behavior on color { ColorAnimation { duration: Motion.fast } }
    scale: tap.pressed ? 0.94 : 1
    Behavior on scale { SpringPress {} }

    FText {
        id: label
        anchors.centerIn: parent
        text: root.text
        color: root.on ? "white" : Theme.label
        font.pixelSize: Theme.callout
        font.weight: root.on ? Font.DemiBold : Font.Medium
    }
    HoverHandler { id: hov; cursorShape: Qt.PointingHandCursor }
    TapHandler { id: tap; onTapped: root.clicked() }
}
