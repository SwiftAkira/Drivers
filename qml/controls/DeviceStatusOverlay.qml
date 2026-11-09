import QtQuick
import Flux

// Covers a device page while the device is connecting or (for wireless
// mice) asleep behind the receiver.
Item {
    id: root
    property var device: null
    readonly property bool connecting: device !== null && !device.ready
    readonly property bool asleep: device !== null && device.ready && device.kind === "mouse" && !device.online
    visible: opacity > 0
    opacity: connecting || asleep ? 1 : 0
    Behavior on opacity { NumberAnimation { duration: Motion.normal } }

    Rectangle {
        anchors.fill: parent
        color: Theme.alpha(Theme.window, 0.86)
    }
    MouseArea { anchors.fill: parent; hoverEnabled: true }

    Column {
        anchors.centerIn: parent
        spacing: 14
        scale: root.opacity > 0.5 ? 1 : 0.94
        Behavior on scale { SpringSoft {} }

        Spinner {
            visible: root.connecting
            size: 28
            anchors.horizontalCenter: parent.horizontalCenter
        }
        Rectangle {
            visible: root.asleep
            width: 64; height: 64; radius: 32
            color: Theme.tertiaryFill
            anchors.horizontalCenter: parent.horizontalCenter
            Icon { anchors.centerIn: parent; name: "moon"; size: 28; color: Theme.secondaryLabel }
        }
        FText {
            anchors.horizontalCenter: parent.horizontalCenter
            text: root.connecting ? "Connecting to " + (root.device ? root.device.displayName || "device" : "device") + "…"
                                  : "Your mouse is asleep"
            font.pixelSize: Theme.title3
            font.weight: Font.DemiBold
        }
        FText {
            anchors.horizontalCenter: parent.horizontalCenter
            visible: root.asleep
            text: "Move or click it to wake it up. Settings will load automatically."
            color: Theme.secondaryLabel
            font.pixelSize: Theme.callout
        }
    }
}
