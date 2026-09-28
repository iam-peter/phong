import QtQuick
import QtQuick3D
import QtQuick3D.Helpers
import Phong

// Stars far behind all scenes, a single instanced draw call
Node {
    id: root

    property int count: 900

    NumberAnimation on eulerRotation.z {
        from: 0
        to: 360
        duration: 900000
        loops: Animation.Infinite
    }

    Model {
        source: "#Cube"
        scale: Qt.vector3d(0.004, 0.004, 0.004)
        instancing: RandomInstancing {
            instanceCount: root.count
            randomSeed: 7
            position: InstanceRange {
                from: Qt.vector3d(-260, -200, -300)
                to: Qt.vector3d(260, 200, -70)
            }
            scale: InstanceRange {
                from: Qt.vector3d(0.4, 0.4, 0.4)
                to: Qt.vector3d(2.0, 2.0, 2.0)
                proportional: true
            }
            rotation: InstanceRange {
                from: Qt.vector3d(0, 0, 0)
                to: Qt.vector3d(360, 360, 360)
            }
            color: InstanceRange {
                from: Theme.starFrom
                to: Theme.starTo
            }
        }
        materials: DefaultMaterial {
            diffuseColor: "white"
            lighting: DefaultMaterial.NoLighting
        }
    }
}
