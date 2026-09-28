pragma ComponentBehavior: Bound

import QtQuick
import QtQuick3D
import QtQuick3D.Helpers
import QtQuick3D.Physics
import Phong

// Three to six players on a regular polygon, you at the bottom against
// the computer. The middle of each side is a goal, posts keep the goals
// apart. A player out of balls gets a wall instead of the goal, the last
// one left wins.
Scene {
    id: root

    property int players: 3
    property Scene menuScene
    // Who plays each side: { kind: "keyboard" }, { kind: "pad", pad } or
    // { kind: "cpu" }. Sides without one are played by the computer.
    property var controllers: [{ kind: "keyboard" }]

    function controller(side) {
        return controllers[side] ?? { kind: "cpu" }
    }

    function playerName(side) {
        const controller = root.controller(side)
        if (controller.kind === "keyboard")
            return qsTr("You")
        if (controller.kind === "pad")
            return qsTr("Pad %1").arg(controllers.slice(0, side + 1).filter((c) => c.kind === "pad").length)
        return qsTr("CPU %1").arg(side)
    }

    // Gamepads work the pause menu and the end, not the game
    menuNavigation: match.state === PartyMatch.Paused || place > 0

    Connections {
        target: Gamepads
        enabled: root.active
        function onButtonPressed(pad, button) {
            if (button === Gamepad.Start && root.place === 0) {
                if (match.state === PartyMatch.Paused)
                    match.resume()
                else if (root.running)
                    match.pause()
            }
        }
    }

    // The inner polygon, the posts take the ends of every side
    readonly property real radius: 11.5
    readonly property real apothem: radius * Math.cos(Math.PI / players)
    readonly property real sideLength: 2 * radius * Math.sin(Math.PI / players)
    readonly property real postLength: 0.22 * sideLength
    readonly property real goalWidth: sideLength - 2 * postLength
    readonly property real wallThickness: 1.0
    readonly property real ballRadius: 0.7
    readonly property real paddleLength: 3.2
    readonly property real paddleWidth: 0.8
    readonly property real paddleSpeed: 22.0
    readonly property real paddleDistance: apothem - 1.4
    // A paddle may reach a little in front of the posts
    readonly property real paddleLimit: 0.5 * (goalWidth - paddleLength) + 0.4
    // Names and balls left sit this far out from the middle
    readonly property real labelDistance: apothem + wallThickness + 1.2

    readonly property var colors: [Theme.leftPlayer, Theme.rightPlayer, Theme.tint("#ffd24d"),
                                   Theme.tint("#66ff66"), Theme.tint("#ff8c1a"), Theme.tint("#a64dff")]
    readonly property bool running: match.state === PartyMatch.Serving || match.state === PartyMatch.Playing

    // Out of the game: the place, 1 is the winner, 0 while still in
    property int place: 0

    property bool leftKey: false
    property bool rightKey: false
    property real pointerX: NaN
    property var pendingGoals: []
    property int currentPauseItem: 0

    function sideAngle(player) {
        return -0.5 * Math.PI + player * 2.0 * Math.PI / players
    }

    function normal(player) {
        const angle = sideAngle(player)
        return Qt.vector2d(Math.cos(angle), Math.sin(angle))
    }

    function tangent(player) {
        const n = normal(player)
        return Qt.vector2d(-n.y, n.x)
    }

    // A point given by distance along the normal and along the tangent
    function sidePoint(player, distance, along) {
        const n = normal(player)
        const t = tangent(player)
        return Qt.vector3d(n.x * distance + t.x * along, n.y * distance + t.y * along, 0)
    }

    // The outer corners, and the camera looks at the middle of them and
    // the labels
    function corner(index, distance) {
        const angle = sideAngle(index) + Math.PI / players
        const r = distance / Math.cos(Math.PI / players)
        return Qt.vector3d(r * Math.cos(angle), r * Math.sin(angle), 0)
    }

    // Wide windows list the players beside the polygon, that lets the
    // camera come closer. Narrow ones have the names at the sides.
    readonly property bool sideLayout: (phong?.aspectRatio ?? 1.0) >= 1.45
    readonly property real extentX: {
        let extent = 0
        for (let i = 0; i < players; ++i)
            extent = Math.max(extent, Math.abs(corner(i, apothem + wallThickness).x))
        return extent
    }
    readonly property real extentY: {
        let low = Infinity
        let high = -Infinity
        for (let i = 0; i < players; ++i) {
            const y = corner(i, apothem + wallThickness).y
            low = Math.min(low, y)
            high = Math.max(high, y)
        }
        return 0.5 * (high - low)
    }
    contentHalfWidth: sideLayout ? extentX + 8.0 : Math.max(extentX + 3.5, 18.5)
    contentHalfHeight: sideLayout ? extentY + 0.6 : 13.0

    readonly property real middleY: {
        let low = Infinity
        let high = -Infinity
        for (let i = 0; i < players; ++i) {
            const points = [corner(i, apothem + wallThickness).y]
            if (!sideLayout)
                points.push(sidePoint(i, labelDistance, 0).y)
            for (const y of points) {
                low = Math.min(low, y)
                high = Math.max(high, y)
            }
        }
        return 0.5 * (low + high)
    }
    viewOffset: Qt.vector3d(0, middleY, 0)

    function start() {
        place = 0
        releaseInput()
        for (let i = 0; i < sides.count; ++i)
            sides.objectAt(i)?.reset()
        resetBall()
        match.start()
    }

    function leave() {
        match.stop()
        releaseInput()
        phong.returnTo(root.menuScene)
    }

    function releaseInput() {
        leftKey = rightKey = false
        pointerX = NaN
    }

    function resetBall() {
        ball.reset(Qt.vector3d(0, 0, 0), Qt.vector3d(0, 0, 0))
        ball.setLinearVelocity(Qt.vector3d(0, 0, 0))
        ball.clearTrail()
    }

    function queueGoal(player) {
        pendingGoals.push(player)
        Qt.callLater(scorePending)
    }

    function scorePending() {
        const goals = pendingGoals
        pendingGoals = []
        for (const player of goals)
            match.goal(player)
    }

    function contact(other, normals) {
        if (other.paddleOf !== undefined) {
            const side = sides.objectAt(other.paddleOf)
            const along = Qt.vector2d(ball.x, ball.y).dotProduct(tangent(other.paddleOf))
            match.paddleHit(other.paddleOf, (along - side.offset) / (0.5 * paddleLength + ballRadius))
        }
        else if (other.wall === true && normals.length > 0) {
            // Pointing to the ball, whichever way the report has it
            let n = Qt.vector2d(normals[0].x, normals[0].y)
            if (n.dotProduct(Qt.vector2d(ball.x - other.x, ball.y - other.y)) < 0)
                n = n.times(-1)
            if (match.bounce(n))
                SoundEffects.play(SoundEffects.WallHit)
        }
    }

    function step(dt) {
        match.advance(dt)
        if (!running)
            return

        const position = Qt.vector2d(ball.x, ball.y)
        const velocity = match.ball.velocity
        for (let i = 0; i < sides.count; ++i) {
            const side = sides.objectAt(i)
            if (!side || !match.isAlive(i))
                continue

            let input = 0
            const controller = root.controller(i)
            if (controller.kind === "keyboard") {
                if (leftKey || rightKey)
                    input = (rightKey ? 1 : 0) - (leftKey ? 1 : 0)
                else if (!isNaN(pointerX))
                    input = Math.max(-1, Math.min(1, (pointerX - side.offset) / 0.5))
            }
            else if (controller.kind === "pad") {
                // Pushed along the side, whichever way it runs on screen
                if (controller.pad)
                    input = Math.max(-1, Math.min(1, controller.pad.direction.dotProduct(tangent(i)) * 1.4))
            }
            else {
                // The computer plays in the frame of its side
                const n = normal(i)
                const t = tangent(i)
                side.computer.update(dt, Qt.vector2d(position.dotProduct(n), position.dotProduct(t)),
                                     Qt.vector2d(velocity.dotProduct(n), velocity.dotProduct(t)), side.offset)
                input = side.computer.direction
            }
            side.offset = Math.max(-paddleLimit, Math.min(paddleLimit, side.offset + input * paddleSpeed * dt))
        }

        ball.advance(dt)
    }

    onActiveChanged: {
        if (!active) {
            releaseInput()
            match.stop()
        }
    }

    onFocusLost: {
        releaseInput()
        match.pause()
    }

    onKeyPressed: (event) => {
        event.accepted = true
        if (event.isAutoRepeat)
            return

        if (place > 0) {
            if (event.key === Qt.Key_Escape) {
                SoundEffects.play(SoundEffects.MenuSelect)
                leave()
            }
            else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter || event.key === Qt.Key_Space) {
                SoundEffects.play(SoundEffects.MenuSelect)
                start()
            }
            return
        }

        if (match.state === PartyMatch.Paused) {
            switch (event.key) {
                case Qt.Key_Escape:
                case Qt.Key_P:
                    SoundEffects.play(SoundEffects.MenuSelect)
                    match.resume()
                    return
                case Qt.Key_Up:
                case Qt.Key_Down:
                    currentPauseItem = event.key === Qt.Key_Up ? 0 : 1
                    SoundEffects.play(SoundEffects.MenuMove)
                    return
                case Qt.Key_Return:
                case Qt.Key_Enter:
                    SoundEffects.play(SoundEffects.MenuSelect)
                    if (currentPauseItem === 0)
                        match.resume()
                    else
                        leave()
                    return
            }
            return
        }

        switch (event.key) {
            case Qt.Key_Left:
            case Qt.Key_A:
                leftKey = true
                break
            case Qt.Key_Right:
            case Qt.Key_D:
                rightKey = true
                break
            case Qt.Key_Escape:
            case Qt.Key_P:
                currentPauseItem = 0
                match.pause()
                break
        }
    }

    onKeyReleased: (event) => {
        event.accepted = true
        if (event.isAutoRepeat)
            return
        if (event.key === Qt.Key_Left || event.key === Qt.Key_A)
            leftKey = false
        else if (event.key === Qt.Key_Right || event.key === Qt.Key_D)
            rightKey = false
    }

    // Your side is the bottom one, a pointer steers along it
    onPointerPressed: (id, x, y) => {
        const clickable = phong.clickableAt(x, y)
        if (clickable) {
            clickable.clicked()
            return
        }
        pointerX = phong.toScene(x, y, root).x
    }
    onPointerMoved: (id, x, y) => pointerX = phong.toScene(x, y, root).x
    onPointerReleased: (id) => pointerX = NaN

    PartyMatch {
        id: match

        players: root.players
        lives: 3
        serveSpeed: GameSettings.serveSpeed * 0.9
        maxSpeed: GameSettings.maxSpeed * 0.9
        serveDelay: GameSettings.kickoffTime

        onServed: SoundEffects.play(SoundEffects.Serve)
        onPaddleHitBall: (player) => {
            const side = sides.objectAt(player)
            side?.paddle.flash()
            SoundEffects.play(SoundEffects.PaddleHit, 1.0 + 0.04 * Math.min(match.rally, 20))
            sparks.burst(Qt.vector3d(ball.x, ball.y, 0.5), Theme.ball, 10)
        }
        onGoalScored: (player) => {
            SoundEffects.play(SoundEffects.Goal)
            sparks.burst(Qt.vector3d(ball.x, ball.y, 0.5), root.colors[player], 50)
            root.resetBall()
            for (let i = 0; i < sides.count; ++i)
                sides.objectAt(i)?.computer.reset()
        }
        onPlayerOut: (player) => {
            banner.show(qsTr("%1 out").arg(sides.objectAt(player)?.name ?? ""), root.colors[player])
            // The last human out ends it, the place is the players left
            // and them
            const humans = root.controllers.filter((c, side) => c.kind !== "cpu" && match.isAlive(side))
            if (root.controller(player).kind !== "cpu" && humans.length === 0) {
                root.place = match.alive + 1
                SoundEffects.play(SoundEffects.Lose)
                match.stop()
            }
        }
        onFinished: {
            if (root.controller(match.winner).kind !== "cpu") {
                root.place = 1
                SoundEffects.play(SoundEffects.Win)
            }
        }
    }

    Binding {
        target: SoundEffects
        property: "musicPlaying"
        when: root.active
        value: root.running
    }

    Binding {
        target: SoundEffects
        property: "musicIntensity"
        when: root.active
        value: match.state === PartyMatch.Playing ? 1 + Math.min(3, Math.floor(match.rally / 4)) : 0
    }

    Binding {
        target: SoundEffects
        property: "musicTempo"
        when: root.active
        value: 112 + Math.min(40, 1.5 * match.rally)
    }

    PhysicsWorld {
        scene: root
        running: root.active && root.running
        gravity: Qt.vector3d(0, 0, 0)
        enableCCD: true
        numThreads: 0
        typicalLength: 1.0
        typicalSpeed: 20.0

        onFrameDone: (timeStep) => root.step(timeStep / 1000.0)
    }

    PhysicsMaterial {
        id: bouncy
        restitution: 1.0
        staticFriction: 0.0
        dynamicFriction: 0.0
    }

    // The floor, a polygon with lines to the corners
    Model {
        visible: GraphicsSettings.floor
        z: -0.7
        geometry: ProceduralMesh {
            positions: {
                const positions = [Qt.vector3d(0, 0, 0)]
                for (let i = 0; i <= root.players; ++i)
                    positions.push(root.corner(i % root.players, root.apothem + root.wallThickness))
                return positions
            }
            normals: {
                const normals = []
                for (let i = 0; i <= root.players + 1; ++i)
                    normals.push(Qt.vector3d(0, 0, 1))
                return normals
            }
            indexes: {
                const indexes = []
                for (let i = 1; i <= root.players; ++i)
                    indexes.push(0, i, i + 1)
                return indexes
            }
        }
        materials: PhongMaterial {
            color: Theme.floor
            shininess: 0.15
        }
    }

    Repeater3D {
        model: GraphicsSettings.floor ? root.players : 0

        delegate: Model {
            required property int index
            readonly property vector3d end: root.corner(index, root.apothem)

            position: Qt.vector3d(0.5 * end.x, 0.5 * end.y, -0.65)
            eulerRotation.z: Math.atan2(end.y, end.x) * 180 / Math.PI
            scale: Qt.vector3d(end.length() / 100, 0.08 / 100, 0.001)
            source: "#Cube"
            materials: PhongMaterial {
                color: Theme.grid
                glow: 0.6
                lighting: DefaultMaterial.NoLighting
            }
        }
    }

    // A side: two posts, the goal and its trigger, a paddle, the name and
    // the balls left
    Repeater3D {
        id: sides
        model: root.players

        delegate: Node {
            id: side

            required property int index
            readonly property color color: root.colors[index]
            readonly property real angle: root.sideAngle(index) * 180 / Math.PI
            readonly property bool alive: (match.livesLeft[index] ?? 1) > 0
            readonly property string name: root.playerName(index)
            property real offset: 0.0
            property alias paddle: paddle
            property alias computer: computer

            function reset() {
                offset = 0.0
                computer.reset()
            }

            ComputerPlayer {
                id: computer
                difficulty: GameSettings.difficulty
                paddleSpeed: root.paddleSpeed
                paddleX: root.paddleDistance - 0.5 * root.paddleWidth - root.ballRadius
                paddleReach: 0.5 * root.paddleLength + root.ballRadius
                fieldTop: 100
                fieldBottom: -100
            }

            component Wall: DynamicRigidBody {
                id: wall
                property int player: 0
                property real angle: 0.0
                property real along: 0.0
                property real length: 1.0
                property bool parked: false
                readonly property bool wall: true
                property color color: Theme.wall
                property real glow: 0.35

                isKinematic: true
                position: kinematicPosition
                kinematicPosition: parked ? Qt.vector3d(1000, 1000 + 10 * player, 0)
                                          : root.sidePoint(player, root.apothem + 0.5 * root.wallThickness, along)
                kinematicEulerRotation: Qt.vector3d(0, 0, angle + 90)
                physicsMaterial: bouncy
                sendContactReports: true
                collisionShapes: BoxShape {
                    extents: Qt.vector3d(wall.length, root.wallThickness, 1.0)
                }

                Model {
                    visible: !wall.parked
                    source: "#Cube"
                    scale: Qt.vector3d(wall.length / 100, root.wallThickness / 100, 0.01)
                    materials: PhongMaterial {
                        color: wall.color
                        glow: wall.glow
                        shininess: 0.5
                    }
                }
            }

            // The posts reach past the corner to close it with the next side
            Wall {
                player: side.index
                angle: side.angle
                along: -(0.5 * root.sideLength - 0.5 * root.postLength) - 0.4
                length: root.postLength + 0.8
            }

            Wall {
                player: side.index
                angle: side.angle
                along: 0.5 * root.sideLength - 0.5 * root.postLength + 0.4
                length: root.postLength + 0.8
            }

            // A player out of balls gets a wall
            Wall {
                player: side.index
                angle: side.angle
                parked: side.alive
                length: root.goalWidth + 0.2
                color: Qt.tint(Theme.wall, Qt.rgba(side.color.r, side.color.g, side.color.b, 0.3))
                glow: 0.2
            }

            // The goal line glows in the player's color
            Model {
                visible: side.alive
                position: root.sidePoint(side.index, root.apothem + 0.2, 0).plus(Qt.vector3d(0, 0, -0.45))
                eulerRotation.z: side.angle + 90
                source: "#Cube"
                scale: Qt.vector3d(root.goalWidth / 100, 0.15 / 100, 0.001)
                materials: PhongMaterial {
                    color: side.color
                    glow: 0.8
                    lighting: DefaultMaterial.NoLighting
                }
            }

            TriggerBody {
                position: root.sidePoint(side.index, root.apothem + root.ballRadius + 2.0, 0)
                eulerRotation.z: side.angle + 90
                collisionShapes: BoxShape {
                    extents: Qt.vector3d(root.goalWidth, 4.0, 2.0)
                }
                onBodyEntered: (body) => {
                    if (body === ball && side.alive)
                        root.queueGoal(side.index)
                }
            }

            PaddleBody {
                id: paddle
                readonly property int paddleOf: side.index
                visible: side.alive
                color: side.color
                // Parked far away when out
                paddleX: side.alive ? root.sidePoint(side.index, root.paddleDistance, side.offset).x : 1000 + 10 * side.index
                paddleY: side.alive ? root.sidePoint(side.index, root.paddleDistance, side.offset).y : 1000
                angle: side.angle
                length: root.paddleLength
                width: root.paddleWidth
            }

            // Name and balls left, outside the side
            Node {
                visible: !root.sideLayout
                position: root.sidePoint(side.index, root.labelDistance, 0)

                Text3D {
                    y: -0.3
                    x: -0.2
                    scale: Qt.vector3d(0.6, 0.6, 0.6)
                    horizontalAlignment: Text.AlignRight
                    verticalAlignment: Text.AlignVCenter
                    color: side.alive ? side.color : Theme.dimmed
                    glow: side.alive ? 0.5 : 0.0
                    text: side.name
                }

                Repeater3D {
                    model: match.lives

                    delegate: Disc {
                        required property int index
                        x: 0.5 + index * 0.7
                        radius: 0.24
                        sphere: true
                        color: index < (match.livesLeft[side.index] ?? 0) ? side.color : Theme.goal
                        glow: 0.4
                    }
                }
            }
        }
    }

    BallBody {
        id: ball
        ball: match.ball
        radius: root.ballRadius
        trailSpeed: match.serveSpeed
        onContact: (body, normals) => root.contact(body, normals)
    }

    Sparks {
        id: sparks
    }

    // Kickoff: the seconds and where the ball goes
    Node {
        visible: match.state === PartyMatch.Serving
        z: 0.6

        Node {
            eulerRotation.z: Math.atan2(match.serveDirection.y, match.serveDirection.x) * 180 / Math.PI

            Repeater3D {
                model: [
                    { x: 2.0, y: 0.0, length: 1.3, angle: 0 },
                    { x: 2.37, y: 0.32, length: 1.0, angle: 140 },
                    { x: 2.37, y: -0.32, length: 1.0, angle: -140 }
                ]

                delegate: Model {
                    required property var modelData
                    position: Qt.vector3d(modelData.x + 0.75, modelData.y, 0)
                    eulerRotation.z: modelData.angle
                    scale: Qt.vector3d(modelData.length / 100, 0.0024, 0.003)
                    source: "#Cube"
                    materials: PhongMaterial {
                        color: Theme.title
                        glow: 1.0
                    }
                }
            }
        }

        Text3D {
            y: -3.2
            scale: Qt.vector3d(1.4, 1.4, 1.4)
            horizontalAlignment: Text.AlignHCenter
            color: Theme.title
            text: Math.max(1, Math.ceil(match.serveCountdown))
        }
    }

    Banner {
        id: banner
        y: 3.0
        z: 1.5
    }

    // The players beside the polygon on wide windows
    Node {
        visible: root.sideLayout
        x: -root.extentX - 7.5
        y: root.middleY + root.extentY - 1.0

        Repeater3D {
            model: root.players

            delegate: Node {
                id: entry

                required property int index
                readonly property bool alive: (match.livesLeft[index] ?? 1) > 0
                readonly property color color: root.colors[index]

                y: -index * 1.6

                Text3D {
                    scale: Qt.vector3d(0.6, 0.6, 0.6)
                    verticalAlignment: Text.AlignVCenter
                    color: entry.alive ? entry.color : Theme.dimmed
                    glow: entry.alive ? 0.5 : 0.0
                    text: root.playerName(entry.index)
                }

                Repeater3D {
                    model: match.lives

                    delegate: Disc {
                        required property int index
                        x: 4.0 + index * 0.7
                        radius: 0.24
                        sphere: true
                        color: index < (match.livesLeft[entry.index] ?? 0) ? entry.color : Theme.goal
                        glow: 0.4
                    }
                }
            }
        }
    }

    // In a corner, the polygons leave it free
    Node {
        x: root.sideLayout ? -root.extentX - 7.5 : -18.5
        y: root.middleY - (root.sideLayout ? root.extentY - 0.8 : 11.5)
        scale: Qt.vector3d(0.5, 0.5, 0.5)

        Repeater3D {
            model: [qsTr("%1 players, a test").arg(root.players), qsTr("[Left/Right] move"), qsTr("[Esc] pause")]

            delegate: Text3D {
                required property string modelData
                required property int index
                y: (2 - index) * 1.6
                color: Theme.dimmed
                text: modelData
            }
        }
    }

    // Pause and the end, over the field
    Node {
        id: overlay
        visible: match.state === PartyMatch.Paused || root.place > 0
        y: root.middleY
        z: 1.5

        Model {
            source: "#Rectangle"
            scale: Qt.vector3d(0.38, 0.28, 1)
            materials: DefaultMaterial {
                diffuseColor: Theme.background
                opacity: 0.75
                lighting: DefaultMaterial.NoLighting
            }
        }

        Text3D {
            y: 2.0
            scale: Qt.vector3d(2, 2, 2)
            horizontalAlignment: Text.AlignHCenter
            color: Theme.title
            glow: 0.8
            text: root.place === 1 ? (root.controller(match.winner).kind === "keyboard" ? qsTr("You win")
                                                                         : qsTr("%1 wins").arg(root.playerName(match.winner)))
                  : root.place > 1 ? qsTr("Place %1 of %2").arg(root.place).arg(root.players)
                  : qsTr("Paused")
        }

        Repeater3D {
            model: root.place > 0 ? [qsTr("Again"), qsTr("Menu")] : [qsTr("Resume"), qsTr("Menu")]

            delegate: Text3D {
                id: item

                required property string modelData
                required property int index

                y: -1.5 - index * 2.0
                horizontalAlignment: Text.AlignHCenter
                text: modelData
                clickable: overlay.visible
                onClicked: {
                    SoundEffects.play(SoundEffects.MenuSelect)
                    if (index === 1)
                        root.leave()
                    else if (root.place > 0)
                        root.start()
                    else
                        match.resume()
                }

                Disc {
                    visible: root.place === 0 && item.index === root.currentPauseItem
                    position: Qt.vector3d(-0.5 * item.textWidth - 1.0, 0.35, 0)
                    radius: 0.35
                    sphere: true
                }
            }
        }

        Text3D {
            visible: root.place > 0
            y: -6.0
            scale: Qt.vector3d(0.5, 0.5, 0.5)
            horizontalAlignment: Text.AlignHCenter
            color: Theme.dimmed
            text: qsTr("[Enter] again   [Esc] menu")
        }
    }
}
