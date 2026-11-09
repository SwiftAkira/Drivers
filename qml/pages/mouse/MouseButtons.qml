import QtQuick
import QtQuick.Layouts
import Flux

ScrollPage {
    id: page
    title: "Buttons"
    subtitle: "Click a button on the mouse or in the list to change what it does."

    property int selected: -1

    function openEditor(b) {
        page.selected = b.id
        editor.openFor(b.id, b.name, b.action)
    }

    RowLayout {
        width: parent.width
        spacing: 22

        Rectangle {
            Layout.preferredWidth: 250
            Layout.preferredHeight: 380
            Layout.alignment: Qt.AlignTop
            radius: 16
            gradient: Gradient {
                GradientStop { position: 0; color: "#2f3036" }
                GradientStop { position: 1; color: "#232327" }
            }
            border.color: Qt.rgba(1, 1, 1, 0.05)
            MouseIllustration {
                anchors.fill: parent
                anchors.margins: 22
                selected: page.selected
                onPicked: (id) => {
                    const b = page.device.buttons.find((x) => x.id === id)
                    if (b) page.openEditor(b)
                }
            }
        }

        Card {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignTop
            width: undefined
            Repeater {
                model: page.device ? page.device.buttons : []
                delegate: SettingRow {
                    required property var modelData
                    title: modelData.name
                    subtitle: modelData.label
                    showChevron: true
                    onClicked: page.openEditor(modelData)
                    Row {
                        spacing: 6
                        Rectangle {
                            visible: !modelData.isDefault
                            width: 8; height: 8; radius: 4
                            color: Theme.accent
                            anchors.verticalCenter: parent.verticalCenter
                        }
                        FButton {
                            visible: !modelData.isDefault
                            compact: true
                            style: "plain"
                            icon: "rotate-ccw"
                            onClicked: page.device.resetButton(modelData.id)
                        }
                    }
                }
            }
        }
    }

    ActionEditor {
        id: editor
        device: page.device
        onApply: (id, action) => page.device.setButtonAction(id, action)
        onClosed: page.selected = -1
    }
}
