import QtQuick
import Flux

// Button styles: "filled" (accent), "gray", "plain" (accent text), "destructive".
Rectangle {
    id: root
    property string text
    property string icon
    property string style: "gray"
    property bool compact: false
    signal clicked()

    implicitHeight: compact ? 26 : 32
    implicitWidth: Math.max(implicitHeight, content.implicitWidth + (text ? (compact ? 20 : 28) : 0) + (text ? 0 : 4))
    radius: Theme.radiusControl
    color: {
        if (style === "filled") return mouse.pressed ? Theme.accentPressed : Theme.accent
        if (style === "plain") return mouse.containsMouse ? Theme.quaternaryFill : "transparent"
        if (style === "destructive") return mouse.pressed ? Theme.alpha(Theme.red, 0.3) : Theme.alpha(Theme.red, mouse.containsMouse ? 0.22 : 0.16)
        return mouse.pressed ? Theme.fill : (mouse.containsMouse ? Theme.secondaryFill : Theme.tertiaryFill)
    }
    Behavior on color { ColorAnimation { duration: Motion.fast } }
    opacity: enabled ? 1 : 0.4
    scale: mouse.pressed ? Motion.pressScale : 1
    Behavior on scale { SpringPress {} }
    activeFocusOnTab: true
    Keys.onSpacePressed: root.clicked()
    Keys.onReturnPressed: root.clicked()

    readonly property color fg: style === "plain" ? Theme.accent
                              : style === "destructive" ? Theme.red
                              : (style === "filled" ? "white" : Theme.label)

    Row {
        id: content
        anchors.centerIn: parent
        spacing: 6
        Icon {
            visible: root.icon !== ""
            name: root.icon
            size: root.compact ? 14 : 16
            color: root.fg
            anchors.verticalCenter: parent.verticalCenter
        }
        FText {
            visible: root.text !== ""
            text: root.text
            color: root.fg
            font.pixelSize: root.compact ? Theme.footnote : Theme.callout
            font.weight: Font.Medium
            anchors.verticalCenter: parent.verticalCenter
        }
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: root.clicked()
    }
}
