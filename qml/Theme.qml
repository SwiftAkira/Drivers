pragma Singleton
import QtQuick

// Apple-style dark palette (macOS / iOS "dark" system colors).
QtObject {
    readonly property string fontFamily: "Inter Variable"

    // Surfaces
    readonly property color window: "#1c1c1e"
    readonly property color sidebar: Qt.rgba(0.17, 0.17, 0.18, 0.78)
    readonly property color sidebarOpaque: "#262628"
    readonly property color card: "#2c2c2e"
    readonly property color cardHover: "#323235"
    readonly property color elevated: "#3a3a3c"
    readonly property color popover: "#2f2f31"
    readonly property color separator: Qt.rgba(1, 1, 1, 0.08)
    readonly property color hairline: Qt.rgba(1, 1, 1, 0.12)
    readonly property color windowBorder: Qt.rgba(1, 1, 1, 0.10)

    // Text
    readonly property color label: "#f5f5f7"
    readonly property color secondaryLabel: Qt.rgba(0.92, 0.92, 0.96, 0.60)
    readonly property color tertiaryLabel: Qt.rgba(0.92, 0.92, 0.96, 0.32)
    readonly property color quaternaryLabel: Qt.rgba(0.92, 0.92, 0.96, 0.18)

    // Fills (iOS system fills, dark)
    readonly property color fill: Qt.rgba(0.47, 0.47, 0.50, 0.36)
    readonly property color secondaryFill: Qt.rgba(0.47, 0.47, 0.50, 0.32)
    readonly property color tertiaryFill: Qt.rgba(0.46, 0.46, 0.50, 0.24)
    readonly property color quaternaryFill: Qt.rgba(0.46, 0.46, 0.50, 0.18)

    // System tints
    readonly property color accent: "#0a84ff"
    readonly property color accentPressed: "#0071e3"
    readonly property color green: "#30d158"
    readonly property color orange: "#ff9f0a"
    readonly property color red: "#ff453a"
    readonly property color yellow: "#ffd60a"
    readonly property color purple: "#bf5af2"
    readonly property color pink: "#ff375f"
    readonly property color teal: "#64d2ff"
    readonly property color indigo: "#5e5ce6"
    readonly property color mint: "#63e6e2"
    readonly property color gray: "#8e8e93"

    // Geometry
    readonly property int radiusWindow: 12
    readonly property int radiusCard: 12
    readonly property int radiusControl: 8
    readonly property int radiusSmall: 6
    readonly property int rowHeight: 46
    readonly property int pagePadding: 28
    readonly property int contentMaxWidth: 760

    // Type scale (px)
    readonly property int largeTitle: 26
    readonly property int title: 20
    readonly property int title3: 17
    readonly property int headline: 14
    readonly property int body: 14
    readonly property int callout: 13
    readonly property int footnote: 12
    readonly property int caption: 11

    function alpha(c, a) { return Qt.rgba(c.r, c.g, c.b, a) }
}
