import QtQuick
import QtQuick3D

// Flat cylinder facing the camera, used for the ball and as a marker
Node {
    id: root

    property real radius: 1.0
    property real thickness: 0.5
    property color color: Theme.accent

    Model {
        source: "#Cylinder"
        eulerRotation.x: 90
        scale: Qt.vector3d(root.radius / 50, root.thickness / 100, root.radius / 50)
        materials: DefaultMaterial {
            diffuseColor: root.color
            specularAmount: 0.0
        }
    }
}
