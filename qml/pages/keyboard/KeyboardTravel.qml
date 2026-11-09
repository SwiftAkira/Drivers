import QtQuick
import QtQuick.Layouts
import Flux

ScrollPage {
    id: page
    title: "Live Travel"
    subtitle: "See exactly how far every switch is pressed, in real time."

    Component.onCompleted: if (device) device.liveTravel = true
    Component.onDestruction: if (device && !device.calibrating) device.liveTravel = false

    headerTrailing: Rectangle {
        implicitWidth: fpsText.implicitWidth + 20
        implicitHeight: 24
        radius: 12
        color: Theme.tertiaryFill
        FText {
            id: fpsText
            anchors.centerIn: parent
            text: page.device ? Math.round(page.device.travelFps) + " Hz" : ""
            color: Theme.secondaryLabel
            font.pixelSize: Theme.footnote
            font.weight: Font.DemiBold
        }
    }

    Rectangle {
        width: parent.width
        height: kb.implicitHeight + 36
        radius: 16
        color: "#141416"
        border.color: Qt.rgba(1, 1, 1, 0.05)
        KeyboardView {
            id: kb
            anchors.fill: parent
            anchors.margins: 18
            device: page.device
            selectable: false
            travel: page.device ? page.device.travel : null
            travelMax: page.device ? page.device.travelMax : 3.5
            detailFor: (i) => {
                const t = page.device ? page.device.travel[i] : 0
                return t > 0.04 ? t.toFixed(2) : ""
            }
        }
    }

    Item { width: 1; height: 6 }
    FText {
        width: parent.width
        text: "Green shows the travel. Orange means the key is bottomed out. Readings are polled over USB, so they arrive about 30–60 times a second."
        color: Theme.tertiaryLabel
        font.pixelSize: Theme.footnote
        wrapMode: Text.WordWrap
        elide: Text.ElideNone
    }

    SectionHeader { text: "Calibration" }
    Card {
        SettingRow {
            icon: "target"
            iconTint: Theme.orange
            title: page.device && page.device.calibrating ? "Now press every key all the way down, slowly, once" : "Calibrate switches"
            subtitle: page.device && page.device.calibrating
                      ? "Watch each key fill up to orange above. Press Done when you've pressed them all."
                      : "Do this if a key triggers too early or late, or after swapping switches."
            Row {
                spacing: 8
                FButton {
                    visible: !(page.device && page.device.calibrating)
                    compact: true
                    text: "Start"
                    onClicked: page.device.startCalibration()
                }
                FButton {
                    visible: page.device && page.device.calibrating
                    compact: true
                    style: "filled"
                    text: "Done"
                    onClicked: page.device.finishCalibration()
                }
            }
        }
    }
}
