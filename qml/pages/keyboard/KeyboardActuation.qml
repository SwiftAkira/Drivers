import QtQuick
import QtQuick.Layouts
import Flux

ScrollPage {
    id: page
    title: "Actuation"
    subtitle: "Pick keys on the board, then set where they trigger. Leave everything unselected to edit all keys."

    readonly property var sel: kb.selection
    readonly property var targets: sel.length ? sel : (device ? device.layout.map((k) => k.index) : [])
    readonly property int ref: targets.length ? targets[0] : 0
    readonly property double act: device && device.actuation.length ? device.actuation[ref] : 1.2
    readonly property var rt: device && device.rapidTrigger.length ? device.rapidTrigger[ref] : ({ on: false, press: 0.3, release: 0.3 })
    readonly property double step: device && device.gameMode ? 0.01 : 0.02
    property string overlay: "actuation"

    Component.onCompleted: if (device) device.liveTravel = true
    Component.onDestruction: if (device) device.liveTravel = false

    headerTrailing: Segmented {
        model: ["Actuation", "Rapid Trigger"]
        currentIndex: page.overlay === "actuation" ? 0 : 1
        onActivated: (i) => page.overlay = i === 0 ? "actuation" : "rt"
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
            travel: page.device ? page.device.travel : null
            travelMax: page.device ? page.device.travelMax : 3.5
            detailFor: (i) => {
                if (!page.device) return ""
                if (page.overlay === "actuation") return page.device.actuation[i].toFixed(2)
                const r = page.device.rapidTrigger[i]
                return r.on ? "RT " + r.press.toFixed(2) : ""
            }
        }
    }

    RowLayout {
        width: parent.width
        spacing: 0
        FText {
            Layout.fillWidth: true
            topPadding: 8
            text: page.sel.length ? page.sel.length + " key" + (page.sel.length > 1 ? "s" : "") + " selected" : "Editing all keys"
            color: Theme.secondaryLabel
            font.pixelSize: Theme.footnote
        }
        FButton { compact: true; style: "plain"; text: "Select All"; onClicked: kb.selectAll() }
        FButton { compact: true; style: "plain"; text: "Clear"; onClicked: kb.clearSelection() }
    }

    Item { width: 1; height: 8 }

    RowLayout {
        width: parent.width
        spacing: 14

        Card {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignTop
            width: undefined

            SliderRow {
                title: "Actuation point"
                subtitle: "How far the key travels before it registers."
                from: page.device ? page.device.actuationMin : 0.1
                to: page.device ? page.device.actuationMax : 3.5
                stepSize: page.step
                value: page.act
                format: (v) => v.toFixed(2) + " mm"
                minLabel: "Hair trigger"
                maxLabel: "Deep"
                onCommitted: (v) => page.device.setActuation(page.targets, v)
            }
            SettingRow {
                icon: "zap"
                iconTint: Theme.purple
                title: "Rapid Trigger"
                subtitle: "Resets as soon as the key starts rising and re-triggers as soon as it moves down again, wherever it is."
                Toggle {
                    checked: page.rt.on
                    onToggled: (v) => page.device.setRapidTrigger(page.targets, v, page.rt.press, page.rt.release)
                }
            }
            SliderRow {
                enabled: page.rt.on
                title: "Press sensitivity"
                from: page.device ? page.device.rtMin : 0.02
                to: 2.0
                stepSize: 0.01
                value: page.rt.press
                format: (v) => v.toFixed(2) + " mm"
                onCommitted: (v) => page.device.setRapidTrigger(page.targets, true, v, page.rt.release)
            }
            SliderRow {
                enabled: page.rt.on
                title: "Release sensitivity"
                from: page.device ? page.device.rtMin : 0.02
                to: 2.0
                stepSize: 0.01
                value: page.rt.release
                format: (v) => v.toFixed(2) + " mm"
                onCommitted: (v) => page.device.setRapidTrigger(page.targets, true, page.rt.press, v)
            }
        }

        Rectangle {
            Layout.preferredWidth: 170
            Layout.preferredHeight: 300
            Layout.alignment: Qt.AlignTop
            radius: Theme.radiusCard
            color: Theme.card
            border.color: Qt.rgba(1, 1, 1, 0.045)
            TravelDiagram {
                anchors.fill: parent
                anchors.margins: 10
                travelMax: page.device ? page.device.travelMax : 3.5
                actuation: page.act
                rtOn: page.rt.on
                rtPress: page.rt.press
                rtRelease: page.rt.release
                deadTop: page.device ? page.device.deadZoneTop : 0
                deadBottom: page.device ? page.device.deadZoneBottom : 0
                live: page.device && page.device.travel.length ? page.device.travel[page.ref] : -1
            }
        }
    }

    SectionHeader { text: "Presets" }
    Card {
        SettingRow {
            icon: "target"
            iconTint: Theme.pink
            title: "osu! mode"
            subtitle: "0.15 mm actuation and 0.05 / 0.08 mm Rapid Trigger on " + (page.device ? page.device.osuKeysText : "A D") + ". Switch off to restore. Change keys under Profiles."
            Toggle { checked: page.device ? page.device.osuMode : false; onToggled: (v) => page.device.osuMode = v }
        }
        Repeater {
            model: [
                { name: "Competitive", desc: "0.20 mm actuation, Rapid Trigger 0.05 mm", a: 0.2, rt: true, p: 0.05, r: 0.05, icon: "flame", tint: Theme.red },
                { name: "Balanced", desc: "1.00 mm actuation, Rapid Trigger 0.15 mm", a: 1.0, rt: true, p: 0.15, r: 0.15, icon: "gauge", tint: Theme.accent },
                { name: "Typing", desc: "1.80 mm actuation, no Rapid Trigger", a: 1.8, rt: false, p: 0.3, r: 0.3, icon: "keyboard", tint: Theme.green }
            ]
            delegate: SettingRow {
                required property var modelData
                icon: modelData.icon
                iconTint: modelData.tint
                title: modelData.name
                subtitle: modelData.desc
                FButton {
                    compact: true
                    text: page.sel.length ? "Apply to Selection" : "Apply to All"
                    onClicked: {
                        page.device.setActuation(page.targets, modelData.a)
                        page.device.setRapidTrigger(page.targets, modelData.rt, modelData.p, modelData.r)
                    }
                }
            }
        }
    }

    SectionHeader { text: "Dead zones"; detail: "Whole keyboard" }
    Card {
        SliderRow {
            title: "Top dead zone"
            subtitle: "Ignore the first bit of travel, so resting fingers don't trigger keys."
            from: 0; to: page.device ? page.device.deadZoneMax : 1; stepSize: 0.02
            value: page.device ? page.device.deadZoneTop : 0
            format: (v) => v.toFixed(2) + " mm"
            onCommitted: (v) => page.device.setDeadZones(v, page.device.deadZoneBottom)
        }
        SliderRow {
            title: "Bottom dead zone"
            subtitle: "Ignore the last bit of travel, where the switch bottoms out."
            from: 0; to: page.device ? page.device.deadZoneMax : 1; stepSize: 0.02
            value: page.device ? page.device.deadZoneBottom : 0
            format: (v) => v.toFixed(2) + " mm"
            onCommitted: (v) => page.device.setDeadZones(page.device.deadZoneTop, v)
        }
    }
}
