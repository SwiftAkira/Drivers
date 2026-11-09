pragma Singleton
import QtQuick

// One place for every spring/easing so the whole app moves the same way.
// Values tuned for a light, slightly bouncy Apple feel: quick settle,
// a few percent of overshoot, never wobbly.
QtObject {
    readonly property bool reduce: App.reduceMotion

    // Springs for SpringAnimation (spring = stiffness, damping 0..1)
    readonly property real springStiff: 5.2      // selection pills, toggles
    readonly property real springDampStiff: reduce ? 1.0 : 0.34
    readonly property real springSoft: 3.6       // page/sheet entrances
    readonly property real springDampSoft: reduce ? 1.0 : 0.30
    readonly property real springPress: 7.0      // button press scale
    readonly property real springDampPress: reduce ? 1.0 : 0.38
    readonly property real springMass: 1.0
    readonly property real epsilon: 0.002

    // Durations for non-positional properties (opacity, color)
    readonly property int fast: 140
    readonly property int normal: 220
    readonly property int slow: 340

    readonly property int easeOut: Easing.OutCubic
    readonly property int easeInOut: Easing.InOutCubic

    // Press feedback scale
    readonly property real pressScale: 0.96
}
