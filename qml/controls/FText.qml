import QtQuick
import Flux

Text {
    color: Theme.label
    font.family: Theme.fontFamily
    font.pixelSize: Theme.body
    font.features: { "tnum": 1 }
    renderType: Text.QtRendering
    elide: Text.ElideRight
    verticalAlignment: Text.AlignVCenter
}
