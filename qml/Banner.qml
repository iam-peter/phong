import QtQuick
import QtQuick3D

// A short announcement that pops up and fades away
Node {
    id: root

    function show(text, color) {
        label.text = text
        label.color = color
        animation.restart()
    }

    opacity: 0.0
    scale: Qt.vector3d(1.5, 1.5, 1.5)

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
                from: Qt.vector3d(0.8, 0.8, 0.8); to: Qt.vector3d(1.5, 1.5, 1.5)
                duration: 250; easing.type: Easing.OutBack
            }
        }
        PauseAnimation { duration: 1100 }
        NumberAnimation { target: root; property: "opacity"; to: 0.0; duration: 400 }
    }
}
