import QtQuick
import QtQuick3D
import Phong

Node {
    id: root

    property alias text: valueText.text
    property alias textWidth: valueText.textWidth
    property bool selected: false
    property bool available: true
    property bool cycles: false
    property color selectedColor: Theme.text
    property color unselectedColor: Theme.dimmed

    signal clicked
    signal decreased
    signal increased

    Text3D {
        id: valueText

        horizontalAlignment: Text.AlignRight
        color: root.selected ? root.selectedColor : root.unselectedColor
        clickable: root.available
        onClicked: root.clicked()
    }

    Text3D {
        visible: root.selected && root.cycles && root.available
        x: -valueText.textWidth - 0.7
        horizontalAlignment: Text.AlignRight
        color: Theme.title
        text: "<"
        clickable: visible
        hitLeft: 1.5
        hitRight: 0.35
        onClicked: root.decreased()
    }

    Text3D {
        visible: root.selected && root.cycles && root.available
        x: 0.7
        color: Theme.title
        text: ">"
        clickable: visible
        hitLeft: 0.35
        hitRight: 3.0
        onClicked: root.increased()
    }
}