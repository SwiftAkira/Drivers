import QtQuick
import Flux

// Small HUD pill that springs up from the bottom edge.
Item {
    id: root
    anchors.fill: parent
    z: 1000

    function show(message, isError) {
        label.text = message
        icon.name = isError ? "circle-alert" : "check"
        icon.color = isError ? Theme.red : Theme.green
        hideTimer.restart()
        pill.shown = true
    }

    Rectangle {
        id: pill
        property bool shown: false
        anchors.horizontalCenter: parent.horizontalCenter
        y: shown ? parent.height - height - 22 : parent.height + 10
        opacity: shown ? 1 : 0
        width: row.implicitWidth + 28
        height: 36
        radius: 18
        color: Qt.rgba(0.23, 0.23, 0.24, 0.96)
        border.color: Theme.hairline
        Behavior on y { SpringSoft {} }
        Behavior on opacity { NumberAnimation { duration: Motion.normal } }

        Row {
            id: row
            anchors.centerIn: parent
            spacing: 8
            Icon { id: icon; size: 15; anchors.verticalCenter: parent.verticalCenter }
            FText {
                id: label
                font.pixelSize: Theme.callout
                font.weight: Font.Medium
                anchors.verticalCenter: parent.verticalCenter
                width: Math.min(implicitWidth, root.width - 120)
            }
        }
    }

    Timer {
        id: hideTimer
        interval: 2200
        onTriggered: pill.shown = false
    }
}
