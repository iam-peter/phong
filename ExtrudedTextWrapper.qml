import QtQuick 2.13
import Qt3D.Core 2.13
import Qt3D.Extras 2.13

Entity {
    id: root

    property int horizontalAlignment: Text.AlignLeft
    property int verticalAlignment: Text.AlignBottom

    property alias minExt: mesh.minExt
    property alias maxExt: mesh.maxExt

    property alias ambientColor: material.ambient
    property alias specularColor: material.specular

    property alias text: mesh.text
    property alias depth: mesh.depth
    property alias font: mesh.font

    property real width: mesh.maxExt.x - mesh.minExt.x
    property real height: mesh.maxExt.y - mesh.minExt.y

    ExtrudedTextMesh {
        id: mesh
        property var minExt: geometry.minExtent
        property var maxExt: geometry.maxExtent
    }

    PhongMaterial {
        id: material
        ambient: "#FFFFFF"
    }

    Transform {
        id: transform
        translation: Qt.vector3d(calculateHorizontalPosition(),
                                 calculateVerticalPosition(),
                                 0)
    }

    components: [mesh, material, transform]

    function calculateHorizontalPosition() {
        if (horizontalAlignment === Text.AlignRight) {
            return -width
        } else if (horizontalAlignment === Text.AlignHCenter){
            return width * -0.5
        } else {
            return 0
        }
    }

    function calculateVerticalPosition() {
        if (verticalAlignment === Text.AlignTop) {
            return -height
        } else if (verticalAlignment === Text.AlignVCenter){
            return height * -0.5
        } else {
            return 0
        }
    }
}
