import QtQuick
import QtQuick3D
import Phong

// A small field of one player in the lobby, to try the controls while
// waiting: the paddle keeps a ball in play against the far wall, and the
// hits in a row count. In the lane's own coordinates the paddle moves
// along y at the right end and the ball comes to it along x, the lobby
// mirrors or turns the lane to the player's side.
Node {
    id: lane

    // Along the ball's way and along the paddle
    property real depth: 8.0
    property real span: 5.0
    // Paddle right, left or at the bottom
    property int orientation: PracticeLane.Right
    property color color: Theme.leftPlayer

    // Played on this machine, by the computer, or shown from the network
    property bool local: false
    property bool computer: false
    // -1 to 1 along the paddle, and where the pointer wants it, NaN for
    // nowhere
    property real input: 0.0
    property real pointer: NaN

    property real paddle: 0.0
    property vector2d ball: Qt.vector2d(0, 0)
    property vector2d velocity: Qt.vector2d(0, 0)
    property int streak: 0
    property int best: 0
    // Seconds until a missed ball comes back
    property real waiting: 0.6
    // Lights up with a hit
    property real flash: 0.0

    // The last state from the network, followed smoothly
    property var target: null

    readonly property real paddleLength: 1.4
    readonly property real paddleThickness: 0.3
    readonly property real paddleX: 0.5 * depth - 0.4
    readonly property real ballRadius: 0.22
    readonly property real wallThickness: 0.12
    readonly property real paddleSpeed: 8.0
    readonly property real serveSpeed: 4.5
    readonly property real maxSpeed: 9.0
    readonly property real limit: 0.5 * span - wallThickness
    // The left one is a mirror of the right, the bottom one turned
    readonly property real mirror: orientation === PracticeLane.Left ? -1.0 : 1.0

    enum Orientation {
        Right = 0,
        Left,
        Bottom
    }

    // Size on the screen
    readonly property real screenWidth: orientation === PracticeLane.Bottom ? span : depth
    readonly property real screenHeight: orientation === PracticeLane.Bottom ? depth : span

    // A hit at a position of the scene, and a ball gone by
    signal hit(vector3d position)
    signal missed()

    // For the network: [paddle, ball x, ball y, streak, best]
    function state() {
        const round = (v) => Math.round(v * 100) / 100
        return [round(paddle), round(ball.x), round(ball.y), streak, best]
    }

    function applyState(s) {
        // A list from the network isn't a JavaScript array
        if (!s || !(s.length >= 5))
            return
        const before = streak
        target = s
        streak = s[3]
        best = s[4]
        if (streak > before) {
            flash = 1.0
            hit(field.mapPositionToScene(Qt.vector3d(mirror * (paddleX - 0.3), paddle, 0.3)))
        }
    }

    // A point of the lane from one of the scene, for the pointer
    function fromScene(position) {
        return field.mapPositionFromScene(position)
    }

    function contains(position) {
        const at = fromScene(position)
        return Math.abs(at.x) <= 0.5 * depth + 0.5 && Math.abs(at.y) <= 0.5 * span + 0.5
    }

    function serve() {
        const angle = (Math.random() - 0.5) * 1.2
        ball = Qt.vector2d(-0.5 * depth + 1.0, (Math.random() - 0.5) * span * 0.5)
        velocity = Qt.vector2d(Math.cos(angle) * serveSpeed, Math.sin(angle) * serveSpeed)
    }

    function reset() {
        paddle = 0.0
        streak = 0
        best = 0
        target = null
        waiting = 0.6
        ball = Qt.vector2d(-0.5 * depth + 1.0, 0)
        velocity = Qt.vector2d(0, 0)
    }

    function step(dt) {
        flash = Math.max(0.0, flash - 3.0 * dt)
        if (!local) {
            follow(dt)
            return
        }

        // The paddle: keys and sticks, the pointer, or the computer
        let move = input
        if (computer) {
            // It looks ahead a little and is not always right
            const aim = velocity.x > 0 ? ball.y + velocity.y * Math.max(0, paddleX - ball.x) / velocity.x * 0.6
                                       : 0.0
            move = Math.max(-1, Math.min(1, (aim - paddle) / 0.4))
        }
        else if (move === 0 && !isNaN(pointer)) {
            move = Math.max(-1, Math.min(1, (pointer - paddle) / 0.5))
        }
        const reach = 0.5 * span - wallThickness - 0.5 * paddleLength
        paddle = Math.max(-reach, Math.min(reach, paddle + move * paddleSpeed * dt))

        if (waiting > 0) {
            waiting -= dt
            if (waiting <= 0)
                serve()
            return
        }

        const before = ball
        let x = ball.x + velocity.x * dt
        let y = ball.y + velocity.y * dt
        let vx = velocity.x
        let vy = velocity.y
        const edge = limit - ballRadius
        if (Math.abs(y) > edge) {
            y = Math.sign(y) * (2 * edge - Math.abs(y))
            vy = -vy
        }
        const back = -0.5 * depth + wallThickness + ballRadius
        if (x < back) {
            x = 2 * back - x
            vx = Math.abs(vx)
        }

        // The face of the paddle, crossed this step
        const face = paddleX - 0.5 * paddleThickness - ballRadius
        if (vx > 0 && before.x <= face && x > face) {
            if (Math.abs(y - paddle) <= 0.5 * paddleLength + ballRadius) {
                // Off center sends it at an angle, a little faster each time
                const speed = Math.min(maxSpeed, Math.hypot(vx, vy) * 1.05)
                const angle = Math.max(-1.0, Math.min(1.0, (y - paddle) / (0.5 * paddleLength))) * 0.9
                vx = -Math.cos(angle) * speed
                vy = Math.sin(angle) * speed
                x = 2 * face - x
                streak += 1
                best = Math.max(best, streak)
                flash = 1.0
                hit(field.mapPositionToScene(Qt.vector3d(mirror * face, y, 0.3)))
            }
        }
        ball = Qt.vector2d(x, y)
        velocity = Qt.vector2d(vx, vy)

        if (x > 0.5 * depth + 0.6) {
            streak = 0
            waiting = 0.6
            missed()
        }
    }

    // Shown from the network: on to the last state, a jump if it's far
    function follow(dt) {
        if (!target)
            return
        const k = Math.min(1.0, 15.0 * dt)
        paddle += (target[0] - paddle) * k
        const to = Qt.vector2d(target[1], target[2])
        ball = to.minus(ball).length() > 2.0 ? to : ball.plus(to.minus(ball).times(k))
    }

    // The texts stay upright, the field turns. Mirrored by its x, a turn
    // would show the flat parts from behind.
    Node {
        id: field
        eulerRotation.z: lane.orientation === PracticeLane.Bottom ? -90 : 0

        Model {
            source: "#Rectangle"
            z: -0.2
            scale: Qt.vector3d(lane.depth / 100, lane.span / 100, 1)
            materials: DefaultMaterial {
                diffuseColor: Qt.tint(Theme.background, Qt.rgba(lane.color.r, lane.color.g, lane.color.b, 0.08))
                opacity: 0.8
                lighting: DefaultMaterial.NoLighting
            }
        }

        // The far wall and the long sides, open behind the paddle
        Repeater3D {
            model: [Qt.vector4d(0, 0.5 * lane.span - 0.5 * lane.wallThickness, lane.depth, lane.wallThickness),
                    Qt.vector4d(0, -0.5 * lane.span + 0.5 * lane.wallThickness, lane.depth, lane.wallThickness),
                    Qt.vector4d(-0.5 * lane.depth + 0.5 * lane.wallThickness, 0, lane.wallThickness, lane.span)]

            delegate: Model {
                required property vector4d modelData
                source: "#Cube"
                position: Qt.vector3d(lane.mirror * modelData.x, modelData.y, 0)
                scale: Qt.vector3d(modelData.z / 100, modelData.w / 100, 0.001)
                materials: PhongMaterial {
                    color: Qt.tint(Theme.wall, Qt.rgba(lane.color.r, lane.color.g, lane.color.b, 0.4))
                    glow: 0.25
                }
            }
        }

        Model {
            source: "#Cube"
            position: Qt.vector3d(lane.mirror * lane.paddleX, lane.paddle, 0.1)
            scale: Qt.vector3d(lane.paddleThickness / 100, lane.paddleLength / 100, 0.003)
            materials: PhongMaterial {
                color: Qt.tint(lane.color, Qt.rgba(1, 1, 1, 0.5 * lane.flash))
                glow: 0.4 + 0.8 * lane.flash
                shininess: 0.6
            }
        }

        Disc {
            visible: lane.local ? lane.waiting <= 0 : lane.target !== null
            position: Qt.vector3d(lane.mirror * lane.ball.x, lane.ball.y, 0.2)
            radius: lane.ballRadius
            sphere: true
            color: Theme.ball
            glow: 0.6
        }
    }

    // The hits in a row, large behind the play, and the best so far
    Text3D {
        visible: lane.streak > 0
        y: -0.9
        z: -0.1
        scale: Qt.vector3d(1.6, 1.6, 1.6)
        horizontalAlignment: Text.AlignHCenter
        color: Qt.rgba(lane.color.r, lane.color.g, lane.color.b, 0.35)
        text: lane.streak
    }

    Text3D {
        visible: lane.best > 0
        y: 0.5 * lane.screenHeight - 0.75
        scale: Qt.vector3d(0.4, 0.4, 0.4)
        horizontalAlignment: Text.AlignHCenter
        color: Theme.dimmed
        text: qsTr("Best %1").arg(lane.best)
    }
}
