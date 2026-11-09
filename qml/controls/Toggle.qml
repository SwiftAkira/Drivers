import QtQuick
import Flux

// iOS-style switch. Does not mutate `checked` itself: bind `checked` to the
// backend and handle `toggled(newValue)`, so bindings never break.
Item {
    id: root
    property bool checked: false
    signal toggled(bool value)

    implicitWidth: 42
    implicitHeight: 25
    opacity: enabled ? 1 : 0.4
    activeFocusOnTab: true

    Keys.onSpacePressed: root.toggled(!root.checked)

    Rectangle {
        id: track
        anchors.fill: parent
        radius: height / 2
        color: root.checked ? Theme.green : Theme.fill
        Behavior on color { ColorAnimation { duration: Motion.normal; easing.type: Motion.easeOut } }
    }

    Rectangle {
        id: knobShadow
        x: knob.x
        y: knob.y + 1
        width: knob.width
        height: knob.height
        radius: knob.radius
        color: Qt.rgba(0, 0, 0, 0.28)
    }

    Rectangle {
        id: knob
        readonly property int pad: 2
        height: root.height - pad * 2
        width: mouse.pressed ? height + 6 : height
        radius: height / 2
        y: pad
        x: root.checked ? root.width - width - pad : pad
        color: "white"
        Behavior on x { SpringStiff {} }
        Behavior on width { SpringStiff {} }
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        anchors.margins: -4
        cursorShape: Qt.PointingHandCursor
        onClicked: root.toggled(!root.checked)
    }
}
