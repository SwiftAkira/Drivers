import QtQuick
import Flux

ScrollPage {
    id: page
    title: "Sensor"
    subtitle: device ? device.sensorName : ""

    SectionHeader { text: "Tracking" }
    Card {
        SettingRow {
            icon: "ruler"
            iconTint: Theme.teal
            title: "Lift-off distance"
            subtitle: "How high you can lift the mouse before it stops tracking. Lower suits low-sens players who reposition often."
            Segmented {
                model: page.device ? page.device.lodOptions.map((v) => v + " mm") : []
                currentIndex: {
                    if (!page.device) return 0
                    const opts = page.device.lodOptions
                    for (let i = 0; i < opts.length; ++i)
                        if (Math.abs(opts[i] - page.device.lod) < 0.05) return i
                    return -1
                }
                onActivated: (i) => page.device.setLod(page.device.lodOptions[i])
            }
        }
        SettingRow {
            icon: "waves"
            iconTint: Theme.accent
            title: "Motion Sync"
            subtitle: "Lines up sensor frames with USB polls for steadier tracking. Adds about half a poll of latency."
            Toggle { checked: page.device ? page.device.motionSync : false; onToggled: (v) => page.device.setMotionSync(v) }
        }
        SettingRow {
            icon: "arrow-left-right"
            iconTint: Theme.orange
            title: "Angle snapping"
            subtitle: "Straightens slightly wobbly lines. Most players leave this off."
            Toggle { checked: page.device ? page.device.angleSnap : false; onToggled: (v) => page.device.setAngleSnap(v) }
        }
        SliderRow {
            title: "Angle tune"
            subtitle: "Rotates the sensor's axis to match how you hold the mouse."
            from: -30
            to: 30
            stepSize: 1
            tickCount: 3
            value: page.device ? page.device.angleTune : 0
            format: (v) => (v > 0 ? "+" : "") + Math.round(v) + "°"
            minLabel: "−30°"
            maxLabel: "+30°"
            onCommitted: (v) => page.device.setAngleTune(Math.round(v))
        }
        SettingRow {
            icon: "scan-line"
            iconTint: Theme.indigo
            title: "Ripple control"
            subtitle: "Smooths jitter at very high DPI at the cost of a little latency."
            Toggle { checked: page.device ? page.device.rippleControl : false; onToggled: (v) => page.device.setRippleControl(v) }
        }
    }

    SectionHeader { text: "Speed" }
    Card {
        SettingRow {
            icon: "flame"
            iconTint: Theme.red
            title: "Competition mode"
            subtitle: "Runs the sensor at full speed all the time. Lowest latency, but the battery drains noticeably faster."
            Toggle { checked: page.device ? page.device.hyperMode : false; onToggled: (v) => page.device.setHyperMode(v) }
        }
        SettingRow {
            visible: page.device && page.device.trackingSupported
            icon: "zap"
            iconTint: Theme.yellow
            title: "20K FPS tracking"
            subtitle: "Needs competition mode."
            enabled: page.device && page.device.hyperMode
            Toggle { checked: page.device ? page.device.trackingMode : false; onToggled: (v) => page.device.setTrackingMode(v) }
        }
    }

    SectionHeader { text: "Clicks" }
    Card {
        SliderRow {
            title: "Debounce time"
            subtitle: "Raise this only if you get accidental double-clicks."
            from: 0
            to: 15
            stepSize: 1
            value: page.device ? page.device.debounce : 0
            format: (v) => Math.round(v) + " ms"
            minLabel: "0 ms"
            maxLabel: "15 ms"
            onCommitted: (v) => page.device.setDebounce(Math.round(v))
        }
    }
}
