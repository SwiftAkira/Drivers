import QtQuick
import QtQuick.Layouts
import QtQuick.Window
import Flux

Window {
    id: win
    property bool startHidden: false

    width: 1100
    height: 740
    minimumWidth: 900
    minimumHeight: 580
    visible: !startHidden
    color: "transparent"
    flags: Qt.Window | Qt.FramelessWindowHint
    title: "Flux"

    readonly property bool maximized: visibility === Window.Maximized || visibility === Window.FullScreen
    readonly property int corner: maximized ? 0 : Theme.radiusWindow
    readonly property int sidebarWidth: 252

    onClosing: (close) => {
        if (App.closeToTray) {
            close.accepted = false
            win.hide()
        } else {
            App.quit()
        }
    }

    WindowChrome {
        id: chrome
        window: win
        radius: Theme.radiusWindow
        blurRect: Qt.rect(0, 0, win.sidebarWidth, win.height)
    }

    // ── Navigation ───────────────────────────────────────────────────────
    property string page: "home"
    Component.onCompleted: {
        const last = App.lastPage
        if (last) page = last
    }
    function go(id) {
        page = id
        App.lastPage = id
    }

    readonly property var mouseDev: Devices.mouse
    readonly property var keyboardDev: Devices.keyboard

    readonly property var nav: {
        const n = []
        if (mouseDev) {
            n.push({ type: "device", id: "mouse/overview", device: mouseDev })
            n.push({ type: "item", id: "mouse/performance", title: "Performance", icon: "gauge", tint: Theme.accent })
            n.push({ type: "item", id: "mouse/sensor", title: "Sensor", icon: "crosshair", tint: Theme.teal })
            n.push({ type: "item", id: "mouse/buttons", title: "Buttons", icon: "mouse-pointer-click", tint: Theme.orange })
            n.push({ type: "item", id: "mouse/macros", title: "Macros", icon: "workflow", tint: Theme.purple })
            n.push({ type: "item", id: "mouse/power", title: "Power", icon: "battery-charging", tint: Theme.green })
            n.push({ type: "gap" })
        }
        if (keyboardDev) {
            n.push({ type: "device", id: "keyboard/overview", device: keyboardDev })
            n.push({ type: "item", id: "keyboard/actuation", title: "Actuation", icon: "arrow-down-to-line", tint: Theme.accent })
            n.push({ type: "item", id: "keyboard/advanced", title: "Advanced Keys", icon: "combine", tint: Theme.purple })
            n.push({ type: "item", id: "keyboard/keymap", title: "Keymap", icon: "keyboard", tint: Theme.indigo })
            n.push({ type: "item", id: "keyboard/macros", title: "Macros", icon: "workflow", tint: Theme.pink })
            n.push({ type: "item", id: "keyboard/lighting", title: "Lighting", icon: "palette", tint: Theme.orange })
            n.push({ type: "item", id: "keyboard/travel", title: "Live Travel", icon: "activity", tint: Theme.green })
            n.push({ type: "item", id: "keyboard/settings", title: "Keyboard Settings", icon: "sliders-horizontal", tint: Theme.gray })
            n.push({ type: "gap" })
        }
        if (!mouseDev && !keyboardDev)
            n.push({ type: "item", id: "home", title: "Devices", icon: "laptop", tint: Theme.accent })
        n.push({ type: "item", id: "profiles", title: "Profiles", icon: "layers", tint: Theme.indigo })
        n.push({ type: "item", id: "settings", title: "Flux Settings", icon: "settings", tint: Theme.gray })
        return n
    }

    readonly property var pageFiles: ({
        "home": "pages/Home.qml",
        "profiles": "pages/Profiles.qml",
        "settings": "pages/AppSettings.qml",
        "mouse/overview": "pages/mouse/MouseOverview.qml",
        "mouse/performance": "pages/mouse/MousePerformance.qml",
        "mouse/sensor": "pages/mouse/MouseSensor.qml",
        "mouse/buttons": "pages/mouse/MouseButtons.qml",
        "mouse/macros": "pages/mouse/MouseMacros.qml",
        "mouse/power": "pages/mouse/MousePower.qml",
        "keyboard/overview": "pages/keyboard/KeyboardOverview.qml",
        "keyboard/actuation": "pages/keyboard/KeyboardActuation.qml",
        "keyboard/advanced": "pages/keyboard/KeyboardAdvanced.qml",
        "keyboard/keymap": "pages/keyboard/KeyboardKeymap.qml",
        "keyboard/macros": "pages/keyboard/KeyboardMacros.qml",
        "keyboard/lighting": "pages/keyboard/KeyboardLighting.qml",
        "keyboard/travel": "pages/keyboard/KeyboardTravel.qml",
        "keyboard/settings": "pages/keyboard/KeyboardSettings.qml"
    })

    // Fall back to a valid page when a device disappears.
    readonly property string effectivePage: {
        if (page.startsWith("mouse/") && !mouseDev) return mouseDev || keyboardDev ? (keyboardDev ? "keyboard/overview" : "home") : "home"
        if (page.startsWith("keyboard/") && !keyboardDev) return mouseDev ? "mouse/overview" : "home"
        if (page === "home" && (mouseDev || keyboardDev)) return mouseDev ? "mouse/overview" : "keyboard/overview"
        return pageFiles[page] ? page : "home"
    }
    readonly property var pageDevice: effectivePage.startsWith("mouse/") ? mouseDev
                                    : effectivePage.startsWith("keyboard/") ? keyboardDev : null

    Connections {
        target: App
        function onShowRequested() { win.visible = true }
    }

    // ── Window body ──────────────────────────────────────────────────────
    Item {
        id: body
        anchors.fill: parent

        // Sidebar (translucent; KWin blurs what's behind it)
        Rectangle {
            id: sidebar
            width: win.sidebarWidth
            height: parent.height
            color: chrome.blurAvailable ? Theme.sidebar : Theme.sidebarOpaque
            topLeftRadius: win.corner
            bottomLeftRadius: win.corner

            WindowControls {
                window: win
                x: 18
                y: 18
                z: 10
            }

            Flickable {
                id: navFlick
                anchors.fill: parent
                anchors.topMargin: 48
                contentHeight: navCol.implicitHeight + 24
                boundsBehavior: Flickable.StopAtBounds
                clip: true

                // Selection highlight that springs between rows.
                Rectangle {
                    id: highlight
                    property Item target: null
                    x: 10
                    width: parent.width - 20
                    y: target ? target.y + navCol.y : 0
                    height: target ? target.height : 0
                    visible: target !== null
                    radius: 8
                    color: target && target.isDevice ? Theme.alpha(Theme.label, 0.10) : Theme.accent
                    opacity: win.active ? 1 : 0.55
                    Behavior on y { SpringStiff {} }
                    Behavior on height { SpringStiff {} }
                    Behavior on color { ColorAnimation { duration: Motion.fast } }
                }

                Column {
                    id: navCol
                    y: 8
                    width: parent.width

                    Repeater {
                        id: navRep
                        model: win.nav
                        delegate: Loader {
                            id: navLoader
                            required property var modelData
                            required property int index
                            readonly property bool selected: modelData.id === win.effectivePage
                            readonly property bool isDevice: modelData.type === "device"
                            width: navCol.width
                            sourceComponent: modelData.type === "device" ? deviceRow
                                           : modelData.type === "gap" ? gapRow : itemRow
                            onSelectedChanged: if (selected) highlight.target = navLoader
                            Component.onCompleted: if (selected) highlight.target = navLoader
                        }
                    }
                }
            }
        }

        // Content
        Rectangle {
            id: content
            anchors.left: sidebar.right
            anchors.right: parent.right
            height: parent.height
            color: Theme.window
            topRightRadius: win.corner
            bottomRightRadius: win.corner

            Rectangle {
                // hairline between sidebar and content
                width: 1
                height: parent.height
                color: Qt.rgba(0, 0, 0, 0.35)
            }

            Loader {
                id: pageLoader
                anchors.fill: parent
                anchors.leftMargin: 1
                clip: true
                property string loadedId: ""
                function reload() {
                    loadedId = win.effectivePage
                    setSource(Qt.resolvedUrl(win.pageFiles[win.effectivePage]), { device: win.pageDevice })
                }
                Component.onCompleted: reload()
                Connections {
                    target: win
                    function onEffectivePageChanged() { if (pageLoader.loadedId !== win.effectivePage) pageLoader.reload() }
                    function onPageDeviceChanged() { pageLoader.reload() }
                }
            }

            // Device is connected but not yet loaded / asleep overlay.
            DeviceStatusOverlay {
                anchors.fill: parent
                device: win.pageDevice
            }
        }

        // Rounded hairline around the whole window (macOS dark window edge).
        Rectangle {
            anchors.fill: parent
            radius: win.corner
            color: "transparent"
            border.width: 1
            border.color: Theme.windowBorder
            visible: !win.maximized
        }
    }

    // ── Sidebar row components ───────────────────────────────────────────
    Component {
        id: itemRow
        Item {
            height: 32
            readonly property var d: parent ? parent.modelData : ({})
            readonly property bool selected: parent ? parent.selected : false
            Row {
                anchors.left: parent.left
                anchors.leftMargin: 18
                anchors.verticalCenter: parent.verticalCenter
                spacing: 10
                IconBadge {
                    icon: d.icon || ""
                    tint: d.tint || Theme.accent
                    size: 21
                    anchors.verticalCenter: parent.verticalCenter
                }
                FText {
                    text: d.title || ""
                    font.pixelSize: Theme.callout + 0.5
                    font.weight: Font.Medium
                    color: selected ? "white" : Theme.label
                    anchors.verticalCenter: parent.verticalCenter
                }
            }
            MouseArea {
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.rightMargin: 10
                cursorShape: Qt.PointingHandCursor
                onClicked: win.go(d.id)
            }
        }
    }

    Component {
        id: deviceRow
        Item {
            height: 58
            readonly property var d: parent ? parent.modelData : ({})
            readonly property var dev: d.device
            Row {
                anchors.left: parent.left
                anchors.leftMargin: 18
                anchors.verticalCenter: parent.verticalCenter
                spacing: 11
                Rectangle {
                    width: 36
                    height: 36
                    radius: 18
                    anchors.verticalCenter: parent.verticalCenter
                    gradient: Gradient {
                        GradientStop { position: 0; color: "#4a4a4f" }
                        GradientStop { position: 1; color: "#2c2c30" }
                    }
                    border.color: Theme.hairline
                    Icon {
                        anchors.centerIn: parent
                        name: dev && dev.kind === "mouse" ? "mouse" : "keyboard"
                        size: 19
                    }
                }
                Column {
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 2
                    FText {
                        text: dev ? dev.displayName || (dev.kind === "mouse" ? "Mouse" : "Keyboard") : ""
                        font.pixelSize: Theme.callout + 0.5
                        font.weight: Font.DemiBold
                        width: win.sidebarWidth - 90
                    }
                    Row {
                        spacing: 6
                        BatteryGlyph {
                            visible: dev && dev.kind === "mouse" && dev.online && dev.batteryLevel >= 0
                            level: visible ? dev.batteryLevel : 0
                            charging: visible ? dev.charging : false
                            glyphScale: 0.75
                            anchors.verticalCenter: parent.verticalCenter
                        }
                        FText {
                            text: {
                                if (!dev) return ""
                                if (!dev.ready) return "Connecting…"
                                if (dev.kind === "mouse") {
                                    if (!dev.online) return "Asleep · " + dev.connection
                                    return (dev.batteryLevel >= 0 ? dev.batteryLevel + "% · " : "") + dev.connection
                                }
                                return dev.connection
                            }
                            font.pixelSize: Theme.footnote
                            color: Theme.secondaryLabel
                        }
                    }
                }
            }
            MouseArea {
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.rightMargin: 10
                cursorShape: Qt.PointingHandCursor
                onClicked: win.go(d.id)
            }
        }
    }

    Component {
        id: gapRow
        Item { height: 14 }
    }

    Toast { id: toast }
    Connections {
        target: win.mouseDev
        ignoreUnknownSignals: true
        function onToast(message, isError) { toast.show(message, isError) }
    }
    Connections {
        target: win.keyboardDev
        ignoreUnknownSignals: true
        function onToast(message, isError) { toast.show(message, isError) }
    }
    function showToast(message, isError) { toast.show(message, isError) }

    // ── Frameless window move / resize ───────────────────────────────────
    Item {
        id: dragStrip
        anchors.left: parent.left
        anchors.right: parent.right
        height: 44
        z: 50
        DragHandler {
            target: null
            grabPermissions: PointerHandler.CanTakeOverFromAnything
            onActiveChanged: if (active) win.startSystemMove()
        }
        TapHandler {
            acceptedButtons: Qt.LeftButton
            gesturePolicy: TapHandler.DragThreshold
            onDoubleTapped: win.maximized ? win.showNormal() : win.showMaximized()
        }
    }

    Repeater {
        model: win.maximized ? [] : [
            { e: Qt.LeftEdge, x: 0, y: 10, w: 5, h: -20, c: Qt.SizeHorCursor },
            { e: Qt.RightEdge, x: -5, y: 10, w: 5, h: -20, c: Qt.SizeHorCursor },
            { e: Qt.TopEdge, x: 10, y: 0, w: -20, h: 4, c: Qt.SizeVerCursor },
            { e: Qt.BottomEdge, x: 10, y: -5, w: -20, h: 5, c: Qt.SizeVerCursor },
            { e: Qt.TopEdge | Qt.LeftEdge, x: 0, y: 0, w: 10, h: 10, c: Qt.SizeFDiagCursor },
            { e: Qt.TopEdge | Qt.RightEdge, x: -10, y: 0, w: 10, h: 10, c: Qt.SizeBDiagCursor },
            { e: Qt.BottomEdge | Qt.LeftEdge, x: 0, y: -10, w: 10, h: 10, c: Qt.SizeBDiagCursor },
            { e: Qt.BottomEdge | Qt.RightEdge, x: -10, y: -10, w: 10, h: 10, c: Qt.SizeFDiagCursor }
        ]
        delegate: MouseArea {
            z: 60
            x: modelData.x < 0 ? win.width + modelData.x : modelData.x
            y: modelData.y < 0 ? win.height + modelData.y : modelData.y
            width: modelData.w <= 0 ? win.width + modelData.w : modelData.w
            height: modelData.h <= 0 ? win.height + modelData.h : modelData.h
            cursorShape: modelData.c
            onPressed: win.startSystemResize(modelData.e)
        }
    }
}
