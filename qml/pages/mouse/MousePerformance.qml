import QtQuick
import QtQuick.Layouts
import Flux

ScrollPage {
    id: page
    title: "Performance"
    subtitle: "DPI stages cycle with the button behind the wheel. The highlighted stage is the one in use."

    property int editing: device ? device.activeStage - 1 : 0
    readonly property var stage: device && device.stages.length > editing ? device.stages[editing] : ({ x: 800, y: 800, color: "#ffffff" })

    function fmtHz(hz) { return hz >= 1000 ? (hz / 1000) + "K" : String(hz) }

    SectionHeader {
        text: "DPI stages"
        detail: device ? device.stages.length + " of " + device.maxStages : ""
        FButton {
            compact: true
            icon: "minus"
            enabled: page.device && page.device.stages.length > 1
            onClicked: {
                page.device.setStageCount(page.device.stages.length - 1)
                page.editing = Math.min(page.editing, page.device.stages.length - 1)
            }
        }
        FButton {
            compact: true
            icon: "plus"
            enabled: page.device && page.device.stages.length < page.device.maxStages
            onClicked: page.device.setStageCount(page.device.stages.length + 1)
        }
    }

    // Stage chips
    Item {
        width: parent.width
        height: 92

        Rectangle {
            id: editRing
            readonly property Item target: chips.itemAt(page.editing)
            x: target ? target.x - 3 : 0
            y: -3
            width: target ? target.width + 6 : 0
            height: 92 + 6 - 6
            radius: 15
            color: "transparent"
            border.width: 2
            border.color: Theme.accent
            visible: target !== null
            Behavior on x { SpringStiff {} }
            Behavior on width { SpringStiff {} }
        }

        Row {
            id: chipRow
            spacing: 10
            width: parent.width
            Repeater {
                id: chips
                model: page.device ? page.device.stages : []
                delegate: Rectangle {
                    id: chip
                    required property var modelData
                    required property int index
                    readonly property bool active: page.device && page.device.activeStage - 1 === index
                    width: (chipRow.width - chipRow.spacing * (chips.count - 1)) / Math.max(1, chips.count)
                    height: 86
                    radius: 12
                    color: active ? Theme.alpha(modelData.color, 0.18) : Theme.card
                    border.color: active ? Theme.alpha(modelData.color, 0.6) : Qt.rgba(1, 1, 1, 0.05)
                    scale: chipTap.pressed ? 0.96 : 1
                    Behavior on scale { SpringPress {} }
                    Behavior on color { ColorAnimation { duration: Motion.normal } }

                    Column {
                        anchors.centerIn: parent
                        spacing: 4
                        Row {
                            anchors.horizontalCenter: parent.horizontalCenter
                            spacing: 6
                            Rectangle { width: 9; height: 9; radius: 4.5; color: chip.modelData.color; anchors.verticalCenter: parent.verticalCenter }
                            FText { text: "Stage " + (chip.index + 1); color: Theme.secondaryLabel; font.pixelSize: Theme.footnote }
                        }
                        FText {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: page.device && page.device.dpiXY && chip.modelData.x !== chip.modelData.y
                                  ? chip.modelData.x + "×" + chip.modelData.y : chip.modelData.x
                            font.pixelSize: Theme.title
                            font.weight: Font.Bold
                        }
                        FText {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: chip.active ? "In use" : " "
                            color: Qt.lighter(chip.modelData.color, 1.6)
                            font.pixelSize: Theme.caption
                            font.weight: Font.DemiBold
                        }
                    }
                    TapHandler {
                        id: chipTap
                        onTapped: page.editing = chip.index
                        onDoubleTapped: page.device.setActiveStage(chip.index + 1)
                    }
                }
            }
        }
    }

    Item { width: 1; height: 12 }

    Card {
        SliderRow {
            title: page.device && page.device.dpiXY ? "Stage " + (page.editing + 1) + " · horizontal (X)" : "Stage " + (page.editing + 1) + " sensitivity"
            from: page.device ? page.device.dpiMin : 50
            to: page.device ? page.device.dpiMax : 30000
            stepSize: page.device ? page.device.dpiStep : 50
            value: page.stage.x
            format: (v) => Math.round(v) + " DPI"
            minLabel: from
            maxLabel: to
            onCommitted: (v) => page.device.setStage(page.editing, Math.round(v), page.device.dpiXY ? page.stage.y : Math.round(v))
        }
        SliderRow {
            visible: page.device && page.device.dpiXY
            title: "Stage " + (page.editing + 1) + " · vertical (Y)"
            from: page.device ? page.device.dpiMin : 50
            to: page.device ? page.device.dpiMax : 30000
            stepSize: page.device ? page.device.dpiStep : 50
            value: page.stage.y
            format: (v) => Math.round(v) + " DPI"
            onCommitted: (v) => page.device.setStage(page.editing, page.stage.x, Math.round(v))
        }
        SettingRow {
            title: "Exact value"
            NumberField {
                value: page.stage.x
                from: page.device ? page.device.dpiMin : 50
                to: page.device ? page.device.dpiMax : 30000
                stepSize: page.device ? page.device.dpiStep : 50
                suffix: "DPI"
                onCommitted: (v) => page.device.setStage(page.editing, v, page.device.dpiXY ? page.stage.y : v)
            }
        }
        SettingRow {
            title: "Indicator colour"
            subtitle: "Shown on the mouse LED when you switch to this stage."
            ColorWell {
                color: page.stage.color
                onPicked: (c) => page.device.setStageColor(page.editing, c)
            }
        }
        SettingRow {
            title: "Use this stage now"
            FButton {
                compact: true
                style: page.device && page.device.activeStage - 1 === page.editing ? "gray" : "filled"
                text: page.device && page.device.activeStage - 1 === page.editing ? "In use" : "Activate"
                enabled: page.device && page.device.activeStage - 1 !== page.editing
                onClicked: page.device.setActiveStage(page.editing + 1)
            }
        }
    }

    Item { width: 1; height: 8 }
    Card {
        SettingRow {
            icon: "move"
            iconTint: Theme.purple
            title: "Separate X and Y sensitivity"
            subtitle: "Give horizontal and vertical movement different DPI."
            Toggle {
                checked: page.device ? page.device.dpiXY : false
                onToggled: (v) => page.device.setDpiXY(v)
            }
        }
    }

    SectionHeader { text: "Polling rate" }
    Card {
        SettingRow {
            icon: "radio"
            iconTint: Theme.accent
            title: "Report rate"
            subtitle: page.device && page.device.pollingRate >= 4000
                      ? "Very high rates use more CPU and battery. Some games stutter above 4K."
                      : "How often the mouse reports to the PC."
            Segmented {
                model: page.device ? page.device.pollingRates.map(page.fmtHz) : []
                currentIndex: page.device ? page.device.pollingRates.indexOf(page.device.pollingRate) : 0
                onActivated: (i) => page.device.setPollingRate(page.device.pollingRates[i])
            }
        }
    }
}
