pragma ComponentBehavior: Bound

import QtQuick
import QtQuick3D
import QtQuick3D.Physics
import Phong

Scene {
    id: root

    enum Mode {
        OnePlayer,
        TwoPlayers
    }

    property int mode: GameScene.OnePlayer
    property alias match: match

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

    // Held keys
    property bool leftUp: false
    property bool leftDown: false
    property bool rightUp: false
    property bool rightDown: false

    // Paddle targets of mouse or touch points, NaN when there is none
    property real leftPointerY: NaN
    property real rightPointerY: NaN
    property var pointerSides: ({})

    readonly property bool running: match.state === Match.Serving || match.state === Match.Playing

    // Items the ball flew through, collected after the physics step
    property var pendingItems: []

    // Definitions of the active effects, for the icons next to the names
    property var leftEffects: []
    property var rightEffects: []

    property int currentPauseItem: 0
    readonly property var pauseItems: [
        { text: qsTr("Resume"), activate: () => match.resume() },
        { text: qsTr("Menu"), activate: () => root.leave() }
    ]

    function startMatch() {
        leftPaddleY = 0.0
        rightPaddleY = 0.0
        leftPaddleAngle = 0.0
        rightPaddleAngle = 0.0
        releaseInput()
        resetBall()
        computer.reset()
        modifiers.reset()
        match.start()
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
        ball.reset(Qt.vector3d(0, 0, 0), Qt.vector3d(0, 0, 0))
        ball.setLinearVelocity(Qt.vector3d(0, 0, 0))
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

    // Upright paddles use the arcade bounce, turned ones reflect the ball
    // off their surface
    function paddleContact(side, paddle, angle, length, normals) {
        if (angle % 180 === 0 || normals.length === 0) {
            match.paddleHit(side, (ball.y - paddle.y) / (0.5 * length + ballRadius))
            return
        }

        // The contact normal points from the paddle to the ball
        let normal = Qt.vector2d(normals[0].x, normals[0].y)
        if (normal.dotProduct(Qt.vector2d(ball.x - paddle.x, ball.y - paddle.y)) < 0)
            normal = normal.times(-1)
        match.deflect(side, normal)
    }

    // Removing an item destroys its trigger body, not while it reports
    function queueCollect(itemId) {
        pendingItems.push(itemId)
        Qt.callLater(collectPending)
    }

    function collectPending() {
        const itemIds = pendingItems
        pendingItems = []
        for (const itemId of itemIds)
            modifiers.collect(itemId)
    }


    // Called after every physics step
    function step(dt) {
        match.advance(dt)
        if (!running)
            return

        modifiers.advance(dt)

        const onePlayer = mode === GameScene.OnePlayer
        const leftInput = onePlayer
                ? humanInput(leftUp || rightUp, leftDown || rightDown, leftPointerY, leftPaddleY)
                : humanInput(leftUp, leftDown, leftPointerY, leftPaddleY)

        let rightInput
        if (onePlayer) {
            computer.update(dt, Qt.vector2d(ball.x, ball.y), match.ballVelocity, rightPaddleY)
            rightInput = computer.direction
        }
        else {
            rightInput = humanInput(rightUp, rightDown, rightPointerY, rightPaddleY)
        }

        leftPaddleY = movePaddle(leftPaddleY, leftInput, leftPaddleLength, dt)
        rightPaddleY = movePaddle(rightPaddleY, rightInput, rightPaddleLength, dt)
        leftPaddleAngle = turnPaddle(leftPaddleAngle, match.left.spinSpeed, dt)
        rightPaddleAngle = turnPaddle(rightPaddleAngle, match.right.spinSpeed, dt)
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
        return mode === GameScene.OnePlayer || x < 0 ? Match.LeftSide : Match.RightSide
    }

    function setPointer(id, x, y) {
        const position = phong.toScene(x, y, root)
        if (pointerSides[id] === Match.LeftSide)
            leftPointerY = position.y
        else if (pointerSides[id] === Match.RightSide)
            rightPointerY = position.y
    }

    onActiveChanged: {
        if (!active)
            releaseInput()
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
                case Qt.Key_Up:
                    currentPauseItem = 0
                    return
                case Qt.Key_Down:
                    currentPauseItem = pauseItems.length - 1
                    return
                case Qt.Key_Enter:
                case Qt.Key_Return:
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
                else
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
        serveSpeed: GameSettings.serveSpeed
        maxSpeed: GameSettings.maxSpeed

        left.name: root.mode === GameScene.OnePlayer ? qsTr("You") : qsTr("Ping")
        right.name: root.mode === GameScene.OnePlayer ? qsTr("CPU") : qsTr("Pong")
        right.computer: root.mode === GameScene.OnePlayer

        onBallVelocityChanged: (velocity) => ball.setLinearVelocity(Qt.vector3d(velocity.x, velocity.y, 0))

        onPointScored: (scorer) => {
            root.resetBall()
            computer.reset()
            if (scorer === Match.LeftSide)
                rightGoalFlash.restart()
            else
                leftGoalFlash.restart()
        }

        // Every pause starts on resume
        onStateChanged: {
            if (match.state === Match.Paused)
                root.currentPauseItem = 0
        }

        // Let the last point sink in before showing the results
        onFinished: resultsDelay.start()
    }

    ComputerPlayer {
        id: computer

        difficulty: GameSettings.difficulty
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

        onCollected: (definition, side, position) => pickupPopup.show(modifiers.definition(definition), position)
        onEffectsChanged: {
            root.leftEffects = activeEffects(Match.LeftSide)
            root.rightEffects = activeEffects(Match.RightSide)
        }
    }

    Timer {
        id: resultsDelay
        interval: 800
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

    // Goal planes
    Model {
        position: Qt.vector3d(-0.5 * (root.stageWidth - root.goalDepth), 0, -0.5)
        scale: Qt.vector3d(root.goalDepth / 100, 2.0 * root.wallY / 100, 1)
        source: "#Rectangle"
        materials: DefaultMaterial {
            id: leftGoalMaterial
            diffuseColor: Theme.goal
            specularAmount: 0.0
        }
    }

    Model {
        position: Qt.vector3d(0.5 * (root.stageWidth - root.goalDepth), 0, -0.5)
        scale: Qt.vector3d(root.goalDepth / 100, 2.0 * root.wallY / 100, 1)
        source: "#Rectangle"
        materials: DefaultMaterial {
            id: rightGoalMaterial
            diffuseColor: Theme.goal
            specularAmount: 0.0
        }
    }

    SequentialAnimation {
        id: leftGoalFlash
        ColorAnimation { target: leftGoalMaterial; property: "diffuseColor"; to: Theme.text; duration: 60 }
        ColorAnimation { target: leftGoalMaterial; property: "diffuseColor"; to: Theme.goal; duration: 500 }
    }

    SequentialAnimation {
        id: rightGoalFlash
        ColorAnimation { target: rightGoalMaterial; property: "diffuseColor"; to: Theme.text; duration: 60 }
        ColorAnimation { target: rightGoalMaterial; property: "diffuseColor"; to: Theme.goal; duration: 500 }
    }

    // Goals, a ball whose center passed the shield line is out
    TriggerBody {
        id: leftGoal
        x: -(root.goalLine + 5.0)
        collisionShapes: BoxShape {
            extents: Qt.vector3d(10.0, root.stageHeight + 10.0, 2.0)
        }
        onBodyEntered: (body) => {
            if (body === ball)
                match.goal(Match.RightSide)
        }
    }

    TriggerBody {
        id: rightGoal
        x: root.goalLine + 5.0
        collisionShapes: BoxShape {
            extents: Qt.vector3d(10.0, root.stageHeight + 10.0, 2.0)
        }
        onBodyEntered: (body) => {
            if (body === ball)
                match.goal(Match.LeftSide)
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
            materials: DefaultMaterial {
                diffuseColor: Theme.wall
                specularAmount: 0.0
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
            materials: DefaultMaterial {
                diffuseColor: Theme.wall
                specularAmount: 0.0
            }
        }
    }

    // Paddles, moved by the players and pushed around by nothing
    DynamicRigidBody {
        id: leftPaddle
        isKinematic: true
        // Created at the target, not at the origin on top of the ball. Only
        // the start, the engine writes the simulated pose back to position.
        position: kinematicPosition
        kinematicPosition: Qt.vector3d(-root.paddleX, root.leftPaddleY, 0)
        kinematicEulerRotation: Qt.vector3d(0, 0, root.leftPaddleAngle)
        physicsMaterial: bouncy
        sendContactReports: true
        collisionShapes: BoxShape {
            extents: Qt.vector3d(root.paddleWidth, root.leftPaddleLength, 1.0)
        }

        Model {
            source: "#Cube"
            scale: Qt.vector3d(root.paddleWidth / 100, root.leftPaddleLength / 100, 0.01)
            materials: DefaultMaterial {
                diffuseColor: Theme.paddle
                specularAmount: 0.0
            }
        }
    }

    DynamicRigidBody {
        id: rightPaddle
        isKinematic: true
        // Created at the target, not at the origin on top of the ball. Only
        // the start, the engine writes the simulated pose back to position.
        position: kinematicPosition
        kinematicPosition: Qt.vector3d(root.paddleX, root.rightPaddleY, 0)
        kinematicEulerRotation: Qt.vector3d(0, 0, root.rightPaddleAngle)
        physicsMaterial: bouncy
        sendContactReports: true
        collisionShapes: BoxShape {
            extents: Qt.vector3d(root.paddleWidth, root.rightPaddleLength, 1.0)
        }

        Model {
            source: "#Cube"
            scale: Qt.vector3d(root.paddleWidth / 100, root.rightPaddleLength / 100, 0.01)
            materials: DefaultMaterial {
                diffuseColor: Theme.paddle
                specularAmount: 0.0
            }
        }
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
            materials: DefaultMaterial {
                diffuseColor: Theme.shield
                specularAmount: 0.0
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
            materials: DefaultMaterial {
                diffuseColor: Theme.shield
                specularAmount: 0.0
            }
        }
    }

    // Collectible modifiers, the ball picks them up by flying through
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
                if (body === ball)
                    root.queueCollect(item.itemId)
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
            color: pickupPopup.definition.color ?? Theme.text
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

    // The engine detects the hits and moves the ball, the match decides
    // where it bounces to
    DynamicRigidBody {
        id: ball
        gravityEnabled: false
        linearAxisLock: DynamicRigidBody.LockZ
        angularAxisLock: DynamicRigidBody.LockX | DynamicRigidBody.LockY | DynamicRigidBody.LockZ
        physicsMaterial: bouncy
        receiveContactReports: true
        sendTriggerReports: true
        collisionShapes: SphereShape {
            diameter: 2.0 * root.ballRadius
        }

        onBodyContact: (body, positions, impulses, normals) => {
            if (body === leftPaddle)
                root.paddleContact(Match.LeftSide, leftPaddle, root.leftPaddleAngle,
                                   root.leftPaddleLength, normals)
            else if (body === rightPaddle)
                root.paddleContact(Match.RightSide, rightPaddle, root.rightPaddleAngle,
                                   root.rightPaddleLength, normals)
            else if (body === topWall)
                match.wallHit(true)
            else if (body === bottomWall)
                match.wallHit(false)
            else if (body === leftShield)
                modifiers.shieldHit(Match.LeftSide)
            else if (body === rightShield)
                modifiers.shieldHit(Match.RightSide)
        }

        Disc {
            radius: root.ballRadius
            color: Theme.ball
        }
    }

    // Scoreboard
    Node {
        y: 0.5 * root.stageHeight + 1.4

        Text3D {
            id: leftName
            x: -0.5 * root.stageWidth
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
            text: match.right.name
        }
    }

    // Controls
    Node {
        y: -0.5 * root.stageHeight - 2.2
        scale: Qt.vector3d(0.5, 0.5, 0.5)

        Text3D {
            x: -root.stageWidth
            color: Theme.dimmed
            text: root.mode === GameScene.OnePlayer
                  ? qsTr("[W/S] or [Up/Down] move")
                  : qsTr("[W/S] left   [Up/Down] right")
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
                diffuseColor: "black"
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
                onClicked: pauseItem.modelData.activate()

                Disc {
                    visible: pauseItem.index === root.currentPauseItem
                    position: Qt.vector3d(-0.5 * pauseItem.textWidth - 1.0, 0.35, 0)
                    radius: 0.4
                }
            }
        }
    }
}
