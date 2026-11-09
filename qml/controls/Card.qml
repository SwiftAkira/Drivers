import QtQuick
import Flux

// Grouped "inset list" container, like a section in System Settings.
Rectangle {
    id: root
    default property alias content: column.data
    property alias spacing: column.spacing

    width: parent ? parent.width : implicitWidth
    implicitHeight: column.implicitHeight
    radius: Theme.radiusCard
    color: Theme.card
    border.color: Qt.rgba(1, 1, 1, 0.045)
    border.width: 1

    Column {
        id: column
        width: parent.width
    }
}
