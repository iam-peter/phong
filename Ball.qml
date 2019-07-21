import QtQuick 2.13
import Qt3D.Core 2.13
import Qt3D.Extras 2.13

Entity {
    id: ballEntity

    property vector3d position: Qt.vector3d(0, 0, 0)
    property real radius: 1.0
    property color ambientColor: "yellow"

    CylinderMesh {
        id: ballMesh
        length: 0.5
        radius: 1.0
    }

    PhongMaterial {
        id: ballMaterial
        ambient: ballEntity.ambientColor
    }

    Transform {
        id: ballTransform
        translation: ballEntity.position
        rotationX: 90.0
    }

    components: [ballMesh, ballMaterial, ballTransform]
}
