import QtQuick
import Flux

// Monochrome Lucide glyph tinted to any colour (rendered by IconProvider).
Item {
    id: root
    property string name
    property color color: Theme.label
    property int size: 16

    implicitWidth: size
    implicitHeight: size

    Image {
        anchors.fill: parent
        source: root.name ? "image://icon/" + root.name + "/" + root.color.toString().substring(1) : ""
        sourceSize: Qt.size(root.size * 2, root.size * 2)
        smooth: true
        mipmap: true
        asynchronous: false
    }
}
