import QtQuick 2.13
import Qt3D.Core 2.13
import Qt3D.Render 2.13
import Qt3D.Input 2.13
import Qt3D.Extras 2.13
import "logic.js" as Logic

Entity {
    id: wallEntity

    property int anchor
    property vector3d position: Qt.vector3d(0, 0, 0)
    property vector3d scale: Qt.vector3d(1, 1, 1)

    PhongMaterial {
        id: wallMaterial
        ambient: "grey"
    }
    CuboidMesh {
        id: wallMesh
    }

    Transform {
        id: wallTransform
        scale3D: wallEntity.scale
        translation: wallEntity.position
    }
    
    components: [wallMesh, wallMaterial, wallTransform]
}
