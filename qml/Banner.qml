import QtQuick
import QtQuick3D

// A short announcement that pops up and fades away
Node {
    id: root

    property real size: 1.5

    function show(text, color, size) {
        label.text = text
        label.color = color
        root.size = size ?? 1.5
        animation.restart()
    }

    opacity: 0.0
    scale: Qt.vector3d(size, size, size)

    Text3D {
        id: label
        horizontalAlignment: Text.AlignHCenter
        glow: 0.8
    }

    SequentialAnimation {
        id: animation
        ParallelAnimation {
            NumberAnimation { target: root; property: "opacity"; from: 0.0; to: 1.0; duration: 150 }
            Vector3dAnimation {
                target: root; property: "scale"
                from: Qt.vector3d(0.5 * root.size, 0.5 * root.size, 0.5 * root.size)
                to: Qt.vector3d(root.size, root.size, root.size)
                duration: 250; easing.type: Easing.OutBack
            }
        }
        PauseAnimation { duration: 1100 }
        NumberAnimation { target: root; property: "opacity"; to: 0.0; duration: 400 }
    }
}
