import QtQuick
import QtQuick3D

// A short text rising and fading where something happened
Node {
    id: root

    property real textScale: 0.6
    property real startY: 0.0

    function show(text, color, position) {
        label.text = text
        label.color = color
        x = position.x
        startY = position.y + 1.2
        animation.restart()
    }

    z: 1.5
    opacity: 0.0

    Text3D {
        id: label
        scale: Qt.vector3d(root.textScale, root.textScale, root.textScale)
        horizontalAlignment: Text.AlignHCenter
        glow: 0.5
    }

    ParallelAnimation {
        id: animation
        NumberAnimation {
            target: root; property: "opacity"
            from: 1.0; to: 0.0; duration: 1400; easing.type: Easing.InQuad
        }
        NumberAnimation {
            target: root; property: "y"
            from: root.startY; to: root.startY + 2.0; duration: 1400
        }
    }
}
