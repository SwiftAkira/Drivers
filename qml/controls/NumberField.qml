import QtQuick
import Flux

// Compact numeric entry with − / + steppers. Emits committed(value) on
// Enter, focus loss or a step.
Row {
    id: root
    property real value: 0
    property real from: 0
    property real to: 100
    property real stepSize: 1
    property int decimals: 0
    property string suffix: ""
    property int fieldWidth: 76
    signal committed(real value)

    spacing: 4

    function clamp(v) {
        v = Math.round(v / stepSize) * stepSize
        return Math.max(from, Math.min(to, v))
    }
    function commit(v) {
        const c = clamp(v)
        field.text = c.toFixed(decimals)
        root.committed(c)
    }

    FButton {
        compact: true
        icon: "minus"
        width: 26
        onClicked: root.commit(root.value - root.stepSize)
    }
    FTextField {
        id: field
        width: root.fieldWidth
        implicitHeight: 26
        horizontalAlignment: TextInput.AlignHCenter
        text: Number(root.value).toFixed(root.decimals)
        validator: DoubleValidator { bottom: root.from; top: root.to; decimals: root.decimals }
        onEditingFinished: root.commit(parseFloat(text))
    }
    FText {
        visible: root.suffix !== ""
        text: root.suffix
        color: Theme.secondaryLabel
        font.pixelSize: Theme.footnote
        anchors.verticalCenter: parent.verticalCenter
    }
    FButton {
        compact: true
        icon: "plus"
        width: 26
        onClicked: root.commit(root.value + root.stepSize)
    }
}
