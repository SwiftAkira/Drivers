import QtQuick
import QtQuick.Layouts
import QtQuick.Templates as T
import Flux

// Title + live value on top, full-width slider below.
Item {
    id: root
    property string title
    property string subtitle
    property real from: 0
    property real to: 100
    property real stepSize: 1
    property real value: 0
    property int tickCount: 0
    property color tint: Theme.accent
    property var format: (v) => Math.round(v).toString()
    property alias minLabel: minText.text
    property alias maxLabel: maxText.text
    signal committed(real value)
    signal moved(real value)

    width: parent ? parent.width : implicitWidth
    implicitHeight: col.implicitHeight + 24

    Rectangle {
        visible: !root.Positioner.isFirstItem
        height: 1
        color: Theme.separator
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: 16
    }

    ColumnLayout {
        id: col
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        anchors.leftMargin: 16
        anchors.rightMargin: 16
        spacing: 6

        RowLayout {
            Layout.fillWidth: true
            ColumnLayout {
                spacing: 2
                Layout.fillWidth: true
                FText { text: root.title; Layout.fillWidth: true }
                FText {
                    visible: root.subtitle !== ""
                    text: root.subtitle
                    color: Theme.secondaryLabel
                    font.pixelSize: Theme.footnote
                    wrapMode: Text.WordWrap
                    elide: Text.ElideNone
                    Layout.fillWidth: true
                }
            }
            Rectangle {
                implicitWidth: valueText.implicitWidth + 16
                implicitHeight: 22
                radius: 6
                color: Theme.tertiaryFill
                FText {
                    id: valueText
                    anchors.centerIn: parent
                    text: root.format(slider.value)
                    font.pixelSize: Theme.callout
                    font.weight: Font.DemiBold
                }
            }
        }

        FSlider {
            id: slider
            Layout.fillWidth: true
            Layout.leftMargin: -10
            Layout.rightMargin: -10
            from: root.from
            to: root.to
            stepSize: root.stepSize
            snapMode: T.Slider.SnapAlways
            value: root.value
            tickCount: root.tickCount
            tint: root.tint
            onMoved: root.moved(value)
            onCommitted: (v) => root.committed(v)
        }

        RowLayout {
            visible: minText.text !== "" || maxText.text !== ""
            Layout.fillWidth: true
            FText { id: minText; color: Theme.tertiaryLabel; font.pixelSize: Theme.caption }
            Item { Layout.fillWidth: true }
            FText { id: maxText; color: Theme.tertiaryLabel; font.pixelSize: Theme.caption }
        }
    }
}
