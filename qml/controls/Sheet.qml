import QtQuick
import QtQuick.Templates as T
import Flux

// Modal sheet: dims the window and springs a card in from slightly smaller.
T.Popup {
    id: root
    property string title
    default property alias content: body.data
    property alias footer: footerRow.data
    property int sheetWidth: 460

    parent: T.Overlay.overlay
    modal: true
    dim: true
    focus: true
    anchors.centerIn: parent
    width: Math.min(sheetWidth, parent ? parent.width - 48 : sheetWidth)
    padding: 22
    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset, implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.min(parent ? parent.height - 40 : 9999,
                             Math.max(implicitBackgroundHeight + topInset + bottomInset, implicitContentHeight + topPadding + bottomPadding))
    closePolicy: T.Popup.CloseOnEscape | T.Popup.CloseOnPressOutside

    T.Overlay.modal: Rectangle {
        color: Qt.rgba(0, 0, 0, 0.45)
        Behavior on opacity { NumberAnimation { duration: Motion.normal } }
    }

    enter: Transition {
        ParallelAnimation {
            NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Motion.normal; easing.type: Motion.easeOut }
            SpringAnimation { property: "scale"; from: 0.92; to: 1; spring: Motion.springSoft; damping: Motion.springDampSoft; epsilon: Motion.epsilon }
        }
    }
    exit: Transition {
        ParallelAnimation {
            NumberAnimation { property: "opacity"; to: 0; duration: Motion.fast }
            NumberAnimation { property: "scale"; to: 0.96; duration: Motion.fast }
        }
    }

    background: Rectangle {
        radius: 14
        color: Theme.popover
        border.color: Theme.hairline
    }

    contentItem: Column {
        spacing: 16
        FText {
            visible: root.title !== ""
            text: root.title
            font.pixelSize: Theme.title3
            font.weight: Font.DemiBold
        }
        Column {
            id: body
            width: parent.width
            spacing: 12
        }
        Row {
            id: footerRow
            spacing: 8
            anchors.right: parent.right
        }
    }
}
