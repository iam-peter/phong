import Qt3D.Core 2.13
import Qt3D.Render 2.13
import Qt3D.Input 2.13
import Qt3D.Extras 2.13

Scene {
    id: sceneRoot

    x: 10.0
    y: 10.0

    PhongMaterial {
        id: fieldMaterial
        ambient: "blue"
    }

    PhongMaterial {
        id: scoreMaterial
        ambient: "yellow"
    }

    ExtrudedTextMesh {
        id: textMesh
        text: "0 : 99"
        depth: 1.0
    }

    Transform {
        id: textTransform
        //scale3D: Qt.vector3d(2, 2, 2)
        translation: Qt.vector3d(0, 0, 2)
    }

    Entity {
        id: textEntity
        components: [textMesh, scoreMaterial, textTransform]
    }


    PlaneMesh {
        id: torusMesh
    }

    Transform {
        id: torusTransform
        scale3D: Qt.vector3d(10, 1, 10)
        rotation: fromAxisAndAngle(Qt.vector3d(1, 0, 0), 90)
    }

    Entity {
        id: torusEntity
        components: [torusMesh, fieldMaterial, torusTransform]
    }
}
