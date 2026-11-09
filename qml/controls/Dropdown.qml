import QtQuick
import QtQuick.Templates as T
import Flux

// macOS pop-up button: shows the current choice, opens a springy menu.
// `model` is an array of strings or {text, value} objects.
Rectangle {
    id: root
    property var model: []
    property int currentIndex: 0
    property int menuMaxHeight: 320
    signal activated(int index)

    function textAt(i) {
        const m = root.model[i]
        return m === undefined ? "" : (typeof m === "object" ? m.text : String(m))
    }

    implicitHeight: 28
    implicitWidth: Math.max(110, label.implicitWidth + 44)
    radius: 7
    color: mouse.pressed ? Theme.fill : (mouse.containsMouse ? Theme.secondaryFill : Theme.tertiaryFill)
    Behavior on color { ColorAnimation { duration: Motion.fast } }
    opacity: enabled ? 1 : 0.4

    FText {
        id: label
        anchors.left: parent.left
        anchors.leftMargin: 11
        anchors.right: chev.left
        anchors.rightMargin: 6
        anchors.verticalCenter: parent.verticalCenter
        text: root.textAt(root.currentIndex)
        font.pixelSize: Theme.callout
        font.weight: Font.Medium
    }
    Icon {
        id: chev
        anchors.right: parent.right
        anchors.rightMargin: 8
        anchors.verticalCenter: parent.verticalCenter
        name: "chevrons-up-down"
        size: 13
        color: Theme.secondaryLabel
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: menu.open()
    }

    T.Popup {
        id: menu
        y: root.height + 4
        x: 0
        width: Math.max(root.width, list.contentItemMaxWidth + 44)
        height: Math.min(list.contentHeight + 12, root.menuMaxHeight)
        padding: 6
        transformOrigin: T.Popup.Top
        focus: true

        enter: Transition {
            ParallelAnimation {
                NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Motion.fast }
                SpringAnimation { property: "scale"; from: 0.9; to: 1; spring: Motion.springStiff; damping: Motion.springDampStiff; epsilon: Motion.epsilon }
            }
        }
        exit: Transition {
            NumberAnimation { property: "opacity"; to: 0; duration: Motion.fast }
        }

        background: Rectangle {
            radius: 10
            color: Theme.popover
            border.color: Theme.hairline
        }

        contentItem: ListView {
            id: list
            property real contentItemMaxWidth: 0
            clip: true
            model: root.model
            currentIndex: root.currentIndex
            boundsBehavior: Flickable.StopAtBounds
            T.ScrollBar.vertical: ThinScrollBar {}
            delegate: Rectangle {
                width: list.width
                height: 28
                radius: 6
                color: hov.hovered ? Theme.accent : "transparent"
                Icon {
                    anchors.left: parent.left
                    anchors.leftMargin: 8
                    anchors.verticalCenter: parent.verticalCenter
                    visible: index === root.currentIndex
                    name: "check"
                    size: 13
                    color: hov.hovered ? "white" : Theme.label
                }
                FText {
                    id: itemText
                    anchors.left: parent.left
                    anchors.leftMargin: 28
                    anchors.verticalCenter: parent.verticalCenter
                    text: root.textAt(index)
                    font.pixelSize: Theme.callout
                    color: hov.hovered ? "white" : Theme.label
                    Component.onCompleted: list.contentItemMaxWidth = Math.max(list.contentItemMaxWidth, implicitWidth)
                }
                HoverHandler { id: hov }
                TapHandler {
                    onTapped: {
                        menu.close()
                        if (index !== root.currentIndex)
                            root.activated(index)
                    }
                }
            }
        }
    }
}
