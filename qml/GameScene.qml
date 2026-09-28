pragma ComponentBehavior: Bound

import QtQuick
import QtQuick3D
import QtQuick3D.Helpers
import QtQuick3D.Physics
import Phong

Scene {
    id: root

    enum Mode {
        OnePlayer,
        TwoPlayers,
        Ladder
    }

    property int mode: GameScene.OnePlayer
    // Ladder level, the computer's difficulty
    property int ladderStage: 0
    property alias match: match

    readonly property bool againstComputer: mode !== GameScene.TwoPlayers
    readonly property int difficulty: mode === GameScene.Ladder ? ladderStage : GameSettings.difficulty

    // Playing field, the ball stays in the plane z = 0
    readonly property real stageWidth: 34.0
    readonly property real stageHeight: 20.0
    readonly property real goalDepth: 4.0
    readonly property real wallThickness: 1.0
    readonly property real ballRadius: 0.8
    readonly property real paddleWidth: 1.0
    readonly property real paddleSpeed: 24.0
    readonly property real paddleX: 0.5 * stageWidth - goalDepth
    readonly property real uprightSpeed: 150.0 // degrees per second, after a spin

    // Shields sit a bit behind the paddles, the goal line behind them
    readonly property real shieldWidth: 0.4
    readonly property real shieldX: paddleX + 1.6
    readonly property real goalLine: shieldX + 0.5 * shieldWidth + ballRadius

    // The walls move in while the field is narrowed
    property real fieldInset: modifiers.fieldInset
    Behavior on fieldInset {
        NumberAnimation { duration: 700; easing.type: Easing.InOutQuad }
    }
    readonly property real wallY: 0.5 * stageHeight - fieldInset
    readonly property real innerHeight: 2.0 * wallY - wallThickness
    readonly property real ballLimit: 0.5 * innerHeight - ballRadius

    property real leftPaddleLength: GameSettings.paddleLength * match.left.paddleScale
    Behavior on leftPaddleLength {
        NumberAnimation { duration: 300; easing.type: Easing.OutQuad }
    }
    property real rightPaddleLength: GameSettings.paddleLength * match.right.paddleScale
    Behavior on rightPaddleLength {
        NumberAnimation { duration: 300; easing.type: Easing.OutQuad }
    }

    property real leftPaddleY: 0.0
    property real rightPaddleY: 0.0
    property real leftPaddleAngle: 0.0
    property real rightPaddleAngle: 0.0
    // Paddle speed of the last step, puts spin on the ball
    property real leftPaddleVelocity: 0.0
    property real rightPaddleVelocity: 0.0

    // Held keys
    property bool leftUp: false
    property bool leftDown: false
    property bool rightUp: false
    property bool rightDown: false

    // Paddle targets of mouse or touch points, NaN when there is none
    property real leftPointerY: NaN
    property real rightPointerY: NaN
    property var pointerSides: ({})

    // Items balls flew through and goals, handled after the physics step
    property var pendingItems: []
    property var pendingGoals: []

    // Definitions of the active effects, for the icons next to the names
    property var leftEffects: []
    property var rightEffects: []

    // Obstacles of the current arena
    property string arenaId: "classic"
    property var arena: ({})

    // Camera shake, applied through viewOffset
    property real shakeAmount: 0.0

    // Last whole second of the kickoff countdown, ticks when it changes
    property int kickoffSecond: 0

    readonly property bool running: match.state === Match.Serving || match.state === Match.Playing

    property int currentPauseItem: 0
    readonly property var pauseItems: [
        { text: qsTr("Resume"), activate: () => match.resume() },
        { text: qsTr("Menu"), activate: () => root.leave() }
    ]

    function startMatch() {
        leftPaddleY = rightPaddleY = 0.0
        leftPaddleAngle = rightPaddleAngle = 0.0
        releaseInput()
        chooseArena()
        resetBall()
        computer.reset()
        modifiers.reset()
        match.start()
        banner.show(arena.name ?? "", Theme.title)
    }

    function chooseArena() {
        let id = GameSettings.arena
        if (id === "random" || Arenas.arena(id).id === undefined)
            id = Arenas.randomId()
        arenaId = id
        arena = Arenas.arena(id)
        modifiers.obstacles = Arenas.obstacleRects(id)
    }

    function leave() {
        match.stop()
        releaseInput()
        phong.previousScene()
    }

    function togglePause() {
        if (match.state === Match.Paused)
            match.resume()
        else
            match.pause()
    }

    function releaseInput() {
        leftUp = leftDown = rightUp = rightDown = false
        leftPointerY = rightPointerY = NaN
        pointerSides = {}
    }

    function resetBall() {
        mainBall.reset(Qt.vector3d(0, 0, 0), Qt.vector3d(0, 0, 0))
        mainBall.setLinearVelocity(Qt.vector3d(0, 0, 0))
        mainBall.clearTrail()
    }

    function shake(amount) {
        shakeAmount = Math.max(shakeAmount, amount)
    }

    function humanInput(up, down, pointerY, paddleY) {
        if (up || down)
            return (up ? 1 : 0) - (down ? 1 : 0)
        if (!isNaN(pointerY))
            return Math.max(-1, Math.min(1, (pointerY - paddleY) / 0.5))
        return 0
    }

    function movePaddle(y, input, length, dt) {
        const limit = 0.5 * (innerHeight - length)
        return Math.max(-limit, Math.min(limit, y + input * paddleSpeed * dt))
    }

    // Spins while cursed and then finishes the turn until upright again
    function turnPaddle(angle, spinSpeed, dt) {
        if (spinSpeed > 0.0)
            return (angle + spinSpeed * dt) % 360
        if (angle % 180 === 0)
            return angle

        const next = angle + uprightSpeed * dt
        const upright = Math.floor(next / 180)
        return upright > Math.floor(angle / 180) ? (upright * 180) % 360 : next
    }

    function ballBodies() {
        const bodies = [mainBall]
        for (let i = 0; i < extraBalls.count; ++i) {
            const body = extraBalls.objectAt(i)
            if (body)
                bodies.push(body)
        }
        return bodies
    }

    // The ball that reaches the computer's paddle first, otherwise the main one
    function urgentBall() {
        let urgent = mainBall
        let soonest = Infinity
        for (const body of ballBodies()) {
            const vx = body.ball.velocity.x
            if (vx <= 0.0)
                continue
            const time = (paddleX - body.x) / vx
            if (time >= 0.0 && time < soonest) {
                soonest = time
                urgent = body
            }
        }
        return urgent
    }

    // Upright paddles use the arcade bounce, turned ones reflect the ball
    // off their surface
    function paddleContact(body, side, paddle, angle, length, velocity, normals) {
        if (angle % 180 === 0 || normals.length === 0) {
            match.paddleHit(body.ball, side, (body.y - paddle.y) / (0.5 * length + ballRadius), velocity)
            return
        }

        // The contact normal points from the paddle to the ball
        let normal = Qt.vector2d(normals[0].x, normals[0].y)
        if (normal.dotProduct(Qt.vector2d(body.x - paddle.x, body.y - paddle.y)) < 0)
            normal = normal.times(-1)
        match.deflect(body.ball, side, normal)
    }

    function ballContact(body, other, normals) {
        const normal = normals.length ? Qt.vector2d(normals[0].x, normals[0].y) : Qt.vector2d(0, 0)

        if (other === leftPaddle) {
            paddleContact(body, Match.LeftSide, leftPaddle, leftPaddleAngle, leftPaddleLength,
                          leftPaddleVelocity, normals)
        }
        else if (other === rightPaddle) {
            paddleContact(body, Match.RightSide, rightPaddle, rightPaddleAngle, rightPaddleLength,
                          rightPaddleVelocity, normals)
        }
        else if (other === topWall || other === bottomWall) {
            const before = body.ball.velocity.y
            match.wallHit(body.ball, other === topWall)
            if (body.ball.velocity.y !== before)
                SoundEffects.play(SoundEffects.WallHit)
        }
        else if (other === leftShield || other === rightShield) {
            if (modifiers.shieldHit(body.ball, other === leftShield ? Match.LeftSide : Match.RightSide)) {
                SoundEffects.play(SoundEffects.ShieldHit)
                sparks.burst(Qt.vector3d(body.x, body.y, 0.5), Theme.shield, 30)
                shake(0.35)
            }
        }
        else if (other.obstacle === true) {
            if (match.bounce(body.ball, normal)) {
                other.flash()
                SoundEffects.play(SoundEffects.Bounce)
                sparks.burst(Qt.vector3d(body.x, body.y, 0.5), Theme.bumper, 14)
                shake(0.12)
            }
        }
    }

    // Removing an item destroys its trigger body, not while it reports
    function queueCollect(itemId, ball) {
        pendingItems.push({ itemId: itemId, ball: ball })
        Qt.callLater(collectPending)
    }

    // A goal can remove the ball's body, not while the triggers report
    function queueGoal(ball, scorer) {
        pendingGoals.push({ ball: ball, scorer: scorer })
        Qt.callLater(scorePending)
    }

    function scorePending() {
        const goals = pendingGoals
        pendingGoals = []
        for (const goal of goals)
            match.goal(goal.ball, goal.scorer)
    }

    function collectPending() {
        const items = pendingItems
        pendingItems = []
        for (const item of items)
            modifiers.collect(item.itemId, item.ball)
    }

    // Called after every physics step
    function step(dt) {
        match.advance(dt)
        if (!running)
            return

        modifiers.advance(dt)

        const onePlayer = mode !== GameScene.TwoPlayers
        const leftInput = onePlayer
                ? humanInput(leftUp || rightUp, leftDown || rightDown, leftPointerY, leftPaddleY)
                : humanInput(leftUp, leftDown, leftPointerY, leftPaddleY)

        let rightInput
        if (onePlayer) {
            const urgent = urgentBall()
            computer.targets = modifiers.itemPositions()
            computer.opponentY = leftPaddleY
            computer.update(dt, Qt.vector2d(urgent.x, urgent.y), urgent.ball.velocity, rightPaddleY)
            rightInput = computer.direction
        }
        else {
            rightInput = humanInput(rightUp, rightDown, rightPointerY, rightPaddleY)
        }

        const leftY = movePaddle(leftPaddleY, leftInput, leftPaddleLength, dt)
        const rightY = movePaddle(rightPaddleY, rightInput, rightPaddleLength, dt)
        leftPaddleVelocity = (leftY - leftPaddleY) / dt
        rightPaddleVelocity = (rightY - rightPaddleY) / dt
        leftPaddleY = leftY
        rightPaddleY = rightY
        leftPaddleAngle = turnPaddle(leftPaddleAngle, match.left.spinSpeed, dt)
        rightPaddleAngle = turnPaddle(rightPaddleAngle, match.right.spinSpeed, dt)

        for (const body of ballBodies())
            body.advance(dt)

        // The camera shakes on impacts and leans a little towards the ball
        shakeAmount *= Math.exp(-7.0 * dt)
        viewOffset = Qt.vector3d((Math.random() - 0.5) * shakeAmount,
                                 (Math.random() - 0.5) * shakeAmount, 0)
        const follow = Math.min(1.0, 3.0 * dt)
        viewRotation = viewRotation.plus(Qt.vector3d(0.07 * mainBall.y, -0.05 * mainBall.x, 0)
                                         .minus(viewRotation).times(follow))
    }

    function setKey(key, pressed) {
        switch (key) {
            case Qt.Key_W: leftUp = pressed; return true
            case Qt.Key_S: leftDown = pressed; return true
            case Qt.Key_Up: rightUp = pressed; return true
            case Qt.Key_Down: rightDown = pressed; return true
        }
        return false
    }

    function pointerSide(x) {
        return mode !== GameScene.TwoPlayers || x < 0 ? Match.LeftSide : Match.RightSide
    }

    function setPointer(id, x, y) {
        const position = phong.toScene(x, y, root)
        if (pointerSides[id] === Match.LeftSide)
            leftPointerY = position.y
        else if (pointerSides[id] === Match.RightSide)
            rightPointerY = position.y
    }

    onActiveChanged: {
        if (!active) {
            releaseInput()
            viewOffset = viewRotation = Qt.vector3d(0, 0, 0)
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

        if (match.state === Match.Paused) {
            switch (event.key) {
                // Esc opened the pause menu, Esc closes it again
                case Qt.Key_Escape:
                    SoundEffects.play(SoundEffects.MenuSelect)
                    match.resume()
                    return
                case Qt.Key_Up:
                    currentPauseItem = 0
                    SoundEffects.play(SoundEffects.MenuMove)
                    return
                case Qt.Key_Down:
                    currentPauseItem = pauseItems.length - 1
                    SoundEffects.play(SoundEffects.MenuMove)
                    return
                case Qt.Key_Enter:
                case Qt.Key_Return:
                    SoundEffects.play(SoundEffects.MenuSelect)
                    pauseItems[currentPauseItem].activate()
                    return
            }
        }

        if (setKey(event.key, true))
            return

        switch (event.key) {
            case Qt.Key_Escape:
                if (running)
                    match.pause()
                else if (match.state !== Match.Paused)
                    leave()
                break
            case Qt.Key_P:
            case Qt.Key_Space:
                togglePause()
                break
        }
    }

    onKeyReleased: (event) => {
        event.accepted = true
        if (!event.isAutoRepeat)
            setKey(event.key, false)
    }

    onPointerPressed: (id, x, y) => {
        const clickable = phong.clickableAt(x, y)
        if (clickable) {
            clickable.clicked()
            return
        }

        const sides = pointerSides
        sides[id] = pointerSide(phong.toScene(x, y, root).x)
        pointerSides = sides
        setPointer(id, x, y)
    }

    onPointerMoved: (id, x, y) => setPointer(id, x, y)

    onPointerReleased: (id) => {
        if (pointerSides[id] === Match.LeftSide)
            leftPointerY = NaN
        else if (pointerSides[id] === Match.RightSide)
            rightPointerY = NaN

        const sides = pointerSides
        delete sides[id]
        pointerSides = sides
    }

    Match {
        id: match

        pointsToWin: GameSettings.pointsToWin
        setsToWin: root.mode === GameScene.Ladder ? 1 : GameSettings.setsToWin
        winByTwo: GameSettings.winByTwo
        serveSpeed: GameSettings.serveSpeed
        maxSpeed: GameSettings.maxSpeed
        paddleSpeed: root.paddleSpeed
        serveDelay: GameSettings.kickoffTime

        left.name: root.againstComputer ? qsTr("You") : qsTr("Ping")
        right.name: root.againstComputer ? qsTr("CPU") : qsTr("Pong")
        right.computer: root.againstComputer

        onServed: SoundEffects.play(SoundEffects.Serve)

        onServeCountdownChanged: {
            const second = Math.ceil(match.serveCountdown)
            if (second > 0 && second !== root.kickoffSecond)
                SoundEffects.play(SoundEffects.CountdownTick)
            root.kickoffSecond = second
        }

        onPaddleHitBall: (ball, side) => {
            const paddle = side === Match.LeftSide ? leftPaddle : rightPaddle
            const speed = ball.velocity.length() / match.serveSpeed
            paddle.flash()
            SoundEffects.play(SoundEffects.PaddleHit, Math.min(0.9 + 0.35 * (speed - 1.0), 1.6))
            sparks.burst(Qt.vector3d(paddle.x + (side === Match.LeftSide ? 0.6 : -0.6), paddle.y, 0.5),
                         Theme.ball, Math.round(6 + 6 * speed))
            if (speed > 1.5)
                root.shake(0.08 * speed)
        }

        onPointScored: (scorer, ball) => {
            const goalX = scorer === Match.LeftSide ? root.goalLine : -root.goalLine
            SoundEffects.play(SoundEffects.Goal)
            sparks.burst(Qt.vector3d(goalX, 0, 0.5), Theme.text, 60)
            root.shake(0.7)
            if (scorer === Match.LeftSide)
                rightGoalFlash.restart()
            else
                leftGoalFlash.restart()

            if (!ball.extra) {
                root.resetBall()
                computer.reset()
            }
        }

        onSetFinished: (winner) => {
            if (match.state !== Match.Finished)
                banner.show(qsTr("Set %1").arg((winner === Match.LeftSide ? match.left : match.right).name),
                            Theme.title)
        }

        // Every pause starts on resume
        onStateChanged: {
            if (match.state === Match.Paused)
                root.currentPauseItem = 0
        }

        onFinished: {
            const won = match.winner === match.left
            SoundEffects.play(root.againstComputer && !won ? SoundEffects.Lose : SoundEffects.Win)
            Stats.recordMatch(root.againstComputer, root.difficulty, won, match.longestRally)
            if (root.mode === GameScene.Ladder && won)
                Stats.recordLadder(root.ladderStage + 1)

            // Let the last point sink in before showing the results
            resultsDelay.start()
        }
    }

    ComputerPlayer {
        id: computer

        difficulty: root.difficulty
        paddleX: root.paddleX - 0.5 * root.paddleWidth - root.ballRadius
        paddleReach: 0.5 * root.rightPaddleLength + root.ballRadius
        fieldTop: root.ballLimit
        fieldBottom: -root.ballLimit
    }

    Modifiers {
        id: modifiers

        match: match
        enabled: GameSettings.modifiers
        // Inside the narrowest field, clear of the paddles
        readonly property real spawnHeight: root.innerHeight + 2.0 * root.fieldInset
                                            - 2.0 * maxFieldInset - 3.0
        spawnArea: Qt.rect(-8.0, -0.5 * spawnHeight, 16.0, spawnHeight)

        onCollected: (index, side, position) => {
            const definition = modifiers.definition(index)
            pickupPopup.show(definition, position)
            sparks.burst(Qt.vector3d(position.x, position.y, 0.5), Theme.tint(definition.color), 24)

            if (definition.effect === Modifiers.MultiBall)
                SoundEffects.play(SoundEffects.MultiBall)
            else if (definition.target === Modifiers.Opponent)
                SoundEffects.play(SoundEffects.Curse)
            else
                SoundEffects.play(SoundEffects.Pickup)
        }
        onEffectsChanged: {
            root.leftEffects = activeEffects(Match.LeftSide)
            root.rightEffects = activeEffects(Match.RightSide)
        }
    }

    Timer {
        id: resultsDelay
        interval: 900
        onTriggered: root.phong.showResults()
    }

    PhysicsWorld {
        scene: root
        running: root.active && root.running
        gravity: Qt.vector3d(0, 0, 0)
        enableCCD: true
        // A handful of bodies, and single-threaded wasm hangs waiting for workers
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

    // The floor under the field, with a faint grid
    Node {
        visible: GraphicsSettings.floor
        z: -0.7

        Model {
            source: "#Rectangle"
            scale: Qt.vector3d((root.stageWidth + 2.0) / 100, (root.stageHeight + 2.0) / 100, 1)
            materials: PhongMaterial {
                color: Theme.floor
                shininess: 0.15
            }
        }

        Model {
            z: 0.02
            scale: Qt.vector3d(0.5 * root.stageWidth, 0.5 * root.stageHeight, 1)
            // The lines span -1 to 1, scaled to the field
            geometry: GridGeometry {
                horizontalLines: 21
                verticalLines: 35
                horizontalStep: 2.0 / 20
                verticalStep: 2.0 / 34
            }
            materials: PhongMaterial {
                color: Theme.grid
                glow: 0.6
                lighting: DefaultMaterial.NoLighting
            }
        }
    }

    // Soft shadows on the floor, the key light comes from above in front
    Node {
        visible: GraphicsSettings.floor && GraphicsSettings.shadows
        z: -0.6

        component Shadow: Model {
            property real size: 1.0
            source: "#Sphere"
            scale: Qt.vector3d(size / 100, size / 100, 0.001)
            opacity: 0.45
            materials: DefaultMaterial {
                diffuseColor: Theme.shadow
                lighting: DefaultMaterial.NoLighting
            }
        }

        Shadow {
            position: Qt.vector3d(mainBall.x + 0.3, mainBall.y - 0.6, 0)
            size: 2.2 * root.ballRadius
        }

        Repeater3D {
            model: match.extraBalls

            delegate: Shadow {
                required property int index
                readonly property var body: extraBalls.objectAt(index)
                position: body ? Qt.vector3d(body.x + 0.3, body.y - 0.6, 0) : Qt.vector3d(0, 0, 0)
                size: 2.2 * root.ballRadius
            }
        }

        Shadow {
            position: Qt.vector3d(leftPaddle.x + 0.3, leftPaddle.y - 0.6, 0)
            scale: Qt.vector3d(1.4 / 100, (root.leftPaddleLength + 0.4) / 100, 0.001)
            source: "#Cube"
        }

        Shadow {
            position: Qt.vector3d(rightPaddle.x + 0.3, rightPaddle.y - 0.6, 0)
            scale: Qt.vector3d(1.4 / 100, (root.rightPaddleLength + 0.4) / 100, 0.001)
            source: "#Cube"
        }
    }

    // Goal planes
    Model {
        position: Qt.vector3d(-0.5 * (root.stageWidth - root.goalDepth), 0, -0.5)
        scale: Qt.vector3d(root.goalDepth / 100, 2.0 * root.wallY / 100, 1)
        source: "#Rectangle"
        materials: PhongMaterial {
            id: leftGoalMaterial
            color: Theme.goal
            glow: 0.15
        }
    }

    Model {
        position: Qt.vector3d(0.5 * (root.stageWidth - root.goalDepth), 0, -0.5)
        scale: Qt.vector3d(root.goalDepth / 100, 2.0 * root.wallY / 100, 1)
        source: "#Rectangle"
        materials: PhongMaterial {
            id: rightGoalMaterial
            color: Theme.goal
            glow: 0.15
        }
    }

    // The goal flashes in the color of the scorer
    SequentialAnimation {
        id: leftGoalFlash
        ColorAnimation { target: leftGoalMaterial; property: "color"; to: Theme.rightPlayer; duration: 60 }
        ColorAnimation { target: leftGoalMaterial; property: "color"; to: Theme.goal; duration: 600 }
    }

    SequentialAnimation {
        id: rightGoalFlash
        ColorAnimation { target: rightGoalMaterial; property: "color"; to: Theme.leftPlayer; duration: 60 }
        ColorAnimation { target: rightGoalMaterial; property: "color"; to: Theme.goal; duration: 600 }
    }

    // Goals, a ball whose center passed the shield line is out
    TriggerBody {
        id: leftGoal
        x: -(root.goalLine + 5.0)
        collisionShapes: BoxShape {
            extents: Qt.vector3d(10.0, root.stageHeight + 10.0, 2.0)
        }
        onBodyEntered: (body) => {
            const ballBody = body as BallBody
            if (ballBody)
                root.queueGoal(ballBody.ball, Match.RightSide)
        }
    }

    TriggerBody {
        id: rightGoal
        x: root.goalLine + 5.0
        collisionShapes: BoxShape {
            extents: Qt.vector3d(10.0, root.stageHeight + 10.0, 2.0)
        }
        onBodyEntered: (body) => {
            const ballBody = body as BallBody
            if (ballBody)
                root.queueGoal(ballBody.ball, Match.LeftSide)
        }
    }

    // Walls, kinematic to move in when the field is narrowed
    DynamicRigidBody {
        id: topWall
        isKinematic: true
        // Created at the target, not at the origin on top of the ball
        position: kinematicPosition
        kinematicPosition: Qt.vector3d(0, root.wallY, 0)
        physicsMaterial: bouncy
        sendContactReports: true
        collisionShapes: BoxShape {
            extents: Qt.vector3d(root.stageWidth, root.wallThickness, 1.0)
        }

        Model {
            source: "#Cube"
            scale: Qt.vector3d(root.stageWidth / 100, root.wallThickness / 100, 0.01)
            materials: PhongMaterial {
                color: Theme.wall
                glow: 0.35
                shininess: 0.5
            }
        }
    }

    DynamicRigidBody {
        id: bottomWall
        isKinematic: true
        // Created at the target, not at the origin on top of the ball
        position: kinematicPosition
        kinematicPosition: Qt.vector3d(0, -root.wallY, 0)
        physicsMaterial: bouncy
        sendContactReports: true
        collisionShapes: BoxShape {
            extents: Qt.vector3d(root.stageWidth, root.wallThickness, 1.0)
        }

        Model {
            source: "#Cube"
            scale: Qt.vector3d(root.stageWidth / 100, root.wallThickness / 100, 0.01)
            materials: PhongMaterial {
                color: Theme.wall
                glow: 0.35
                shininess: 0.5
            }
        }
    }

    // Arena obstacles, round bumpers and blocks
    Repeater3D {
        model: root.arena.bumpers ?? []

        delegate: StaticRigidBody {
            id: bumper

            required property var modelData
            readonly property bool obstacle: true
            property real glow: 0.0

            function flash() {
                bumperGlow.restart()
            }

            position: Qt.vector3d(modelData.x, modelData.y, 0)
            physicsMaterial: bouncy
            sendContactReports: true
            collisionShapes: SphereShape {
                diameter: 2.0 * bumper.modelData.radius
            }

            Disc {
                radius: bumper.modelData.radius * (1.0 + 0.15 * bumper.glow)
                thickness: 1.0
                color: Qt.tint(Theme.bumper, Qt.rgba(1, 1, 1, 0.7 * bumper.glow))
                glow: 0.2 + bumper.glow
                shininess: 0.4
            }

            NumberAnimation {
                id: bumperGlow
                target: bumper
                property: "glow"
                from: 1.0
                to: 0.0
                duration: 300
                easing.type: Easing.OutQuad
            }
        }
    }

    Repeater3D {
        model: root.arena.blocks ?? []

        delegate: StaticRigidBody {
            id: block

            required property var modelData
            readonly property bool obstacle: true
            property real glow: 0.0

            function flash() {
                blockGlow.restart()
            }

            position: Qt.vector3d(modelData.x, modelData.y, 0)
            physicsMaterial: bouncy
            sendContactReports: true
            collisionShapes: BoxShape {
                extents: Qt.vector3d(block.modelData.width, block.modelData.height, 1.0)
            }

            Model {
                source: "#Cube"
                scale: Qt.vector3d(block.modelData.width / 100, block.modelData.height / 100, 0.01)
                materials: PhongMaterial {
                    color: Qt.tint(Theme.block, Qt.rgba(1, 1, 1, 0.7 * block.glow))
                    glow: 0.3 + block.glow
                    shininess: 0.5
                }
            }

            NumberAnimation {
                id: blockGlow
                target: block
                property: "glow"
                from: 1.0
                to: 0.0
                duration: 300
                easing.type: Easing.OutQuad
            }
        }
    }

    PaddleBody {
        id: leftPaddle
        color: Theme.leftPlayer
        paddleX: -root.paddleX
        paddleY: root.leftPaddleY
        angle: root.leftPaddleAngle
        length: root.leftPaddleLength
        width: root.paddleWidth
    }

    PaddleBody {
        id: rightPaddle
        color: Theme.rightPlayer
        paddleX: root.paddleX
        paddleY: root.rightPaddleY
        angle: root.rightPaddleAngle
        length: root.rightPaddleLength
        width: root.paddleWidth
    }

    // Shields, parked far away while the player has none
    DynamicRigidBody {
        id: leftShield
        isKinematic: true
        // Created at the target, not at the origin on top of the ball
        position: kinematicPosition
        kinematicPosition: Qt.vector3d(-root.shieldX, match.left.shielded ? 0 : 1000, 0)
        physicsMaterial: bouncy
        sendContactReports: true
        collisionShapes: BoxShape {
            extents: Qt.vector3d(root.shieldWidth, root.innerHeight, 1.0)
        }

        Model {
            visible: match.left.shielded
            source: "#Cube"
            scale: Qt.vector3d(root.shieldWidth / 100, root.innerHeight / 100, 0.01)
            materials: PhongMaterial {
                color: Theme.shield
                glow: 1.0
            }
        }
    }

    DynamicRigidBody {
        id: rightShield
        isKinematic: true
        // Created at the target, not at the origin on top of the ball
        position: kinematicPosition
        kinematicPosition: Qt.vector3d(root.shieldX, match.right.shielded ? 0 : 1000, 0)
        physicsMaterial: bouncy
        sendContactReports: true
        collisionShapes: BoxShape {
            extents: Qt.vector3d(root.shieldWidth, root.innerHeight, 1.0)
        }

        Model {
            visible: match.right.shielded
            source: "#Cube"
            scale: Qt.vector3d(root.shieldWidth / 100, root.innerHeight / 100, 0.01)
            materials: PhongMaterial {
                color: Theme.shield
                glow: 1.0
            }
        }
    }

    // Collectible modifiers, a ball picks them up by flying through
    Repeater3D {
        model: modifiers

        delegate: TriggerBody {
            id: item

            required property int itemId
            required property color itemColor
            required property string itemGlyph
            required property real itemX
            required property real itemY

            position: Qt.vector3d(itemX, itemY, 0)
            collisionShapes: SphereShape {
                diameter: 1.8
            }
            onBodyEntered: (body) => {
                const ballBody = body as BallBody
                if (ballBody)
                    root.queueCollect(item.itemId, ballBody.ball)
            }

            ModifierItem {
                color: item.itemColor
                glyph: item.itemGlyph

                Vector3dAnimation on scale {
                    from: Qt.vector3d(0, 0, 0)
                    to: Qt.vector3d(1, 1, 1)
                    duration: 300
                    easing.type: Easing.OutBack
                }
            }
        }
    }

    // The engine detects the hits and moves the balls, the match decides
    // where they bounce to
    BallBody {
        id: mainBall
        ball: match.ball
        radius: root.ballRadius
        trailSpeed: match.serveSpeed
        onContact: (body, normals) => root.ballContact(mainBall, body, normals)
    }

    Repeater3D {
        id: extraBalls
        model: match.extraBalls

        delegate: BallBody {
            id: extraBall
            radius: root.ballRadius
            trailSpeed: match.serveSpeed
            onContact: (body, normals) => root.ballContact(extraBall, body, normals)
        }
    }

    Sparks {
        id: sparks
    }

    // Kickoff countdown around the ball and where the ball will go
    Node {
        id: kickoff

        readonly property real fraction: match.serveDelay > 0.0 ? match.serveCountdown / match.serveDelay : 0.0
        readonly property int dots: 16

        visible: match.state === Match.Serving
        z: 0.6

        // A clock of dots going out one after another
        Repeater3D {
            model: kickoff.dots

            delegate: Disc {
                required property int index
                readonly property real angle: 0.5 * Math.PI - index * 2.0 * Math.PI / kickoff.dots

                visible: index < Math.ceil(kickoff.fraction * kickoff.dots)
                position: Qt.vector3d(1.7 * Math.cos(angle), 1.7 * Math.sin(angle), 0)
                radius: 0.15
                thickness: 0.2
                color: Theme.text
            }
        }

        // Arrow in the direction of the kickoff
        Node {
            eulerRotation.z: Math.atan2(match.serveDirection.y, match.serveDirection.x) * 180.0 / Math.PI

            Node {
                id: arrow
                property real pulse: 0.0
                x: 3.4 + 0.5 * pulse

                SequentialAnimation on pulse {
                    running: kickoff.visible
                    loops: Animation.Infinite
                    NumberAnimation { from: 0.0; to: 1.0; duration: 350; easing.type: Easing.OutQuad }
                    NumberAnimation { from: 1.0; to: 0.0; duration: 350; easing.type: Easing.InQuad }
                }

                Repeater3D {
                    // Shaft and the two strokes of the head, tip at the origin
                    model: [
                        { x: -0.75, y: 0.0, length: 1.3, angle: 0 },
                        { x: -0.38, y: 0.32, length: 1.0, angle: 140 },
                        { x: -0.38, y: -0.32, length: 1.0, angle: -140 }
                    ]

                    delegate: Model {
                        required property var modelData
                        position: Qt.vector3d(modelData.x, modelData.y, 0)
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
        }

        // Seconds left
        Text3D {
            y: -4.2
            scale: Qt.vector3d(1.6, 1.6, 1.6)
            horizontalAlignment: Text.AlignHCenter
            color: Theme.title
            text: Math.max(1, Math.ceil(match.serveCountdown))
        }
    }

    // Name of the collected modifier, rising and fading
    Node {
        id: pickupPopup

        property var definition: ({})
        property real startY: 0.0

        function show(definition, position) {
            pickupPopup.definition = definition
            x = position.x
            startY = position.y + 1.2
            popupAnimation.restart()
        }

        z: 1.5
        opacity: 0.0

        Text3D {
            scale: Qt.vector3d(0.6, 0.6, 0.6)
            horizontalAlignment: Text.AlignHCenter
            color: Theme.tint(pickupPopup.definition.color ?? Theme.text)
            text: pickupPopup.definition.name ?? ""
        }

        ParallelAnimation {
            id: popupAnimation
            NumberAnimation {
                target: pickupPopup; property: "opacity"
                from: 1.0; to: 0.0; duration: 1400; easing.type: Easing.InQuad
            }
            NumberAnimation {
                target: pickupPopup; property: "y"
                from: pickupPopup.startY; to: pickupPopup.startY + 2.0; duration: 1400
            }
        }
    }

    // Arena name at the start, set winners
    Banner {
        id: banner
        y: 3.0
        z: 1.5
    }

    // Scoreboard
    Node {
        y: 0.5 * root.stageHeight + 1.4

        Text3D {
            id: leftName
            x: -0.5 * root.stageWidth
            color: Theme.leftPlayer
            glow: 0.5
            text: match.left.name
        }

        // Active effects next to the names
        Repeater3D {
            model: root.leftEffects

            delegate: ModifierItem {
                required property var modelData
                required property int index

                x: leftName.x + leftName.textWidth + 1.0 + index * 1.2
                y: 0.35
                radius: 0.5
                wobbling: false
                color: modelData.color
                glyph: modelData.glyph
            }
        }

        Repeater3D {
            model: root.rightEffects

            delegate: ModifierItem {
                required property var modelData
                required property int index

                x: rightName.x - rightName.textWidth - 1.0 - index * 1.2
                y: 0.35
                radius: 0.5
                wobbling: false
                color: modelData.color
                glyph: modelData.glyph
            }
        }

        Text3D {
            x: -2.0
            horizontalAlignment: Text.AlignRight
            text: match.left.score
        }

        Text3D {
            horizontalAlignment: Text.AlignHCenter
            text: ":"
        }

        Text3D {
            x: 2.0
            text: match.right.score
        }

        Text3D {
            id: rightName
            x: 0.5 * root.stageWidth
            horizontalAlignment: Text.AlignRight
            color: Theme.rightPlayer
            glow: 0.5
            text: match.right.name
        }

        // Won sets as dots above the scores
        Repeater3D {
            model: match.setsToWin > 1 ? match.setsToWin : 0

            delegate: Disc {
                required property int index
                x: -2.4 - index * 0.7
                y: 1.5
                radius: 0.22
                thickness: 0.2
                color: index < match.left.sets ? Theme.title : Theme.goal
            }
        }

        Repeater3D {
            model: match.setsToWin > 1 ? match.setsToWin : 0

            delegate: Disc {
                required property int index
                x: 2.4 + index * 0.7
                y: 1.5
                radius: 0.22
                thickness: 0.2
                color: index < match.right.sets ? Theme.title : Theme.goal
            }
        }
    }

    // Controls
    Node {
        y: -0.5 * root.stageHeight - 2.2
        scale: Qt.vector3d(0.5, 0.5, 0.5)

        Text3D {
            x: -root.stageWidth
            color: Theme.dimmed
            text: root.mode === GameScene.TwoPlayers
                  ? qsTr("[W/S] left   [Up/Down] right")
                  : root.mode === GameScene.Ladder
                  ? qsTr("Ladder %1/3   [W/S] or [Up/Down] move").arg(root.ladderStage + 1)
                  : qsTr("[W/S] or [Up/Down] move")
        }

        Text3D {
            x: root.stageWidth
            horizontalAlignment: Text.AlignRight
            color: Theme.dimmed
            text: qsTr("[Esc] pause")
            clickable: root.running
            onClicked: match.pause()
        }
    }

    // Pause overlay
    Node {
        id: pauseOverlay
        visible: match.state === Match.Paused
        z: 1.0

        Model {
            source: "#Rectangle"
            scale: Qt.vector3d(root.stageWidth / 100, root.stageHeight / 100, 1)
            materials: DefaultMaterial {
                diffuseColor: Theme.background
                opacity: 0.75
                lighting: DefaultMaterial.NoLighting
            }
        }

        Text3D {
            y: 2.5
            scale: Qt.vector3d(2, 2, 2)
            horizontalAlignment: Text.AlignHCenter
            color: Theme.title
            text: qsTr("Paused")
        }

        Repeater3D {
            model: root.pauseItems

            delegate: Text3D {
                id: pauseItem

                required property var modelData
                required property int index

                y: -1.0 - index * 2.0
                horizontalAlignment: Text.AlignHCenter
                text: modelData.text
                clickable: pauseOverlay.visible
                onClicked: {
                    SoundEffects.play(SoundEffects.MenuSelect)
                    pauseItem.modelData.activate()
                }

                Disc {
                    visible: pauseItem.index === root.currentPauseItem
                    position: Qt.vector3d(-0.5 * pauseItem.textWidth - 1.0, 0.35, 0)
                    radius: 0.4
                }
            }
        }
    }
}
