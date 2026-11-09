import QtQuick
import QtQuick.Layouts
import Flux

ScrollPage {
    id: page
    title: device ? device.displayName : "Keyboard"
    subtitle: device ? "Hall-effect · firmware " + (device.firmware || "—") : ""

    Rectangle {
        width: parent.width
        height: kb.implicitHeight + 40
        radius: 16
        gradient: Gradient {
            GradientStop { position: 0; color: "#2f3036" }
            GradientStop { position: 1; color: "#1e1e22" }
        }
        border.color: Qt.rgba(1, 1, 1, 0.05)
        KeyboardView {
            id: kb
            anchors.fill: parent
            anchors.margins: 20
            device: page.device
            selectable: false
            readonly property var leds: page.device ? (page.device.fluxEffect ? page.device.liveColors : page.device.keyColors) : null
            ledColors: page.device && (page.device.fluxEffect || page.device.effect === 1) ? leds : null
            fillOpacity: 0.88
            fillFor: page.device && (page.device.fluxEffect || page.device.effect === 1) ? (i) => leds[i] : null
        }
    }

    SectionHeader { text: "At a glance" }
    Card {
        SettingRow {
            icon: "palette"
            iconTint: Theme.orange
            title: "Lighting"
            subtitle: {
                if (!page.device) return ""
                if (page.device.fluxEffect) {
                    const e = page.device.fluxEffects.find((x) => x.id === page.device.fluxEffect)
                    return "Flux · " + (e ? e.name : page.device.fluxEffect)
                }
                const f = page.device.effects.find((x) => x.id === page.device.effect)
                return f ? f.name : "Effect " + page.device.effect
            }
            showChevron: true
            onClicked: win.go("keyboard/lighting")
        }
        SettingRow {
            icon: "arrow-down-to-line"
            iconTint: Theme.accent
            title: "Actuation"
            subtitle: {
                if (!page.device || !page.device.actuation.length) return ""
                const a = page.device.actuation, r = page.device.rapidTrigger
                let mn = 99, mx = 0, rts = 0
                for (const k of page.device.layout) { mn = Math.min(mn, a[k.index]); mx = Math.max(mx, a[k.index]); if (r[k.index].on) rts++ }
                return (mn === mx ? mn.toFixed(2) + " mm" : mn.toFixed(2) + "–" + mx.toFixed(2) + " mm") + " · Rapid Trigger on " + rts + " keys"
            }
            showChevron: true
            onClicked: win.go("keyboard/actuation")
        }
        SettingRow {
            icon: "target"
            iconTint: Theme.pink
            title: "osu! mode"
            subtitle: page.device ? "Hair-trigger + Rapid Trigger on " + page.device.osuKeysText : ""
            Toggle { checked: page.device ? page.device.osuMode : false; onToggled: (v) => page.device.osuMode = v }
        }
        SettingRow {
            icon: "flame"
            iconTint: Theme.red
            title: "Competitive mode"
            Toggle { checked: page.device ? page.device.gameMode : false; onToggled: (v) => page.device.setGameMode(v) }
        }
        SettingRow {
            icon: "lock"
            iconTint: Theme.orange
            title: "Lock the Super key"
            Toggle { checked: page.device ? page.device.winLock : false; onToggled: (v) => page.device.setWinLock(v) }
        }
    }
}
