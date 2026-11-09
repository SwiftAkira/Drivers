import QtQuick
import QtQuick.Layouts
import Flux

// One row inside a Card: [badge] title/subtitle ............ [trailing control]
Item {
    id: root
    property string title
    property string subtitle
    property string icon
    property color iconTint: Theme.accent
    property bool showChevron: false
    property bool clickable: showChevron
    default property alias trailing: trailingBox.data
    signal clicked()

    width: parent ? parent.width : implicitWidth
    implicitHeight: Math.max(Theme.rowHeight, textCol.implicitHeight + 22)

    Rectangle {
        visible: !root.Positioner.isFirstItem
        height: 1
        color: Theme.separator
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: root.icon ? 50 : 16
    }

    Rectangle {
        anchors.fill: parent
        anchors.margins: 1
        radius: Theme.radiusCard - 1
        color: Theme.quaternaryFill
        opacity: root.clickable && rowMouse.containsMouse ? (rowMouse.pressed ? 1 : 0.6) : 0
        Behavior on opacity { NumberAnimation { duration: Motion.fast } }
    }

    MouseArea {
        id: rowMouse
        anchors.fill: parent
        enabled: root.clickable
        hoverEnabled: root.clickable
        cursorShape: root.clickable ? Qt.PointingHandCursor : Qt.ArrowCursor
        onClicked: root.clicked()
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 14
        anchors.rightMargin: 14
        spacing: 12

        IconBadge {
            visible: root.icon !== ""
            icon: root.icon
            tint: root.iconTint
            size: 24
        }

        ColumnLayout {
            id: textCol
            Layout.fillWidth: true
            spacing: 2
            FText {
                Layout.fillWidth: true
                text: root.title
                font.pixelSize: Theme.body
            }
            FText {
                Layout.fillWidth: true
                visible: root.subtitle !== ""
                text: root.subtitle
                color: Theme.secondaryLabel
                font.pixelSize: Theme.footnote
                wrapMode: Text.WordWrap
                elide: Text.ElideNone
            }
        }

        Item {
            id: trailingBox
            implicitWidth: childrenRect.width
            implicitHeight: childrenRect.height
            Layout.alignment: Qt.AlignVCenter | Qt.AlignRight
        }

        Icon {
            visible: root.showChevron
            name: "chevron-right"
            size: 14
            color: Theme.tertiaryLabel
        }
    }
}
