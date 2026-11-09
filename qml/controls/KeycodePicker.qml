import QtQuick
import QtQuick.Layouts
import QtQuick.Templates as T
import Flux

// Sheet for choosing a keyboard keycode: press a key, search, or browse groups.
Sheet {
    id: root
    property var device: null
    property int current: 0
    property string heading: "Choose a key"
    property int fnCode: 0x5221        // MO(1) on Win layers, MO(3) on Mac layers
    signal picked(int code)

    title: heading
    sheetWidth: 640
    property string query: ""
    readonly property var groups: device ? device.keycodeGroups() : []

    onOpened: { query = ""; search.text = "" }

    RowLayout {
        width: parent.width
        spacing: 10
        KeyCapture {
            Layout.preferredWidth: 220
            onCaptured: (usage, mods) => { root.picked(usage); root.close() }
        }
        FButton {
            text: "Fn"
            style: root.current === root.fnCode ? "filled" : "gray"
            onClicked: { root.picked(root.fnCode); root.close() }
        }
        FTextField {
            id: search
            Layout.fillWidth: true
            placeholderText: "Search keys"
            onTextChanged: root.query = text.toLowerCase()
        }
    }

    Flickable {
        width: parent.width
        height: Math.min(contentHeight, 380)
        contentHeight: groupsCol.implicitHeight
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        T.ScrollBar.vertical: ThinScrollBar {}

        Column {
            id: groupsCol
            width: parent.width - 8
            spacing: 6
            Repeater {
                model: root.groups
                delegate: Column {
                    id: grp
                    required property var modelData
                    readonly property var keys: modelData.keys.filter((k) => root.query === "" || k.name.toLowerCase().indexOf(root.query) >= 0)
                    visible: keys.length > 0
                    width: groupsCol.width
                    spacing: 6
                    FText {
                        text: grp.modelData.title
                        color: Theme.secondaryLabel
                        font.pixelSize: Theme.footnote
                        font.weight: Font.DemiBold
                        topPadding: 6
                    }
                    Flow {
                        width: parent.width
                        spacing: 6
                        Repeater {
                            model: grp.keys
                            delegate: Rectangle {
                                required property var modelData
                                readonly property bool cur: modelData.code === root.current
                                width: Math.max(40, keyTxt.implicitWidth + 18)
                                height: 32
                                radius: 7
                                color: cur ? Theme.accent : (h.hovered ? Theme.secondaryFill : Theme.tertiaryFill)
                                scale: t.pressed ? 0.92 : 1
                                Behavior on scale { SpringPress {} }
                                FText {
                                    id: keyTxt
                                    anchors.centerIn: parent
                                    text: modelData.name
                                    font.pixelSize: Theme.footnote
                                    font.weight: Font.Medium
                                    color: parent.cur ? "white" : Theme.label
                                }
                                HoverHandler { id: h; cursorShape: Qt.PointingHandCursor }
                                TapHandler { id: t; onTapped: { root.picked(modelData.code); root.close() } }
                            }
                        }
                    }
                }
            }
        }
    }

    footer: [ FButton { text: "Cancel"; onClicked: root.close() } ]
}
