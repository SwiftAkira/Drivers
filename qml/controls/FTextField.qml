import QtQuick
import QtQuick.Templates as T
import Flux

T.TextField {
    id: root
    implicitWidth: 180
    implicitHeight: 30
    leftPadding: 10
    rightPadding: 10
    color: Theme.label
    placeholderTextColor: Theme.tertiaryLabel
    selectionColor: Theme.alpha(Theme.accent, 0.5)
    selectedTextColor: "white"
    font.family: Theme.fontFamily
    font.pixelSize: Theme.callout
    font.features: { "tnum": 1 }
    verticalAlignment: TextInput.AlignVCenter
    selectByMouse: true

    background: Rectangle {
        radius: 7
        color: Theme.tertiaryFill
        border.width: root.activeFocus ? 2 : 1
        border.color: root.activeFocus ? Theme.alpha(Theme.accent, 0.8) : Theme.hairline
        Behavior on border.color { ColorAnimation { duration: Motion.fast } }
    }

    FText {
        visible: !root.text && !root.activeFocus
        text: root.placeholderText
        color: root.placeholderTextColor
        font: root.font
        x: root.leftPadding
        anchors.verticalCenter: parent.verticalCenter
    }
}
