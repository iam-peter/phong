import QtQuick
import QtQuick3D

// Look of a modifier, on the field and as icon next to the scores
Node {
    id: root

    property int kind: 0
    property real radius: 0.9
    property bool wobbling: true

    Node {
        id: body

        SequentialAnimation on eulerRotation.y {
            running: root.wobbling
            loops: Animation.Infinite
            NumberAnimation { from: -35; to: 35; duration: 1200; easing.type: Easing.InOutSine }
            NumberAnimation { from: 35; to: -35; duration: 1200; easing.type: Easing.InOutSine }
        }

        Disc {
            radius: root.radius
            thickness: 0.3
            color: Theme.modifierColors[root.kind]
        }

        Text3D {
            z: 0.15
            scale: Qt.vector3d(root.radius, root.radius, 1.0)
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            depth: 0.2
            color: "black"
            text: Theme.modifierGlyphs[root.kind]
        }
    }
}
