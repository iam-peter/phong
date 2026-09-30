import QtQuick
import QtQuick3D
import Phong

Node {
    id: root

    property alias text: inputText.text
    property alias textWidth: inputText.textWidth
    property bool selected: false
    property bool editing: false

    signal clicked

    Text3D {
        id: inputText

        horizontalAlignment: Text.AlignRight
        color: root.editing ? Theme.title : root.selected ? Theme.text : Theme.dimmed
        clickable: true
        onClicked: root.clicked()
    }

    Text3D {
        visible: root.editing
        color: Theme.title
        text: "_"
    }
}