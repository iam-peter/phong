pragma ComponentBehavior: Bound

import QtQuick
import QtQuick3D
import QtQuick3D.Physics
import Phong

// The physics body of a ball. The engine moves it, the rules in Match set
// its velocity.
DynamicRigidBody {
    id: root

    required property Ball ball
    property real radius: 0.8
    // Speed from which the trail shows
    property real trailSpeed: 16.0

    // Contact reports go to the game scene
    signal contact(var body, var normals)

    // Degrees the ball has turned, only for the looks
    property real turn: 0.0
    property var trail: []
    readonly property real speed: ball.velocity.length()
    readonly property real trailStrength: Math.max(0.0, Math.min(1.0, (speed / trailSpeed - 1.05) / 0.7))

    // Called after every physics step
    function advance(dt) {
        turn = (turn + ball.spin * dt * 180.0 / Math.PI * 6.0) % 360

        const points = trail.slice(0, 7)
        points.unshift(Qt.vector3d(x, y, 0))
        trail = points
    }

    function clearTrail() {
        trail = []
    }

    gravityEnabled: false
    linearAxisLock: DynamicRigidBody.LockZ
    angularAxisLock: DynamicRigidBody.LockX | DynamicRigidBody.LockY | DynamicRigidBody.LockZ
    receiveContactReports: true
    sendTriggerReports: true
    // Balls pass through each other
    filterGroup: 1
    filterIgnoreGroups: 1 << 1
    physicsMaterial: PhysicsMaterial {
        restitution: 1.0
        staticFriction: 0.0
        dynamicFriction: 0.0
    }
    collisionShapes: SphereShape {
        diameter: 2.0 * root.radius
    }

    position: Qt.vector3d(ball.spawnPosition.x, ball.spawnPosition.y, 0)

    onBodyContact: (body, positions, impulses, normals) => root.contact(body, normals)

    Component.onCompleted: setLinearVelocity(Qt.vector3d(ball.velocity.x, ball.velocity.y, 0))

    Connections {
        target: root.ball
        function onVelocityChanged() {
            root.setLinearVelocity(Qt.vector3d(root.ball.velocity.x, root.ball.velocity.y, 0))
        }
    }

    // Extra balls blink before they go
    readonly property bool leaving: ball.extra && ball.lifetime < 2.0

    Node {
        id: body
        eulerRotation.z: root.turn
        opacity: root.leaving ? blink.value : 1.0

        Disc {
            radius: root.radius
            color: root.ball.extra ? Theme.extraBall : Theme.ball
        }

        // A mark to see the spin by
        Disc {
            position: Qt.vector3d(0.5 * root.radius, 0, 0.1)
            radius: 0.2 * root.radius
            thickness: 0.4
            color: Theme.ballMark
        }
    }

    QtObject {
        id: blink
        property real value: 1.0
        SequentialAnimation on value {
            running: root.leaving
            loops: Animation.Infinite
            NumberAnimation { to: 0.25; duration: 120 }
            NumberAnimation { to: 1.0; duration: 120 }
        }
    }

    // Afterimages, the trail points are in scene coordinates while the
    // ghosts move with the body
    Repeater3D {
        model: 7

        delegate: Node {
            id: ghost

            required property int index

            readonly property var point: root.trail[index + 1]
            visible: point !== undefined && root.trailStrength > 0.0
            position: point !== undefined ? point.minus(Qt.vector3d(root.x, root.y, 0)).minus(Qt.vector3d(0, 0, 0.3))
                                          : Qt.vector3d(0, 0, 0)
            opacity: root.trailStrength * 0.5 * (1.0 - index / 7)

            Disc {
                radius: root.radius * (1.0 - 0.08 * ghost.index)
                thickness: 0.1
                color: root.ball.extra ? Theme.extraBall : Theme.ball
            }
        }
    }
}
