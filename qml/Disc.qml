import QtQuick
import QtQuick3D

// Flat cylinder facing the camera, or a sphere, for balls and markers
Node {
    id: root

    property real radius: 1.0
    property real thickness: 0.5
    property color color: Theme.accent
    property bool sphere: false
    property real glow: 0.25
    property real shininess: 0.6

    Model {
        source: root.sphere ? "#Sphere" : "#Cylinder"
        eulerRotation.x: 90
        scale: root.sphere ? Qt.vector3d(root.radius / 50, root.radius / 50, root.radius / 50)
                           : Qt.vector3d(root.radius / 50, root.thickness / 100, root.radius / 50)
        materials: PhongMaterial {
            color: root.color
            glow: root.glow
            shininess: root.shininess
        }
    }
}
