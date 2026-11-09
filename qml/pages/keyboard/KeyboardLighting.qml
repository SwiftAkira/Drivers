import QtQuick
import QtQuick.Layouts
import Flux

ScrollPage {
    id: page
    title: "Lighting"
    subtitle: device && device.fluxEffect ? "Flux is driving the LEDs · " + Math.round(device.fluxFps) + " fps" : "Choose live Flux effects, the keyboard's built-in modes, or paint your own."

    property int tab: device && device.fluxEffect ? 0 : 0
    readonly property var paintSel: preview.selection
    property color paintColor: "#0a84ff"

    readonly property var fwEffects: device ? device.effects : []
    function fwIndex() {
        if (!device) return 0
        for (let i = 0; i < fwEffects.length; ++i) if (fwEffects[i].id === device.effect) return i
        return 0
    }

    headerTrailing: Segmented {
        model: ["Flux Effects", "Built-in", "Paint", "Side Light"]
        currentIndex: page.tab
        onActivated: (i) => page.tab = i
    }

    // Live preview (always visible)
    Rectangle {
        width: parent.width
        height: preview.implicitHeight + 36
        radius: 16
        color: "#141416"
        border.color: Qt.rgba(1, 1, 1, 0.05)
        KeyboardView {
            id: preview
            anchors.fill: parent
            anchors.margins: 18
            device: page.device
            selectable: page.tab === 2
            readonly property var leds: page.device ? (page.device.fluxEffect ? page.device.liveColors : page.device.keyColors) : null
            labelFor: page.tab === 2 ? null : (i) => ""
            ledColors: leds
            fillOpacity: 0.88
            fillFor: (i) => leds ? leds[i] : undefined
        }
    }

    Item { width: 1; height: 6 }

    // ── Flux effects ──────────────────────────────────────────────────────
    Column {
        visible: page.tab === 0
        width: parent.width
        spacing: 0

        SectionHeader {
            text: "Reacts to your keys"
            detail: "Uses the analog switches, so pressing harder makes bigger effects"
        }
        EffectGrid {
            width: parent.width
            effects: page.device ? page.device.fluxEffects.filter((e) => e.reactive) : []
            current: page.device ? page.device.fluxEffect : ""
            onPicked: (id) => page.device.startFluxEffect(id)
        }
        SectionHeader {
            text: "Reacts to sound"
            detail: "Listens to whatever your PC is playing"
        }
        EffectGrid {
            width: parent.width
            effects: page.device ? page.device.fluxEffects.filter((e) => e.audio) : []
            current: page.device ? page.device.fluxEffect : ""
            onPicked: (id) => page.device.startFluxEffect(id)
        }
        SectionHeader { text: "Ambient" }
        EffectGrid {
            width: parent.width
            effects: page.device ? page.device.fluxEffects.filter((e) => !e.reactive && !e.audio) : []
            current: page.device ? page.device.fluxEffect : ""
            onPicked: (id) => page.device.startFluxEffect(id)
        }

        SectionHeader { text: "Tune"; visible: page.device && page.device.fluxEffect !== "" }
        Card {
            visible: page.device && page.device.fluxEffect !== ""
            SettingRow {
                icon: "palette"
                iconTint: Theme.pink
                title: "Palette"
                subtitle: "Three colours the effect blends between."
                Row {
                    spacing: 10
                    Repeater {
                        model: page.device ? page.device.fluxPalette : []
                        ColorWell {
                            required property var modelData
                            required property int index
                            color: modelData
                            size: 28
                            onPicked: (c) => page.device.setFluxPaletteColor(index, c)
                        }
                    }
                    FButton { compact: true; style: "plain"; icon: "rotate-ccw"; onClicked: page.device.resetFluxPalette() }
                }
            }
            SliderRow {
                title: "Speed"
                from: 0.2; to: 3; stepSize: 0.05
                value: page.device ? page.device.fluxSpeed : 1
                format: (v) => v.toFixed(2) + "×"
                onMoved: (v) => page.device.setFluxSpeed(v)
                onCommitted: (v) => page.device.setFluxSpeed(v)
            }
            SliderRow {
                title: "Brightness"
                subtitle: "Capped at the level the vendor rates as stable over USB."
                from: 0.05; to: 1; stepSize: 0.01
                value: page.device ? page.device.fluxIntensity : 0.85
                format: (v) => Math.round(v * 100) + "%"
                onMoved: (v) => page.device.setFluxIntensity(v)
                onCommitted: (v) => page.device.setFluxIntensity(v)
            }
            SettingRow {
                icon: "save"
                iconTint: Theme.green
                title: "Bake this frame to the keyboard"
                subtitle: "Saves what you see right now as a static design, so it stays when Flux isn't running."
                FButton { compact: true; text: "Bake"; onClicked: page.device.bakeFluxFrame() }
            }
            SettingRow {
                title: "Stop Flux effect"
                subtitle: "Hands the LEDs back to the keyboard's own lighting."
                FButton { compact: true; style: "destructive"; text: "Stop"; onClicked: page.device.stopFluxEffect() }
            }
        }
        FText {
            width: parent.width
            topPadding: 12
            text: "Live effects run while Flux is open or in the tray. Turn on “Open at login” in Flux Settings to have them every time your PC starts."
            color: Theme.tertiaryLabel
            font.pixelSize: Theme.footnote
            wrapMode: Text.WordWrap
            elide: Text.ElideNone
        }
    }

    // ── Built-in ─────────────────────────────────────────────────────────
    Column {
        id: builtIn
        visible: page.tab === 1
        width: parent.width
        readonly property var cur: page.fwEffects[page.fwIndex()] || ({ usesColor: true, usesSpeed: true })
        SectionHeader { text: "Keyboard effect" }
        Card {
            SettingRow {
                icon: "sun"
                iconTint: Theme.orange
                title: "Effect"
                subtitle: page.device && page.device.fluxEffect ? "Choosing one stops the Flux effect." : "Runs on the keyboard itself, even without Flux."
                Dropdown {
                    model: page.fwEffects.map((e) => e.name)
                    currentIndex: page.fwIndex()
                    menuMaxHeight: 420
                    onActivated: (i) => page.device.setLighting(page.fwEffects[i].id, page.device.speed, Math.max(1, page.device.brightness), page.device.lightColor)
                }
            }
            SliderRow {
                title: "Brightness"
                subtitle: "The vendor warns above 60 % the USB link can become unstable."
                from: 1; to: page.device ? page.device.brightnessMax : 200; stepSize: 1
                value: page.device ? page.device.brightness : 100
                tint: value > 120 ? Theme.orange : Theme.accent
                format: (v) => Math.round(v / (page.device ? page.device.brightnessMax : 200) * 100) + "%"
                onCommitted: (v) => page.device.setLighting(page.device.effect, page.device.speed, Math.round(v), page.device.lightColor)
            }
            SliderRow {
                visible: builtIn.cur.usesSpeed
                title: "Speed"
                from: 1; to: page.device ? page.device.speedMax : 255; stepSize: 1
                value: page.device ? page.device.speed : 128
                format: (v) => Math.round(v)
                onCommitted: (v) => page.device.setLighting(page.device.effect, Math.round(v), page.device.brightness, page.device.lightColor)
            }
            SettingRow {
                visible: builtIn.cur.usesColor
                title: "Colour"
                ColorWell {
                    color: page.device ? page.device.lightColor : "white"
                    onPicked: (c) => page.device.setLighting(page.device.effect, page.device.speed, page.device.brightness, c)
                }
            }
        }
    }

    // ── Paint ────────────────────────────────────────────────────────────
    Column {
        visible: page.tab === 2
        width: parent.width
        SectionHeader {
            text: "Paint keys"
            detail: page.paintSel.length ? page.paintSel.length + " selected" : "Click or drag across keys above"
            FButton { compact: true; style: "plain"; text: "All"; onClicked: preview.selectAll() }
            FButton { compact: true; style: "plain"; text: "None"; onClicked: preview.clearSelection() }
        }
        Card {
            SettingRow {
                icon: "pipette"
                iconTint: Theme.pink
                title: "Colour"
                ColorWell { color: page.paintColor; onPicked: (c) => page.paintColor = c }
            }
            SettingRow {
                title: "Apply to selected keys"
                subtitle: "Switches the keyboard to per-key mode and saves the design to it."
                FButton {
                    compact: true
                    style: "filled"
                    text: "Paint"
                    enabled: page.paintSel.length > 0
                    onClicked: {
                        page.device.setKeyColor(page.paintSel, page.paintColor)
                        if (page.device.effect !== 1)
                            page.device.setLighting(1, page.device.speed, Math.max(1, page.device.brightness), page.device.lightColor)
                    }
                }
            }
        }
        FText {
            width: parent.width
            topPadding: 10
            text: "Tip: start any Flux effect, wait for a frame you like, then use “Bake” to turn it into a painted design."
            color: Theme.tertiaryLabel
            font.pixelSize: Theme.footnote
            wrapMode: Text.WordWrap
            elide: Text.ElideNone
        }
    }

    // ── Side light ───────────────────────────────────────────────────────
    Column {
        visible: page.tab === 3
        width: parent.width
        SectionHeader { text: "Side light" }
        Card {
            SettingRow {
                icon: "lightbulb"
                iconTint: Theme.yellow
                title: "Mode"
                Segmented {
                    model: ["Off", "Wave", "Spectrum", "Steady", "Breath"]
                    currentIndex: page.device ? page.device.boxMode : 0
                    onActivated: (i) => page.device.setBoxLight(i, page.device.boxRainbow, Math.max(page.device.boxBrightness, i ? 80 : 0), page.device.boxSpeed, page.device.boxColor)
                }
            }
            SliderRow {
                enabled: page.device && page.device.boxMode > 0
                title: "Brightness"
                from: 0; to: 160; stepSize: 1
                value: page.device ? page.device.boxBrightness : 0
                format: (v) => Math.round(v / 1.6) + "%"
                onCommitted: (v) => page.device.setBoxLight(page.device.boxMode, page.device.boxRainbow, Math.round(v), page.device.boxSpeed, page.device.boxColor)
            }
            SliderRow {
                enabled: page.device && [1, 2, 4].indexOf(page.device.boxMode) >= 0
                title: "Speed"
                from: 0; to: 40; stepSize: 1
                value: page.device ? page.device.boxSpeed : 0
                onCommitted: (v) => page.device.setBoxLight(page.device.boxMode, page.device.boxRainbow, page.device.boxBrightness, Math.round(v), page.device.boxColor)
            }
            SettingRow {
                enabled: page.device && [1, 3, 4].indexOf(page.device.boxMode) >= 0
                title: "Rainbow"
                Toggle {
                    checked: page.device ? page.device.boxRainbow : false
                    onToggled: (v) => page.device.setBoxLight(page.device.boxMode, v, page.device.boxBrightness, page.device.boxSpeed, page.device.boxColor)
                }
            }
            SettingRow {
                enabled: page.device && [1, 3, 4].indexOf(page.device.boxMode) >= 0 && !page.device.boxRainbow
                title: "Colour"
                ColorWell {
                    color: page.device ? page.device.boxColor : "white"
                    onPicked: (c) => page.device.setBoxLight(page.device.boxMode, false, page.device.boxBrightness, page.device.boxSpeed, c)
                }
            }
        }
    }
}
