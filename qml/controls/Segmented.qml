import QtQuick
import Flux

// macOS/iOS segmented control with a spring-loaded selection pill.
// Bind currentIndex to the backend and react to activated(index).
Rectangle {
    id: root
    property var model: []
    property int currentIndex: 0
    property int minSegmentWidth: 52
    signal activated(int index)

    readonly property real segmentWidth: {
        let w = minSegmentWidth
        for (let i = 0; i < measure.count; ++i) {
            const it = measure.itemAt(i)
            if (it) w = Math.max(w, it.implicitWidth + 26)
        }
        return w
    }

    implicitHeight: 28
    implicitWidth: segmentWidth * model.length + 4
    radius: 8
    color: Theme.tertiaryFill
    opacity: enabled ? 1 : 0.4

    // Invisible text used only for measuring segment widths.
    Repeater {
        id: measure
        model: root.model
        delegate: FText {
            visible: false
            text: modelData
            font.pixelSize: Theme.callout
            font.weight: Font.DemiBold
        }
    }

    Rectangle {
        id: pill
        visible: root.currentIndex >= 0
        y: 2
        height: root.height - 4
        width: root.segmentWidth
        x: 2 + Math.max(0, root.currentIndex) * root.segmentWidth
        radius: 6
        color: "#636366"
        Behavior on x { SpringStiff {} }
        Rectangle {
            anchors.fill: parent
            radius: parent.radius
            color: "transparent"
            border.color: Qt.rgba(1, 1, 1, 0.06)
        }
    }

    Row {
        x: 2
        y: 2
        Repeater {
            model: root.model
            delegate: Item {
                width: root.segmentWidth
                height: root.height - 4
                Rectangle {
                    // hairline divider between unselected neighbours
                    visible: index > 0 && index !== root.currentIndex && index - 1 !== root.currentIndex
                    width: 1
                    height: parent.height * 0.5
                    anchors.verticalCenter: parent.verticalCenter
                    color: Theme.separator
                }
                FText {
                    anchors.centerIn: parent
                    text: modelData
                    font.pixelSize: Theme.callout
                    font.weight: index === root.currentIndex ? Font.DemiBold : Font.Medium
                    color: index === root.currentIndex ? Theme.label : Theme.secondaryLabel
                    scale: segMouse.pressed ? 0.94 : 1
                    Behavior on scale { SpringPress {} }
                    Behavior on color { ColorAnimation { duration: Motion.fast } }
                }
                MouseArea {
                    id: segMouse
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: if (index !== root.currentIndex) root.activated(index)
                }
            }
        }
    }
}
