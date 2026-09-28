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
    property color color: Theme.paddle
    // Smash wind up, 0 to 1, the paddle glows and trembles
    property real charge: 0.0
    // Squashes on a hit
    property real squash: 1.0
    // Direction of a dash, 0 while not dashing, leaves afterimages
    property int dash: 0
    // Catches balls, glows at the face in the magnet color
    property bool magnet: false
    property color magnetColor: "#ff3333"

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
        x: root.charge > 0.05 ? 0.06 * root.charge * Math.sin(tremble.phase) : 0.0
        scale: Qt.vector3d(root.width * root.squash * (1.0 + 0.25 * root.charge) / 100, root.length / 100, 0.01)
        materials: PhongMaterial {
            id: material
            color: root.color
            glow: 0.6 + 1.4 * root.charge
            shininess: 0.7
        }
    }

    // Afterimages behind a dashing paddle
    Repeater3D {
        model: 3

        delegate: Model {
            required property int index

            visible: root.dash !== 0
            y: -root.dash * (0.8 + 0.8 * index)
            z: -0.2
            opacity: 0.35 - 0.1 * index
            source: "#Cube"
            scale: Qt.vector3d(root.width / 100, root.length / 100, 0.01)
            materials: PhongMaterial {
                color: root.color
                glow: 0.8
            }
        }
    }

    // A magnet sits on the face, towards the field
    Model {
        id: magnetFace
        visible: root.magnet
        property real pulse: 0.0
        x: (root.paddleX < 0 ? 1 : -1) * (0.5 * root.width + 0.15)
        source: "#Cube"
        scale: Qt.vector3d(0.3 / 100, (root.length + 0.3) / 100, 0.014)
        materials: PhongMaterial {
            color: Theme.tint(root.magnetColor)
            glow: 0.8 + 1.2 * magnetFace.pulse
        }

        SequentialAnimation on pulse {
            running: root.magnet
            loops: Animation.Infinite
            NumberAnimation { from: 0.0; to: 1.0; duration: 400; easing.type: Easing.InOutSine }
            NumberAnimation { from: 1.0; to: 0.0; duration: 400; easing.type: Easing.InOutSine }
        }
    }

    QtObject {
        id: tremble
        property real phase: 0.0
        NumberAnimation on phase {
            running: root.charge > 0.05
            from: 0
            to: 2 * Math.PI
            duration: 90
            loops: Animation.Infinite
        }
    }

    SequentialAnimation {
        id: flashAnimation
        ParallelAnimation {
            ColorAnimation { target: material; property: "color"; to: Theme.text; duration: 40 }
            NumberAnimation { target: root; property: "squash"; to: 1.7; duration: 40 }
        }
        ParallelAnimation {
            ColorAnimation { target: material; property: "color"; to: root.color; duration: 220 }
            NumberAnimation { target: root; property: "squash"; to: 1.0; duration: 220; easing.type: Easing.OutBack }
        }
    }
}
