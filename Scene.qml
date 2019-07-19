import QtQuick 2.13
import Qt3D.Core 2.13
import Qt3D.Render 2.13
import Qt3D.Input 2.13

Entity {
    id: scene

    property bool active: false
    property var position: Qt.vector3d(0, 0, 0)

    property Item root
    property Camera camera
    property string menuItemName: "unknown"

    Transform {
        id: sceneTransform
        translation: scene.position
    }

    components: [sceneTransform]

/*
    property KeyboardHandler keyboardHandler: KeyboardHandler {
        sourceDevice: root.keyboardDevice
        focus: true
        enabled: true
    }
*/
}
