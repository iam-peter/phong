import QtQuick
import QtQuick3D

// Discs or spheres like Disc, all of them in one draw call, see Instances
Instances {
    property bool sphere: false
    property real radius: 1.0
    property real thickness: 0.5

    source: sphere ? "#Sphere" : "#Cylinder"
    meshRotation: Qt.vector3d(90, 0, 0)
    meshScale: sphere ? Qt.vector3d(radius / 50, radius / 50, radius / 50)
                      : Qt.vector3d(radius / 50, thickness / 100, radius / 50)
}
