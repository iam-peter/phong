import QtQuick
import QtQuick3D
import Phong

Node {
    id: root

    property alias text: label.text
    property alias textWidth: label.textWidth
    property bool selected: false
    property bool available: true
    property bool centered: true
    property bool arrows: false
    property real markerGap: arrows ? 2.8 : 1.0
    property color selectedColor: Theme.text
    property color unselectedColor: Theme.unselected
    property color disabledColor: Theme.dimmed

    signal clicked
    signal decreased
    signal increased

    Text3D {
        id: label

          horizontalAlignment: root.centered ? Text.AlignHCenter : Text.AlignLeft
          color: !root.available ? root.disabledColor
               : root.selected ? root.selectedColor : root.unselectedColor
          clickable: root.available
        onClicked: root.clicked()
    }

    Text3D {
        visible: root.selected && root.arrows && root.available
        x: -0.5 * label.textWidth - 0.9
        horizontalAlignment: Text.AlignRight
        color: Theme.title
        text: "<"
        clickable: visible
        hitLeft: 3.0
        hitRight: 0.5
        hitVertical: 0.35
        onClicked: root.decreased()
    }

    Text3D {
        visible: root.selected && root.arrows && root.available
        x: 0.5 * label.textWidth + 0.9
        color: Theme.title
        text: ">"
        clickable: visible
        hitLeft: 0.5
        hitRight: 3.0
        hitVertical: 0.35
        onClicked: root.increased()
    }

    Disc {
        visible: root.selected
        sphere: true
        position: Qt.vector3d(root.centered ? -0.5 * label.textWidth - root.markerGap : -1.0,
                              0.35, 0)
        radius: 0.35
    }
}