import QtQuick
import QtQuick.Layouts
import Flux

Item {
    id: root
    property string text
    property string detail
    default property alias trailing: trailingRow.data

    width: parent ? parent.width : implicitWidth
    implicitHeight: 40

    RowLayout {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 7
        anchors.leftMargin: 6
        anchors.rightMargin: 4
        FText {
            text: root.text
            font.pixelSize: Theme.callout
            font.weight: Font.DemiBold
            color: Theme.label
        }
        FText {
            visible: root.detail !== ""
            text: root.detail
            font.pixelSize: Theme.footnote
            color: Theme.secondaryLabel
        }
        Item { Layout.fillWidth: true }
        Row { id: trailingRow; spacing: 6 }
    }
}
