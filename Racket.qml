import QtQuick 2.13
import Qt3D.Core 2.13
import Qt3D.Extras 2.13

Entity {
    id: racketEntity

    property vector3d position: Qt.vector3d(0, 4, 0)
    property vector3d scale: Qt.vector3d(1, 1, 1)
    property color ambientColor: "red"

    CuboidMesh {
        id: racketMesh
    }

    PhongMaterial {
        id: racketMaterial
        ambient: racketEntity.ambientColor
    }

    Transform {
        id: racketTransform
        scale3D: racketEntity.scale
        translation: racketEntity.position
    }

    components: [racketMesh, racketMaterial, racketTransform]
}
