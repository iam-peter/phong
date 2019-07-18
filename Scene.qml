import QtQuick 2.13
import Qt3D.Core 2.13
import Qt3D.Render 2.13

Entity {
    id: scene

    property real x: 0.0
    property real y: 0.0

    property Item root
    property Camera camera

    Transform {
        id: sceneTransform
        translation: Qt.vector3d(scene.x, scene.y, 0)
    }

    components: [sceneTransform]
}
