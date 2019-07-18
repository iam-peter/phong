import QtQuick 2.13
import Qt3D.Core 2.13
import Qt3D.Render 2.13

Entity {
    id: scene

    property var position: Qt.vector3d(0, 0, 0)

    property Item root
    property Camera camera

    Transform {
        id: sceneTransform
        translation: scene.position
    }

    components: [sceneTransform]
}
