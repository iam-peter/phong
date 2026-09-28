import QtQuick
import QtQuick3D
import QtQuick3D.Physics

// A paddle, moved by its player and pushed around by nothing
DynamicRigidBody {
    id: root

    property real paddleX: 0.0
    property real paddleY: 0.0
    property real angle: 0.0
    property real length: 4.0
    property real width: 1.0
    // Squashes on a hit
    property real squash: 1.0

    function flash() {
        flashAnimation.restart()
    }

    isKinematic: true
    // Created at the target, not at the origin on top of the ball. Only
    // the start, the engine writes the simulated pose back to position.
    position: kinematicPosition
    kinematicPosition: Qt.vector3d(paddleX, paddleY, 0)
    kinematicEulerRotation: Qt.vector3d(0, 0, angle)
    sendContactReports: true
    physicsMaterial: PhysicsMaterial {
        restitution: 1.0
        staticFriction: 0.0
        dynamicFriction: 0.0
    }
    collisionShapes: BoxShape {
        extents: Qt.vector3d(root.width, root.length, 1.0)
    }

    Model {
        source: "#Cube"
        scale: Qt.vector3d(root.width * root.squash / 100, root.length / 100, 0.01)
        materials: DefaultMaterial {
            id: material
            diffuseColor: Theme.paddle
            specularAmount: 0.0
        }
    }

    SequentialAnimation {
        id: flashAnimation
        ParallelAnimation {
            ColorAnimation { target: material; property: "diffuseColor"; to: Theme.text; duration: 40 }
            NumberAnimation { target: root; property: "squash"; to: 1.7; duration: 40 }
        }
        ParallelAnimation {
            ColorAnimation { target: material; property: "diffuseColor"; to: Theme.paddle; duration: 220 }
            NumberAnimation { target: root; property: "squash"; to: 1.0; duration: 220; easing.type: Easing.OutBack }
        }
    }
}
