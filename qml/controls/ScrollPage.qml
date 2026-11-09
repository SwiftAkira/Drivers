import QtQuick
import QtQuick.Layouts
import QtQuick.Templates as T
import Flux

// A settings page: large title that hands off to a compact toolbar title
// as you scroll (macOS behaviour), smooth wheel scrolling and a springy
// entrance.
Item {
    id: root
    property string title
    property string subtitle
    property var device: null
    default property alias content: contentCol.data
    property alias headerTrailing: headerTrailingRow.data
    property alias flickable: flick
    property int maxWidth: Theme.contentMaxWidth

    Flickable {
        id: flick
        anchors.fill: parent
        contentWidth: width
        contentHeight: inner.implicitHeight + 48
        boundsBehavior: Flickable.StopAtBounds
        flickDeceleration: 4000
        maximumFlickVelocity: 4500
        clip: true

        T.ScrollBar.vertical: ThinScrollBar {}

        Column {
            id: inner
            width: Math.min(flick.width - Theme.pagePadding * 2, root.maxWidth)
            x: (flick.width - width) / 2
            topPadding: 58
            spacing: 0
            opacity: 0
            transform: Translate { id: enterShift; y: 16 }

            RowLayout {
                width: parent.width
                spacing: 12
                ColumnLayout {
                    spacing: 3
                    Layout.fillWidth: true
                    FText {
                        text: root.title
                        font.pixelSize: Theme.largeTitle
                        font.weight: Font.Bold
                        font.letterSpacing: -0.5
                        Layout.fillWidth: true
                    }
                    FText {
                        visible: root.subtitle !== ""
                        text: root.subtitle
                        color: Theme.secondaryLabel
                        font.pixelSize: Theme.callout
                        wrapMode: Text.WordWrap
                        elide: Text.ElideNone
                        Layout.fillWidth: true
                    }
                }
                Row {
                    id: headerTrailingRow
                    spacing: 8
                    Layout.alignment: Qt.AlignBottom
                }
            }

            Item { width: 1; height: 10 }

            Column {
                id: contentCol
                width: parent.width
                spacing: 0
            }
        }

    }

    // Entrance: fade + spring up a few pixels.
    NumberAnimation {
        target: inner; property: "opacity"; from: 0; to: 1
        duration: Motion.slow; easing.type: Motion.easeOut
        running: true
    }
    SpringAnimation {
        target: enterShift; property: "y"; from: 16; to: 0
        spring: Motion.springSoft; damping: Motion.springDampSoft; epsilon: Motion.epsilon
        running: true
    }

    // Smooth mouse-wheel scrolling (touchpads keep native pixel scrolling).
    WheelHandler {
        target: null
        acceptedDevices: PointerDevice.Mouse
        onWheel: (ev) => {
            const maxY = Math.max(0, flick.contentHeight - flick.height)
            const base = wheelAnim.running ? wheelAnim.to : flick.contentY
            wheelAnim.stop()
            wheelAnim.from = flick.contentY
            wheelAnim.to = Math.max(0, Math.min(maxY, base - ev.angleDelta.y * 1.1))
            wheelAnim.start()
        }
    }
    NumberAnimation {
        id: wheelAnim
        target: flick
        property: "contentY"
        duration: 280
        easing.type: Easing.OutCubic
    }

    // Compact toolbar title that fades in once the large title scrolls away.
    Rectangle {
        id: toolbar
        anchors.left: parent.left
        anchors.right: parent.right
        height: 52
        color: Theme.alpha(Theme.window, 0.92)
        opacity: flick.contentY > 40 ? 1 : 0
        Behavior on opacity { NumberAnimation { duration: Motion.normal } }
        Rectangle {
            anchors.bottom: parent.bottom
            width: parent.width
            height: 1
            color: Theme.separator
        }
        FText {
            anchors.centerIn: parent
            text: root.title
            font.pixelSize: Theme.headline
            font.weight: Font.DemiBold
        }
    }
}
