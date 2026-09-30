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
        Ladder,
        Endless,
        Tournament,
        Bricks,
        Squash
    }

    property int mode: GameScene.OnePlayer
    property Scene menuScene
    property Scene settingsScene
    property var rules: GameSettings.rules(mode)
    // Ladder level, the computer's difficulty
    property int ladderStage: 0
    property alias match: match

    readonly property bool squash: mode === GameScene.Squash
    readonly property bool againstComputer: mode !== GameScene.TwoPlayers && !squash
    readonly property bool endless: mode === GameScene.Endless
    readonly property bool tournament: mode === GameScene.Tournament
    readonly property bool bricks: mode === GameScene.Bricks
    // Alone against the computer or the wall, with a few balls to lose
    readonly property bool solo: endless || squash
    // The tournament opponent of this match, the tournament moves on
    // while the results show
    property var opponent: ({})
    // Endless gets harder the longer it lasts
    readonly property int difficulty: mode === GameScene.Ladder ? ladderStage
                                      : endless ? (match.playTime < 30 ? 0 : match.playTime < 90 ? 1 : 2)
                                      : tournament ? opponent.difficulty ?? rules.difficulty
                                      : rules.difficulty
    readonly property real endlessSpeedUp: endless ? 1.0 + Math.min(0.5, match.playTime / 240) : 1.0
    readonly property int lives: 3
    // Returns count, balls the computer missed count more
    readonly property int endlessScore: match.left.hits + 10 * match.left.score
    property bool newHighScore: false

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

    property real leftPaddleLength: rules.paddleLength * match.left.paddleScale
    Behavior on leftPaddleLength {
        NumberAnimation { duration: 300; easing.type: Easing.OutQuad }
    }
    property real rightPaddleLength: rules.paddleLength * match.right.paddleScale
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

    // Items balls flew through, goals and portal passages, handled after
    // the physics step
    property var pendingItems: []
    property var pendingGoals: []
    property var pendingPortals: []

    // A ghost ball can't be seen in the middle third of the field
    readonly property real ghostHalfWidth: stageWidth / 6

    // Definitions of the active effects, for the icons next to the names
    property var leftEffects: []
    property var rightEffects: []

    // Obstacles of the current arena
    property string arenaId: "classic"
    property var arena: ({})

    // Bricks in the middle of the field, they break when hit
    readonly property real brickWidth: 1.0
    readonly property real brickHeight: 1.8
    property int nextBrickId: 1
    property var pendingBricks: []

    // Challenges count for the one player against the computer or the
    // wall: perfect hits, the deepest deficit and the points lost
    readonly property bool challenges: againstComputer || squash
    property int leftPerfects: 0
    property int leftDeficit: 0
    property int leftConceded: 0
    // Names of the achievements of this match, for the results
    property var newAchievements: []

    function achieve(id) {
        if (challenges)
            Stats.unlock(id)
    }

    // After every point, the scores before a new set starts
    function countPoint(scorer) {
        if (scorer === Match.RightSide)
            ++leftConceded
        const left = match.left.score
        const right = match.right.score
        leftDeficit = Math.max(leftDeficit, right - left)
        lastSmash = Match.NoSide
        doubleSmashCalled = false

        // Level again after trailing by three
        behind = [Math.max(behind[0], right - left), Math.max(behind[1], left - right)]
        if (!solo && left === right && behind[scorer] >= 3 && !comebackCalled[scorer]) {
            const called = comebackCalled
            called[scorer] = true
            comebackCalled = called
            callout(qsTr("Comeback!"))
        }
    }

    // Callouts: how far each side was behind in this set, whether its
    // comeback was called, who smashed hard last in the rally and whether
    // a double smash was called in it
    property var behind: [0, 0]
    property var comebackCalled: [false, false]
    property int lastSmash: Match.NoSide
    property bool doubleSmashCalled: false

    function callout(text) {
        effect({ e: "callout", text: text })
    }

    // Two players on the LAN: the host runs the game and sends its state
    // after every step, the other machine shows it and sends its input.
    // The players on the network, { id, name, token } by side, at the host.
    property var remotes: [null, null]
    // Host: players on the network who watch, by id, and the sides of the
    // players who left by their token, they get them back when they join
    // again
    property var spectators: []
    property var departed: ({})
    // Host: the match was started on the LAN, it takes the ones who join
    property bool lanGame: false
    readonly property bool hostingLan: Lan.role === Lan.Host
                                       && (remotes.some((remote) => remote !== null) || spectators.length > 0)
    // Host: the input of the remote sides, and what happened since the
    // last state
    property var remoteMoves: [0, 0]
    property var netEvents: []
    // The computer took over a side whose player left
    property bool rightByComputer: false
    // On the joined machine: the side played there and the latest state
    property bool remote: false
    property int localSide: Match.RightSide
    // A joined machine without a side watches
    readonly property bool spectating: remote && localSide === Match.NoSide
    property var remoteBalls: []
    property var remotePaddles: [0, 0, 0, 0, 0, 0, 0, 0, 0, 0]
    property real remoteAge: 0.0
    property bool remoteUp: false
    property bool remoteDown: false
    property bool remoteCharging: false
    property real sentMove: 0.0
    property bool sentCharging: false
    property real sinceSent: 0.0
    property bool askLeave: false
    // On the network every player pauses for all, everybody sees who
    property string pausedBy: ""
    // The player on the network whose pause the host is taking
    property string pauser: ""
    // The input sent in the last moments, [seconds, move], for the own
    // paddle before the host's state has it
    property var inputHistory: []

    // Seconds to the host and back, as measured
    readonly property real roundTrip: Math.max(0, Math.min(Lan.latency, 500)) / 1000

    // Where a paddle at y the host sent is by now, after the input the host
    // can't have seen yet: sent within the round trip and the age of the
    // state
    function predictPaddle(y, length, charge, window) {
        let left = window
        for (let i = inputHistory.length - 1; i >= 0 && left > 0; --i) {
            const dt = Math.min(inputHistory[i][0], left)
            y = movePaddle(y, inputHistory[i][1], length, charge, dt)
            left -= dt
        }
        return y
    }

    // The top and bottom walls fold a ball flown on beyond them
    function foldY(y) {
        const limit = ballLimit
        if (Math.abs(y) <= limit)
            return y
        return Math.sign(y) * Math.max(0, 2 * limit - Math.abs(y))
    }

    // Sounds, sparks and banners of what happened, at the host they also
    // go to the players on the network
    function effect(event) {
        showEffect(event)
        if (hostingLan)
            netEvents.push(event)
    }

    // The gamepad playing a side here, if any
    function padOf(side) {
        if (remote)
            return side === localSide && Gamepads.count > 0 ? Gamepads.pads[0] : null
        if (remotes[side])
            return null
        return side === Match.LeftSide ? leftPad : rightPad
    }

    function rumble(side, strength, duration) {
        const pad = padOf(side)
        if (pad)
            Gamepads.rumble(pad, strength, duration)
    }

    function showEffect(event) {
        const paddle = event.side === Match.LeftSide ? leftPaddle : rightPaddle
        const sparkX = paddle.x + (event.side === Match.LeftSide ? 0.6 : -0.6)
        switch (event.e) {
            case "serve":
                SoundEffects.play(SoundEffects.Serve)
                clearRecording()
                break
            case "wall":
                SoundEffects.play(SoundEffects.WallHit)
                break
            case "shield":
                SoundEffects.play(SoundEffects.ShieldHit)
                sparks.burst(Qt.vector3d(event.x, event.y, 0.5), Theme.shield, 30)
                shake(0.35)
                break
            case "bounce":
                SoundEffects.play(SoundEffects.Bounce)
                sparks.burst(Qt.vector3d(event.x, event.y, 0.5), Theme.bumper, 14)
                shake(0.12)
                break
            case "portal":
                SoundEffects.play(SoundEffects.Portal)
                sparks.burst(Qt.vector3d(event.from[0], event.from[1], 0.5), Theme.tint(portalColors[event.index]), 16)
                sparks.burst(Qt.vector3d(event.to[0], event.to[1], 0.5), Theme.tint(portalColors[1 - event.index]), 16)
                break
            case "hit": {
                paddle.flash()
                if (event.smash >= 0.25)
                    rumble(event.side, 0.5 + 0.5 * event.smash, 160)
                else
                    rumble(event.side, event.perfect ? 0.45 : 0.25, 70)
                // Perfect hits ring an octave higher
                if (event.perfect) {
                    SoundEffects.play(SoundEffects.Perfect, rallyPitch(match.rally))
                    sparks.burst(Qt.vector3d(sparkX, paddle.y, 0.5), Theme.text, 30)
                    perfectPopup.show(qsTr("Perfect!"), Theme.text,
                                      Qt.vector3d(event.side === Match.LeftSide ? sparkX + 3.5 : sparkX - 3.5, paddle.y, 0))
                }
                if (event.smash >= 0.25) {
                    SoundEffects.play(SoundEffects.Smash, 0.8 + 0.4 * event.smash)
                    sparks.burst(Qt.vector3d(sparkX, paddle.y, 0.5), Theme.text, Math.round(20 + 40 * event.smash))
                    shake(0.3 + 0.5 * event.smash)
                }
                else {
                    SoundEffects.play(SoundEffects.PaddleHit, rallyPitch(match.rally))
                    sparks.burst(Qt.vector3d(sparkX, paddle.y, 0.5), Theme.ball, Math.round(6 + 6 * event.speed))
                    if (event.speed > 1.5)
                        shake(0.08 * event.speed)
                }
                break
            }
            case "special":
                rumble(event.side, 0.6, 220)
                SoundEffects.play(SoundEffects.Special)
                sparks.burst(Qt.vector3d(paddle.x, paddle.y, 0.5), paddle.color, 40)
                shake(0.2)
                break
            case "catch":
                rumble(event.side, 0.4, 120)
                paddle.flash()
                SoundEffects.play(SoundEffects.Catch)
                sparks.burst(Qt.vector3d(sparkX, paddle.y, 0.5), Theme.tint(paddle.magnetColor), 16)
                break
            case "dash":
                SoundEffects.play(SoundEffects.Dash)
                sparks.burst(Qt.vector3d(paddle.x, paddle.y, 0.5), paddle.color, 10)
                break
            case "goal":
                // The goal hurts the one who let it in
                rumble(event.scorer === Match.LeftSide ? Match.RightSide : Match.LeftSide, 1.0, 350)
                rumble(event.scorer, 0.3, 100)
                SoundEffects.play(SoundEffects.Goal)
                sparks.burst(Qt.vector3d(event.scorer === Match.LeftSide ? goalLine : -goalLine, 0, 0.5), Theme.text, 60)
                shake(0.7)
                if (event.scorer === Match.LeftSide)
                    rightGoalFlash.restart()
                else
                    leftGoalFlash.restart()
                break
            case "set":
                banner.show(qsTr("Set %1").arg(event.name), Theme.title)
                break
            case "pickup": {
                const definition = modifiers.definition(modifiers.findDefinition(event.def))
                const position = Qt.vector3d(event.x, event.y, 0)
                pickupPopup.show(definition.name, Theme.tint(definition.color), position)
                sparks.burst(Qt.vector3d(event.x, event.y, 0.5), Theme.tint(definition.color), 24)
                if (definition.effect === Modifiers.MultiBall)
                    SoundEffects.play(SoundEffects.MultiBall)
                else if (definition.effect === Modifiers.Portals)
                    SoundEffects.play(SoundEffects.Portal)
                else if (definition.effect === Modifiers.Freeze)
                    SoundEffects.play(SoundEffects.Freeze)
                else if (definition.target === Modifiers.Opponent || definition.effect === Modifiers.GhostBall)
                    SoundEffects.play(SoundEffects.Curse)
                else
                    SoundEffects.play(SoundEffects.Pickup)
                break
            }
            case "callout":
                SoundEffects.play(SoundEffects.RallyMilestone, 1.3)
                calloutBanner.show(event.text, Theme.accent, 1.2)
                break
            case "left":
                calloutBanner.show(qsTr("%1 left").arg(event.name), Theme.dimmed, 1.0)
                break
            case "back":
                calloutBanner.show(qsTr("%1 is back").arg(event.name), Theme.accent, 1.0)
                break
            case "finished":
                // The joined machine replays its own recording
                SoundEffects.play(spectating || match.winner === (localSide === Match.LeftSide ? match.left : match.right)
                                  ? SoundEffects.Win : SoundEffects.Lose)
                startReplay()
                break
        }
    }

    function sendState() {
        if (!hostingLan)
            return
        const balls = ballBodies().map((body) => [body.x, body.y])
        Lan.sendAll({
            t: "state", match: match.snapshot(), modifiers: modifiers.snapshot(), balls: balls,
            paddles: [leftPaddleY, rightPaddleY, leftPaddleAngle, rightPaddleAngle, leftCharge, rightCharge,
                      leftDash.direction, rightDash.direction, leftDash.cooldown, rightDash.cooldown],
            arenaTime: arenaTime, timeScale: timeScale, pausedBy: pausedBy, events: netEvents
        })
        netEvents = []
    }

    // The side a player on the network plays at the host
    function remoteSide(peer) {
        return remotes.findIndex((remote) => remote?.id === peer)
    }

    Connections {
        target: Lan
        enabled: root.active && root.hostingLan
        function onReceived(peer, message) {
            const side = root.remoteSide(peer)
            if (side < 0)
                return
            if (message.t === "input") {
                const moves = root.remoteMoves
                moves[side] = Math.max(-1, Math.min(1, Number(message.move) || 0))
                root.remoteMoves = moves
                const charging = message.charging === true
                if (charging !== (side === Match.LeftSide ? root.leftCharging : root.rightCharging))
                    root.setCharging(side, charging)
            }
            else if (message.t === "action" && message.a === "pause") {
                if (root.running) {
                    root.pauser = root.remotes[side].name
                    match.pause()
                }
            }
            else if (message.t === "action" && message.a === "resume") {
                if (match.state === Match.Paused)
                    match.resume()
            }
            else if (message.t === "action" && root.running) {
                const player = side === Match.LeftSide ? match.left : match.right
                if (message.a === "special")
                    match.useSpecial(side)
                else if (message.a === "tap" && !player.frozen)
                    (side === Match.LeftSide ? leftDash : rightDash).tap((message.d > 0 ? 1 : -1)
                                                                          * (player.reversed ? -1 : 1))
                // The dash button of a pad, right away
                else if (message.a === "dash" && !player.frozen)
                    (side === Match.LeftSide ? leftDash : rightDash).trigger((message.d > 0 ? 1 : -1)
                                                                              * (player.reversed ? -1 : 1))
            }
        }
    }

    Connections {
        target: Lan
        enabled: root.lanGame && Lan.role === Lan.Host
        function onPeerLeft(peer) {
            root.spectators = root.spectators.filter((id) => id !== peer)

            // The computer or the keyboard takes over until they are back
            const side = root.remoteSide(peer)
            if (side < 0)
                return
            root.effect({ e: "left", name: root.remotes[side].name })
            if ((root.remotes[side].token ?? "") !== "") {
                const departed = root.departed
                departed[root.remotes[side].token] = { side: side, name: root.remotes[side].name }
                root.departed = departed
            }
            const remotes = root.remotes.slice()
            remotes[side] = null
            root.remotes = remotes
            if (side === Match.RightSide)
                root.rightByComputer = true
        }

        // A player who left gets the side back, everybody else watches
        function onPeerJoined(peer, name, token) {
            const back = root.departed[token]
            if (back !== undefined && root.remotes[back.side] === null) {
                const departed = root.departed
                delete departed[token]
                root.departed = departed
                const remotes = root.remotes.slice()
                remotes[back.side] = { id: peer, name: back.name, token: token }
                root.remotes = remotes
                if (back.side === Match.RightSide)
                    root.rightByComputer = false
                root.effect({ e: "back", name: back.name })
                root.sendStart(peer, back.side)
                return
            }
            root.spectators = root.spectators.concat([peer])
            root.sendStart(peer, Match.NoSide)
        }
    }

    // The match so far for a player on the network, NoSide watches
    function sendStart(peer, side) {
        Lan.send(peer, { t: "start", party: false, side: side, arena: arenaId,
                         rules: root.rules, match: match.snapshot(), modifiers: modifiers.snapshot() })
    }

    // The game from the host, again after a rematch
    function startRemote(message) {
        mode = GameScene.TwoPlayers
        rules = message.rules ?? GameSettings.rules(mode)
        remote = true
        localSide = message.side
        askLeave = false
        replaying = false
        newAchievements = []
        clearRecording()
        releaseInput()
        arenaId = message.arena
        arena = Arenas.arena(message.arena)
        brickModel.clear()
        timeScale = 1.0
        remoteBalls = []
        resetBall()
        match.applySnapshot(message.match)
        modifiers.applySnapshot(message.modifiers)
        banner.show(spectating ? qsTr("Watching") : arena.name ?? "", Theme.title)
    }

    function applyRemote(message) {
        match.applySnapshot(message.match)
        pausedBy = message.pausedBy ?? ""
        modifiers.applySnapshot(message.modifiers)
        remoteBalls = message.balls
        remotePaddles = message.paddles
        arenaTime = message.arenaTime
        timeScale = message.timeScale
        remoteAge = 0.0
        for (const event of message.events ?? [])
            showEffect(event)
    }

    // The pause key on the joined machine: the host pauses or resumes for
    // everybody, watchers only get to leave
    function remotePause() {
        currentPauseItem = 0
        if (askLeave || spectating)
            askLeave = !askLeave
        else if (running)
            Lan.sendToHost({ t: "action", a: "pause" })
        else if (match.state === Match.Paused)
            Lan.sendToHost({ t: "action", a: "resume" })
        else
            askLeave = true
    }

    // A key on the joined machine: both sets move, smash and special go to
    // the host
    function remoteKey(key, pressed) {
        if (spectating)
            return false
        if (key === Qt.Key_Space) {
            remoteCharging = pressed
            return true
        }
        switch (KeySettings.action(key)) {
            case KeySettings.LeftUp:
            case KeySettings.RightUp:
                if (pressed && running)
                    Lan.sendToHost({ t: "action", a: "tap", d: 1 })
                remoteUp = pressed
                return true
            case KeySettings.LeftDown:
            case KeySettings.RightDown:
                if (pressed && running)
                    Lan.sendToHost({ t: "action", a: "tap", d: -1 })
                remoteDown = pressed
                return true
            case KeySettings.LeftSmash:
            case KeySettings.RightSmash:
                remoteCharging = pressed
                return true
            case KeySettings.LeftSpecial:
            case KeySettings.RightSpecial:
                if (pressed)
                    Lan.sendToHost({ t: "action", a: "special" })
                return true
        }
        return false
    }

    // The joined machine: the picture follows the host, the input goes there
    FrameAnimation {
        running: root.active && root.remote
        onTriggered: {
            const dt = Math.min(frameTime, 0.05)
            root.remoteAge += dt

            // The balls fly on from the last state to where they are at the
            // host by now, half a round trip later, a moment at most
            const age = match.state === Match.Playing
                      ? Math.min(root.remoteAge + 0.5 * root.roundTrip, 0.1 + Math.min(0.5 * root.roundTrip, 0.1))
                        * root.timeScale : 0.0
            const bodies = root.ballBodies()
            for (let i = 0; i < bodies.length; ++i) {
                const at = root.remoteBalls[i]
                if (!at)
                    continue
                const v = bodies[i].ball.velocity
                bodies[i].position = Qt.vector3d(at[0] + v.x * age, root.foldY(at[1] + v.y * age), 0)
                bodies[i].hidden = modifiers.ghostBall && match.state === Match.Playing
                                   && Math.abs(bodies[i].x) < root.ghostHalfWidth
                bodies[i].advance(dt)
            }

            // The own paddle answers the input right away, unless something
            // else moves it
            const paddles = root.remotePaddles
            const follow = Math.min(1.0, 25.0 * dt)
            const own = root.localSide === Match.LeftSide ? match.left : root.localSide === Match.RightSide ? match.right : null
            const predicting = own && !own.frozen && root.running
                               && !root.ballBodies().some((body) => body.ball.heldBy === root.localSide)
            const window = root.roundTrip + root.remoteAge
            const leftTarget = predicting && root.localSide === Match.LeftSide
                             ? root.predictPaddle(paddles[0], root.leftPaddleLength, paddles[4], window) : paddles[0]
            const rightTarget = predicting && root.localSide === Match.RightSide
                              ? root.predictPaddle(paddles[1], root.rightPaddleLength, paddles[5], window) : paddles[1]
            root.leftPaddleY += (leftTarget - root.leftPaddleY) * follow
            root.rightPaddleY += (rightTarget - root.rightPaddleY) * follow
            root.leftPaddleAngle = paddles[2]
            root.rightPaddleAngle = paddles[3]
            root.leftCharge = paddles[4]
            root.rightCharge = paddles[5]

            // The camera shakes and leans as at the host
            root.shakeAmount *= Math.exp(-7.0 * dt)
            root.viewOffset = Qt.vector3d((Math.random() - 0.5) * root.shakeAmount,
                                          (Math.random() - 0.5) * root.shakeAmount, -6.0 * root.slowMotion)
            root.viewRotation = root.viewRotation.plus(root.lean().minus(root.viewRotation)
                                                       .times(Math.min(1.0, 3.0 * dt)))
            if (root.running)
                root.record(dt)

            // Keys, the first gamepad, or the pointer as at the host
            let move = root.keyInput(root.remoteUp, root.remoteDown)
            let charging = root.remoteCharging
            const pad = Gamepads.count > 0 ? Gamepads.pads[0] : null
            if (pad) {
                if (move === 0)
                    move = pad.direction.y
                charging = charging || pad.isPressed(KeySettings.padButton(KeySettings.PadSmash))
            }
            // Reversed controls mirror it, the host swaps the move back
            if (move === 0) {
                const left = root.localSide === Match.LeftSide
                const sign = own?.reversed ? -1 : 1
                move = sign * root.humanInput(false, false, sign * (left ? root.leftPointerY : root.rightPointerY),
                                              left ? root.leftPaddleY : root.rightPaddleY)
            }
            root.sinceSent += dt
            if (root.spectating)
                return
            // As the host applies it, reversed controls swap it there
            const history = root.inputHistory
            history.push([dt, (own?.reversed ? -1 : 1) * Math.max(-1, Math.min(1, move))])
            if (history.length > 240)
                history.shift()
            if (move !== root.sentMove || charging !== root.sentCharging || root.sinceSent > 0.2) {
                Lan.sendToHost({ t: "input", move: move, charging: charging })
                root.sentMove = move
                root.sentCharging = charging
                root.sinceSent = 0.0
            }
        }
    }

    // The last seconds of play, replayed when the match is decided
    readonly property real replayLength: 3.0
    // The end of the replay runs slower
    readonly property real replaySlowTime: 1.0
    readonly property real replaySlowSpeed: 0.35
    property var recording: []
    property real recordedTime: 0.0
    property bool replaying: false
    property real replayTime: 0.0
    property var replayFrame: null
    property int replayIndex: 0

    function record(dt) {
        const balls = []
        for (const body of ballBodies())
            balls.push({ x: body.x, y: body.y, extra: body.ball.extra, shown: body.visibility })
        recording.push({
            dt: dt, balls: balls,
            leftY: leftPaddleY, rightY: rightPaddleY,
            leftAngle: leftPaddleAngle, rightAngle: rightPaddleAngle,
            leftLength: leftPaddleLength, rightLength: rightPaddleLength
        })
        recordedTime += dt
        while (recording.length > 1 && recordedTime - recording[0].dt > replayLength)
            recordedTime -= recording.shift().dt
    }

    function clearRecording() {
        recording = []
        recordedTime = 0.0
    }

    function startReplay() {
        if (recordedTime < 0.6) {
            resultsDelay.start()
            return
        }
        replayTime = 0.0
        replayIndex = 0
        replayFrame = recording[0]
        replaying = true
        viewOffset = Qt.vector3d(0, 0, -3)
        viewRotation = Qt.vector3d(0, 0, 0)
    }

    // Plays the recording on, returns false at its end
    function advanceReplay(dt) {
        const slow = replayTime > recordedTime - replaySlowTime
        replayTime += dt * (slow ? replaySlowSpeed : 1.0)
        let time = 0.0
        for (let index = 0; index < recording.length; ++index) {
            time += recording[index].dt
            if (time >= replayTime) {
                replayIndex = index
                replayFrame = recording[index]
                return true
            }
        }
        return false
    }

    function endReplay() {
        if (!replaying)
            return
        replaying = false
        replayFrame = null
        viewOffset = Qt.vector3d(0, 0, 0)

        // The deciding goal once more
        const leftWon = match.winner === match.left
        SoundEffects.play(SoundEffects.Goal)
        sparks.burst(Qt.vector3d(leftWon ? goalLine : -goalLine, 0, 0.5), Theme.text, 60)
        if (leftWon)
            rightGoalFlash.restart()
        else
            leftGoalFlash.restart()
        resultsDelay.start()
    }

    // Wide windows get the scores and keys beside the field, that leaves
    // the height to the field. Narrow ones have them above and below.
    readonly property bool sideLayout: (phong?.aspectRatio ?? 1.0) >= 1.72
    contentHalfWidth: sideLayout ? 22.6 : 18.5
    contentHalfHeight: sideLayout ? 10.9 : 12.9

    // Against the computer the first gamepad plays too, two players get
    // theirs in the lobby
    property var leftPadAssigned: null
    property var rightPadAssigned: null
    readonly property var primaryPad: Gamepads.count > 0 ? Gamepads.pads[0] : null
    readonly property int hostPadSide: !hostingLan ? Match.NoSide
                                        : remotes[0] === null ? Match.LeftSide
                                        : remotes[1] === null ? Match.RightSide : Match.NoSide
    readonly property var leftPad: mode === GameScene.TwoPlayers ? leftPadAssigned
                                   ?? (hostPadSide === Match.LeftSide ? primaryPad : null)
                                   : Gamepads.count > 0 ? Gamepads.pads[0] : null
    readonly property var rightPad: mode === GameScene.TwoPlayers ? rightPadAssigned
                                    ?? (hostPadSide === Match.RightSide ? primaryPad : null) : null
    // Gamepads work the pause menu and skip the replay, not the game
    menuNavigation: match.state === Match.Paused || match.state === Match.Finished || askLeave

    // What a pad button does, as set in the controls. Shoulders nothing is
    // set to dash as well.
    function padAction(button) {
        const action = KeySettings.padAction(button)
        if (action < 0 && (button === Gamepad.LeftShoulder || button === Gamepad.RightShoulder))
            return KeySettings.PadDash
        return action
    }

    function gamepadButton(pad, button, pressed) {
        const action = padAction(button)
        if (remote) {
            if (!pressed || pad !== Gamepads.pads[0])
                return
            if (action === KeySettings.PadPause)
                remotePause()
            else if (spectating)
                return
            else if (action === KeySettings.PadSpecial && running)
                Lan.sendToHost({ t: "action", a: "special" })
            else if (action === KeySettings.PadDash && running && Math.abs(pad.direction.y) > 0.3)
                Lan.sendToHost({ t: "action", a: "dash", d: pad.direction.y > 0 ? 1 : -1 })
            return
        }
        if (replaying) {
            if (pressed)
                endReplay()
            return
        }
        if (action === KeySettings.PadPause) {
            if (pressed && (running || match.state === Match.Paused))
                togglePause()
            return
        }

        const side = pad === leftPad ? Match.LeftSide : pad === rightPad ? Match.RightSide : Match.NoSide
        if (side === Match.NoSide || !running)
            return

        const player = side === Match.LeftSide ? match.left : match.right
        switch (action) {
            case KeySettings.PadSmash:
                setCharging(side, pressed)
                break
            case KeySettings.PadSpecial:
                if (pressed)
                    match.useSpecial(side)
                break
            case KeySettings.PadDash:
                // A dash the way the stick points
                if (pressed && !player.frozen && Math.abs(pad.direction.y) > 0.3)
                    (side === Match.LeftSide ? leftDash : rightDash)
                        .trigger((pad.direction.y > 0 ? 1 : -1) * (player.reversed ? -1 : 1))
                break
        }
    }

    Connections {
        target: Gamepads
        enabled: root.active
        function onButtonPressed(pad, button) { root.gamepadButton(pad, button, true) }
        function onButtonReleased(pad, button) { root.gamepadButton(pad, button, false) }
    }

    // Two players share this keyboard, each with a set of keys. Otherwise
    // both sets play the one paddle, also for a host against a player on
    // the network, and on the joined machine.
    readonly property bool sharedKeyboard: mode === GameScene.TwoPlayers && !remote
                                           && !remotes.some((remote) => remote !== null)

    // What's worth knowing besides the score: the stage, the best to beat,
    // watching, the time to the host and back. The keys are in the controls.
    readonly property string statusLine: {
        const parts = []
        if (spectating)
            parts.push(qsTr("Watching"))
        else if (mode === GameScene.Ladder)
            parts.push(qsTr("Ladder %1/3").arg(ladderStage + 1))
        else if (tournament)
            parts.push(qsTr("Tournament"))
        else if (squash)
            parts.push(qsTr("Best %1").arg(Math.max(match.longestRally, Stats.squashBest)))
        if (remote && Lan.latency >= 0)
            parts.push(qsTr("%1 ms").arg(Lan.latency))
        return parts.join("   ")
    }

    // Camera shake, applied through viewOffset
    property real shakeAmount: 0.0

    // Last whole second of the kickoff countdown, ticks when it changes
    property int kickoffSecond: 0

    // Smash wind up, 0 to 1, grows while the key is held
    readonly property real chargeTime: 0.6
    property bool leftCharging: false
    property bool rightCharging: false
    property real leftCharge: 0.0
    property real rightCharge: 0.0

    // The rally heats up the field
    readonly property real rallyHeat: Math.min(1.0, match.rally / 25)

    // A held ball slides along the paddle this much per second, and the
    // computer lets go once it held a ball this long
    readonly property real holdSlide: 2.0
    readonly property real computerHoldTime: 0.45

    // Slow motion when a ball is about to decide the match
    property real timeScale: 1.0
    slowMotion: 1.0 - timeScale

    // Moving obstacles follow the game time
    property real arenaTime: 0.0

    readonly property bool running: match.state === Match.Serving || match.state === Match.Playing

    property int currentPauseItem: 0
    // On the joined machine the host resumes, or one leaves
    readonly property var pauseItems: remote
        ? (askLeave ? [{ text: qsTr("Stay"), activate: () => root.askLeave = false },
                       { text: qsTr("Leave"), activate: () => root.leave() }]
           : match.state === Match.Paused && !spectating
             ? [{ text: qsTr("Resume"), activate: () => Lan.sendToHost({ t: "action", a: "resume" }) },
                { text: qsTr("Leave"), activate: () => root.leave() }]
           : [{ text: qsTr("Leave"), activate: () => root.leave() }])
          : [{ text: qsTr("Resume"), activate: () => match.resume() },
              { text: qsTr("Settings"), activate: () => phong.nextScene(root.settingsScene) },
              { text: qsTr("Menu"), activate: () => root.leave() }]

    function startMatch() {
        rules = GameSettings.rules(mode)
        // The shared scores want the server awake at the end
        if (endless || squash)
            OnlineService.check()
        remote = false
        rightByComputer = false
        remoteMoves = [0, 0]
        netEvents = []
        opponent = tournament ? Tournament.opponent : {}
        leftPaddleY = rightPaddleY = 0.0
        leftPaddleAngle = rightPaddleAngle = 0.0
        leftCharge = rightCharge = 0.0
        leftDash.reset()
        rightDash.reset()
        timeScale = 1.0
        arenaTime = 0.0
        newHighScore = false
        leftPerfects = leftDeficit = leftConceded = 0
        behind = [0, 0]
        comebackCalled = [false, false]
        lastSmash = Match.NoSide
        doubleSmashCalled = false
        replaying = false
        clearRecording()
        newAchievements = []
        releaseInput()
        chooseArena()
        buildBricks()
        resetBall()
        computer.reset()
        modifiers.reset()
        match.start()
        banner.show(bricks ? qsTr("Bricks") : squash ? qsTr("Squash") : arena.name ?? "", Theme.title)

        // The players on the network start with it
        lanGame = mode === GameScene.TwoPlayers && Lan.role === Lan.Host
        if (lanGame) {
            remotes.forEach((remote, side) => {
                if (remote)
                    sendStart(remote.id, side)
            })
            for (const peer of spectators)
                sendStart(peer, Match.NoSide)
            Lan.setInfo({ mode: "classic", players: 2, open: 0, playing: true })
        }
    }

    // Bricks and squash play on the empty field
    function chooseArena() {
        let id = rules.arena
        if (bricks || squash)
            id = "classic"
        else if (id === "random" || Arenas.arena(id).id === undefined)
            id = Arenas.randomId()
        arenaId = id
        arena = Arenas.arena(id)
        modifiers.obstacles = bricks ? [Qt.rect(-0.5 * brickWidth, -0.5 * stageHeight, brickWidth, stageHeight)]
                                     : Arenas.obstacleRects(id)
    }

    // A wall of bricks with a gap for the kickoff, every third brick drops
    // a modifier instead of giving a point
    function buildBricks() {
        brickModel.clear()
        if (!bricks)
            return

        for (const side of [1, -1]) {
            for (let i = 0; i < 4; ++i) {
                const item = rules.modifiers && Math.random() < 1 / 3
                brickModel.append({ brickId: nextBrickId++, brickY: side * (2.6 + 2.0 * i), item: item })
            }
        }
    }

    function brickPositions() {
        const positions = []
        for (let i = 0; i < brickModel.count; ++i)
            positions.push(Qt.vector2d(0, brickModel.get(i).brickY))
        return positions
    }

    // Removing a brick destroys its body, not while it reports
    function queueBrick(brickId, ball) {
        pendingBricks.push({ brickId: brickId, ball: ball })
        Qt.callLater(breakPending)
    }

    function breakPending() {
        const broken = pendingBricks
        pendingBricks = []
        for (const brick of broken) {
            let index = -1
            for (let i = 0; i < brickModel.count; ++i) {
                if (brickModel.get(i).brickId === brick.brickId)
                    index = i
            }
            if (index < 0)
                continue

            const entry = brickModel.get(index)
            const position = Qt.vector3d(0, entry.brickY, 0)
            const item = entry.item
            brickModel.remove(index)
            SoundEffects.play(SoundEffects.BrickBreak)
            sparks.burst(position.plus(Qt.vector3d(0, 0, 0.5)), item ? Theme.tint("#ffd24d") : Theme.block, 28)
            shake(0.25)

            // A point for the player who sent the ball, or a modifier
            const scorer = brick.ball.lastTouch
            if (item) {
                modifiers.spawnAt(Qt.vector2d(position.x, position.y))
            }
            else if (scorer !== Match.NoSide) {
                brickPopup.show("+1", scorer === Match.LeftSide ? Theme.leftPlayer : Theme.rightPlayer, position)
                match.awardPoint(scorer)
            }

            if (brickModel.count === 0) {
                banner.show(qsTr("Wall down"), Theme.title)
                if (scorer === Match.LeftSide)
                    root.achieve("demolition")
            }
        }
    }

    function leave() {
        // Nothing of the match that ended follows to the next screen
        resultsDelay.stop()
        replaying = false
        replayFrame = null
        match.stop()
        releaseInput()
        lanGame = false
        spectators = []
        departed = {}
        Lan.leave()
        remote = false
        // The tournament goes back to its bracket, the rest to where it
        // was chosen, also past the lobby
        if (tournament)
            phong.previousScene()
        else
            phong.returnToMenu()
    }

    function togglePause() {
        if (match.state === Match.Paused)
            match.resume()
        else
            match.pause()
    }

    function releaseInput() {
        leftUp = leftDown = rightUp = rightDown = false
        leftCharging = rightCharging = false
        leftPointerY = rightPointerY = NaN
        pointerSides = {}
    }

    function resetBall() {
        mainBall.reset(Qt.vector3d(0, 0, 0), Qt.vector3d(0, 0, 0))
        mainBall.setLinearVelocity(Qt.vector3d(0, 0, 0))
        mainBall.clearTrail()
    }

    // The camera leans a little towards the ball, never further than to
    // the edge of the field, wherever a ball may get to
    function lean() {
        const x = Math.max(-0.5 * stageWidth, Math.min(0.5 * stageWidth, mainBall.x || 0))
        const y = Math.max(-0.5 * stageHeight, Math.min(0.5 * stageHeight, mainBall.y || 0))
        return Qt.vector3d(0.07 * y, -0.05 * x, 0)
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

    // Winding up a smash slows the paddle down
    function movePaddle(y, input, length, charge, dt) {
        const limit = 0.5 * (innerHeight - length)
        return Math.max(-limit, Math.min(limit, y + input * paddleSpeed * (1.0 - 0.4 * charge) * dt))
    }

    // Charges while held, fades quickly when let go
    function windUp(charge, charging, dt) {
        return charging ? Math.min(1.0, charge + dt / chargeTime) : Math.max(0.0, charge - 3.0 * dt)
    }

    function obstaclePosition(obstacle, time) {
        const move = obstacle.move
        if (!move)
            return Qt.vector3d(obstacle.x, obstacle.y, 0)
        const swing = Math.sin(2.0 * Math.PI * (time / move.period + move.phase))
        return Qt.vector3d(obstacle.x + move.x * swing, obstacle.y + move.y * swing, 0)
    }

    // A ball about to decide the match: the goal it heads for wins it and
    // the defending paddle can't make it there in time any more
    function aboutToDecide() {
        if (!match.matchPoint || match.state !== Match.Playing)
            return false

        for (const body of ballBodies()) {
            const velocity = body.ball.velocity
            if (velocity.x === 0.0)
                continue

            const towardsRight = velocity.x > 0.0
            if (!match.winsWithNextPoint(towardsRight ? Match.LeftSide : Match.RightSide))
                continue

            const paddle = towardsRight ? rightPaddle : leftPaddle
            const length = towardsRight ? rightPaddleLength : leftPaddleLength
            const lineX = (towardsRight ? 1.0 : -1.0) * (paddleX - 0.5 * paddleWidth - ballRadius)
            const time = (lineX - body.x) / velocity.x
            if (time > 0.8)
                continue
            if (time < 0.0)
                return true

            const crossing = computer.predictCrossing(Qt.vector2d(body.x, body.y), velocity, lineX)
            const gap = Math.abs(crossing - paddle.y) - (0.5 * length + ballRadius)
            if (gap > time * paddleSpeed)
                return true
        }
        return false
    }

    // Each hit of a rally climbs a pentatonic scale, slower in slow motion
    function rallyPitch(rally) {
        const scale = [0, 2, 4, 7, 9]
        const step = Math.max(0, rally - 1)
        const semitones = Math.min(24, 12 * Math.floor(step / scale.length) + scale[step % scale.length])
        return Math.pow(2.0, semitones / 12.0) * (0.75 + 0.25 * timeScale)
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

    // The ball the paddle of side holds, or null
    function heldBody(side) {
        for (const body of ballBodies()) {
            if (body.ball.heldBy === side)
                return body
        }
        return null
    }

    // Throws the held ball with the wind up so far
    function releaseHeld(side) {
        const body = heldBody(side)
        if (body)
            match.releaseBall(body.ball, side === Match.LeftSide ? leftCharge : rightCharge)
    }

    // A held ball sits on the face of its paddle
    function placeHeld(body) {
        const left = body.ball.heldBy === Match.LeftSide
        const length = left ? leftPaddleLength : rightPaddleLength
        const x = (left ? -1 : 1) * (paddleX - 0.5 * paddleWidth - ballRadius - 0.05)
        const y = (left ? leftPaddleY : rightPaddleY) + body.ball.holdOffset * 0.5 * length
        body.reset(Qt.vector3d(x, y, 0), Qt.vector3d(0, 0, 0))
    }

    // A ball squeezed between the end of a paddle and a wall is pressed out
    // by the physics, through the wall at times, where it would fly on
    // above the goals forever. Next to a paddle it comes back in front of
    // it and the paddle sends it back, anywhere else inside the wall it
    // went through.
    function keepInField(body) {
        if (body.ball.heldBy !== Match.NoSide)
            return
        const limit = ballLimit
        const outside = Math.abs(body.y) > limit + 0.1
        for (const side of [Match.LeftSide, Match.RightSide]) {
            const left = side === Match.LeftSide
            const x = left ? -paddleX : paddleX
            const y = left ? leftPaddleY : rightPaddleY
            const length = left ? leftPaddleLength : rightPaddleLength
            const across = Math.abs(body.x - x)
            const beside = across < 0.5 * paddleWidth + ballRadius
            // Into the paddle at the wall, deeper than a touch of its face
            const pressed = across < 0.5 * paddleWidth + ballRadius - 0.15 && Math.abs(body.y) > limit - ballRadius
                            && Math.abs(body.y - y) < 0.5 * length + ballRadius - 0.1
            if (!(beside && outside) && !pressed)
                continue
            const front = x + (left ? 1 : -1) * (0.5 * paddleWidth + ballRadius + 0.1)
            body.reset(Qt.vector3d(front, Math.max(-limit, Math.min(limit, body.y)), 0), Qt.vector3d(0, 0, 0))
            match.deflect(body.ball, side, Qt.vector2d(left ? 1 : -1, 0))
            body.applyVelocity()
            body.clearTrail()
            return
        }
        if (outside) {
            const top = body.y > 0
            body.reset(Qt.vector3d(body.x, top ? limit : -limit, 0), Qt.vector3d(0, 0, 0))
            match.wallHit(body.ball, top)
            body.applyVelocity()
            body.clearTrail()
            return
        }
        // Stopped at a wall while the rules send it away, the engine lost
        // the velocity in the squeeze and would slide it along the wall
        const away = body.y > 0 ? body.ball.velocity.y < 0 : body.ball.velocity.y > 0
        if (Math.abs(body.y) > limit - 0.02 && away)
            body.applyVelocity()
    }

    // Keys slide the held ball along the paddle, a pointer puts it where it is
    function aimHeld(body, keys, pointerY, paddleY, length, dt) {
        const offset = keys !== 0 || isNaN(pointerY) ? body.ball.holdOffset + keys * holdSlide * dt
                                                     : (pointerY - paddleY) / (0.5 * length)
        match.aimHeldBall(body.ball, offset)
    }

    // The computer aims through a target and lets go
    function computerHold(body, dt) {
        const offset = body.ball.holdOffset
        const slide = holdSlide * dt
        const aim = computer.holdAim(body.y)
        match.aimHeldBall(body.ball, offset + Math.max(-slide, Math.min(slide, aim - offset)))
        if (body.ball.holdTime <= match.maxHoldTime - computerHoldTime)
            releaseHeld(Match.RightSide)
    }

    function keyInput(up, down) {
        return (up ? 1 : 0) - (down ? 1 : 0)
    }

    // Upright paddles use the arcade bounce, turned ones reflect the ball
    // off their surface. A magnetic paddle catches the ball.
    function paddleContact(body, side, paddle, angle, length, velocity, normals) {
        if (angle % 180 === 0 || normals.length === 0) {
            const smash = side === Match.LeftSide ? leftCharge : rightCharge
            const offset = (body.y - paddle.y) / (0.5 * length + ballRadius)
            const player = side === Match.LeftSide ? match.left : match.right
            if (player.catches > 0 && match.catchBall(body.ball, side, offset))
                return
            match.paddleHit(body.ball, side, offset, velocity, smash)
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
                effect({ e: "wall" })
        }
        else if (other === leftShield || other === rightShield) {
            if (modifiers.shieldHit(body.ball, other === leftShield ? Match.LeftSide : Match.RightSide)) {
                effect({ e: "shield", x: body.x, y: body.y })
                callout(qsTr("What a save!"))
            }
        }
        else if (other === squashWall) {
            if (match.bounce(body.ball, normal)) {
                squashWall.flash()
                SoundEffects.play(SoundEffects.WallHit, 0.8)
                sparks.burst(Qt.vector3d(body.x, body.y, 0.5), Theme.wall, 10)
            }
        }
        else if (other.brick === true) {
            if (match.bounce(body.ball, normal))
                queueBrick(other.brickId, body.ball)
        }
        else if (other.obstacle === true) {
            if (match.bounce(body.ball, normal)) {
                other.flash()
                effect({ e: "bounce", x: body.x, y: body.y })
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

    // Teleporting resets the body, not while the triggers report
    function queuePortal(body, index) {
        pendingPortals.push({ body: body, index: index })
        Qt.callLater(teleportPending)
    }

    // The ball comes out of the other portal, flying on as it was
    function teleportPending() {
        const passages = pendingPortals
        pendingPortals = []
        const portals = modifiers.portals
        for (const passage of passages) {
            const body = passage.body
            if (portals.length !== 2 || body.portalLock === passage.index || body.ball.heldBy !== Match.NoSide)
                continue

            const from = portals[passage.index]
            const to = portals[1 - passage.index]
            body.portalLock = 1 - passage.index
            body.reset(Qt.vector3d(to.x, to.y, 0), Qt.vector3d(0, 0, 0))
            body.applyVelocity()
            body.clearTrail()
            effect({ e: "portal", index: passage.index, from: [from.x, from.y], to: [to.x, to.y] })
        }
    }

    readonly property var portalColors: ["#ff9933", "#3399ff"]

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

    // Called after every physics step, dt in real seconds
    function step(realDt) {
        // Ease in and out of the slow motion
        const slow = aboutToDecide() ? 0.3 : 1.0
        timeScale += (slow - timeScale) * Math.min(1.0, 8.0 * realDt)
        const dt = realDt * timeScale

        // A held ball about to go is thrown with the wind up so far
        for (const body of ballBodies()) {
            if (body.ball.heldBy !== Match.NoSide && body.ball.holdTime <= dt)
                releaseHeld(body.ball.heldBy)
        }

        // The ball waits for the kickoff on the serve spot. A reset while
        // the physics stood still, after the last point of a match, is lost,
        // and the ball would start where it ended, e.g. behind the wall of
        // squash.
        if (match.state === Match.Serving && (Math.abs(mainBall.x) > 0.01 || Math.abs(mainBall.y) > 0.01))
            resetBall()

        match.advance(dt)
        if (!running)
            return

        modifiers.advance(dt)
        leftDash.advance(dt)
        rightDash.advance(dt)

        const onePlayer = !sharedKeyboard
        // Reversed controls swap the keys and mirror the pointer
        const leftSign = match.left.reversed ? -1 : 1
        const rightSign = match.right.reversed ? -1 : 1
        const leftKeys = leftSign * (onePlayer ? keyInput(leftUp || rightUp, leftDown || rightDown)
                                               : keyInput(leftUp, leftDown))
        let leftInput = humanInput(leftKeys > 0, leftKeys < 0, leftSign * leftPointerY, leftPaddleY)
        // A stick moves the paddle as far as it is tilted, a player on the
        // network as far as they want
        const leftStick = remotes[0] ? leftSign * remoteMoves[0]
                        : leftKeys === 0 && leftPad ? leftSign * leftPad.direction.y : 0
        if (leftStick !== 0 || remotes[0])
            leftInput = leftStick

        let rightInput
        const rightKeys = onePlayer ? 0 : rightSign * keyInput(rightUp, rightDown)
        const leftHeld = heldBody(Match.LeftSide)
        const rightHeld = heldBody(Match.RightSide)
        if (againstComputer || rightByComputer) {
            const urgent = urgentBall()
            // Bricks give points, worth aiming at
            computer.targets = bricks ? modifiers.itemPositions().concat(brickPositions()) : modifiers.itemPositions()
            computer.opponentY = leftPaddleY
            // Curses get to the computer too
            computer.confusion = match.right.reversed ? 1.0 : 0.0
            computer.blind = modifiers.ghostBall && Math.abs(urgent.x) < ghostHalfWidth
            computer.update(dt, Qt.vector2d(urgent.x, urgent.y), urgent.ball.velocity, rightPaddleY)
            rightInput = computer.direction
            if (computer.wantsDash && !rightHeld && !match.right.frozen)
                rightDash.trigger(computer.direction >= 0 ? 1 : -1)
            // The computer spends a full power bar right away
            if (match.right.power >= 1.0 && match.right.catches === 0)
                match.useSpecial(Match.RightSide)
        }
        else if (squash) {
            rightInput = 0
        }
        else if (remotes[1]) {
            rightInput = rightSign * remoteMoves[1]
        }
        else {
            rightInput = humanInput(rightKeys > 0, rightKeys < 0, rightSign * rightPointerY, rightPaddleY)
            const rightStick = rightKeys === 0 && rightPad ? rightSign * rightPad.direction.y : 0
            if (rightStick !== 0)
                rightInput = rightStick
        }

        // A paddle holding a ball stands, the input aims the ball
        if (leftHeld) {
            aimHeld(leftHeld, leftKeys || leftStick, leftPointerY, leftPaddleY, leftPaddleLength, dt)
            leftInput = 0
        }
        if (rightHeld) {
            if (againstComputer || rightByComputer)
                computerHold(rightHeld, dt)
            else
                aimHeld(rightHeld, remotes[1] ? rightSign * remoteMoves[1]
                                   : rightKeys || (rightPad ? rightSign * rightPad.direction.y : 0), rightPointerY,
                        rightPaddleY, rightPaddleLength, dt)
            rightInput = 0
        }

        // A dash takes over the paddle for a moment
        if (leftDash.active && !leftHeld)
            leftInput = leftDash.direction * leftDash.boost
        if (rightDash.active && !rightHeld)
            rightInput = rightDash.direction * rightDash.boost

        // Nothing moves a frozen paddle
        if (match.left.frozen)
            leftInput = 0
        if (match.right.frozen)
            rightInput = 0

        // A gravity well bends the flights
        if (modifiers.gravityStrength > 0.0) {
            for (const body of ballBodies())
                match.attract(body.ball, Qt.vector2d(body.x, body.y), modifiers.gravityWell,
                              modifiers.gravityStrength, dt)
        }

        leftCharge = windUp(leftCharge, leftCharging, dt)
        rightCharge = windUp(rightCharge, againstComputer || rightByComputer ? computer.charging : rightCharging, dt)
        arenaTime += dt

        const leftY = movePaddle(leftPaddleY, leftInput, leftPaddleLength, leftCharge, dt)
        const rightY = movePaddle(rightPaddleY, rightInput, rightPaddleLength, rightCharge, dt)
        leftPaddleVelocity = (leftY - leftPaddleY) / dt
        rightPaddleVelocity = (rightY - rightPaddleY) / dt
        leftPaddleY = leftY
        rightPaddleY = rightY
        leftPaddleAngle = turnPaddle(leftPaddleAngle, match.left.spinSpeed, dt)
        rightPaddleAngle = turnPaddle(rightPaddleAngle, match.right.spinSpeed, dt)

        for (const body of ballBodies()) {
            if (body.ball.heldBy !== Match.NoSide)
                placeHeld(body)
            else
                keepInField(body)
            body.hidden = modifiers.ghostBall && match.state === Match.Playing && Math.abs(body.x) < ghostHalfWidth
            body.advance(dt)
        }

        // The camera shakes on impacts and leans a little towards the ball
        shakeAmount *= Math.exp(-7.0 * realDt)
        // and moves in closer in slow motion
        viewOffset = Qt.vector3d((Math.random() - 0.5) * shakeAmount,
                                 (Math.random() - 0.5) * shakeAmount, -6.0 * slowMotion)
        const follow = Math.min(1.0, 3.0 * realDt)
        viewRotation = viewRotation.plus(lean().minus(viewRotation).times(follow))

        record(realDt)
        sendState()
    }

    // Letting go of the smash key throws a held ball
    function setCharging(side, pressed) {
        if (side === Match.LeftSide) {
            if (!pressed && leftCharging)
                releaseHeld(side)
            leftCharging = pressed
        }
        else {
            if (!pressed && rightCharging)
                releaseHeld(side)
            rightCharging = pressed
        }
    }

    // The keys come from the controls settings. Against the computer both
    // sets move your paddle, and space smashes too.
    function setKey(key, pressed) {
        const twoPlayers = sharedKeyboard
        if (key === Qt.Key_Space && !twoPlayers) {
            setCharging(Match.LeftSide, pressed)
            return true
        }

        const right = twoPlayers ? Match.RightSide : Match.LeftSide
        switch (KeySettings.action(key)) {
            case KeySettings.LeftUp: leftUp = pressed; return true
            case KeySettings.LeftDown: leftDown = pressed; return true
            case KeySettings.RightUp: rightUp = pressed; return true
            case KeySettings.RightDown: rightDown = pressed; return true
            case KeySettings.LeftSmash: setCharging(Match.LeftSide, pressed); return true
            case KeySettings.RightSmash: setCharging(right, pressed); return true
            // The special, when the power bar is full
            case KeySettings.LeftSpecial:
                if (pressed)
                    match.useSpecial(Match.LeftSide)
                return true
            case KeySettings.RightSpecial:
                if (pressed)
                    match.useSpecial(right)
                return true
        }
        return false
    }

    // Tapping a direction twice dashes, not when frozen
    function tapDash(key) {
        const twoPlayers = sharedKeyboard
        const upDown = twoPlayers ? rightDash : leftDash
        const left = match.left.frozen ? 0 : match.left.reversed ? -1 : 1
        const right = twoPlayers ? (match.right.frozen ? 0 : match.right.reversed ? -1 : 1) : left
        switch (KeySettings.action(key)) {
            case KeySettings.LeftUp: return leftDash.tap(left)
            case KeySettings.LeftDown: return leftDash.tap(-left)
            case KeySettings.RightUp: return upDown.tap(right)
            case KeySettings.RightDown: return upDown.tap(-right)
        }
        return false
    }

    function pointerSide(x) {
        // On the joined machine anywhere steers the own paddle
        if (remote)
            return localSide
        return !sharedKeyboard || x < 0 ? Match.LeftSide : Match.RightSide
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
            resultsDelay.stop()
            releaseInput()
            viewOffset = viewRotation = Qt.vector3d(0, 0, 0)
        }
    }

    // Others on the network play on
    onFocusLost: {
        releaseInput()
        if (!hostingLan && !remote)
            match.pause()
    }

    onKeyPressed: (event) => {
        event.accepted = true
        if (event.isAutoRepeat)
            return

        // Any key skips the replay, and the moment after it
        if (replaying) {
            endReplay()
            return
        }
        if (resultsDelay.running) {
            resultsDelay.stop()
            phong.showResults()
            return
        }

        if (remote) {
            if (askLeave || match.state === Match.Paused) {
                switch (event.key) {
                    case Qt.Key_Escape:
                        if (askLeave)
                            askLeave = false
                        else if (spectating)
                            leave()
                        else
                            remotePause()
                        break
                    case Qt.Key_Up:
                    case Qt.Key_Down:
                        currentPauseItem = event.key === Qt.Key_Up ? 0 : pauseItems.length - 1
                        SoundEffects.play(SoundEffects.MenuMove)
                        break
                    case Qt.Key_Enter:
                    case Qt.Key_Return:
                        SoundEffects.play(SoundEffects.MenuSelect)
                        pauseItems[Math.min(currentPauseItem, pauseItems.length - 1)].activate()
                        break
                }
                return
            }
            if (event.key === Qt.Key_Escape || KeySettings.action(event.key) === KeySettings.Pause) {
                remotePause()
                return
            }
            remoteKey(event.key, true)
            return
        }

        if (match.state === Match.Paused) {
            switch (event.key) {
                // Esc opened the pause menu, Esc closes it again
                case Qt.Key_Escape:
                    SoundEffects.play(SoundEffects.MenuSelect)
                    match.resume()
                    return
                case Qt.Key_Up:
                    currentPauseItem = Math.max(currentPauseItem - 1, 0)
                    SoundEffects.play(SoundEffects.MenuMove)
                    return
                case Qt.Key_Down:
                    currentPauseItem = Math.min(currentPauseItem + 1, pauseItems.length - 1)
                    SoundEffects.play(SoundEffects.MenuMove)
                    return
                case Qt.Key_Enter:
                case Qt.Key_Return:
                    SoundEffects.play(SoundEffects.MenuSelect)
                    pauseItems[currentPauseItem].activate()
                    return
            }
        }

        if (running)
            tapDash(event.key)
        if (setKey(event.key, true))
            return

        switch (event.key) {
            case Qt.Key_Escape:
                if (running)
                    match.pause()
                else if (match.state !== Match.Paused)
                    leave()
                break
            case Qt.Key_Space:
                togglePause()
                break
            default:
                if (KeySettings.action(event.key) === KeySettings.Pause)
                    togglePause()
                break
        }
    }

    onKeyReleased: (event) => {
        event.accepted = true
        if (event.isAutoRepeat)
            return
        if (remote)
            remoteKey(event.key, false)
        else
            setKey(event.key, false)
    }

    onPointerPressed: (id, x, y) => {
        if (replaying) {
            endReplay()
            return
        }
        if (resultsDelay.running) {
            resultsDelay.stop()
            phong.showResults()
            return
        }

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

        endless: root.solo
        pointsToWin: root.solo ? root.lives : root.rules.pointsToWin
        setsToWin: root.mode === GameScene.Ladder || root.solo ? 1 : root.rules.setsToWin
        winByTwo: root.rules.winByTwo && !root.solo
        serveSpeed: root.rules.serveSpeed * root.endlessSpeedUp
        maxSpeed: root.rules.maxSpeed
        paddleSpeed: root.paddleSpeed
        serveDelay: root.rules.kickoffTime

        // On the LAN the machines' names
        left.name: root.remotes[0]?.name ?? (root.hostingLan ? Lan.localName
                   : root.mode !== GameScene.TwoPlayers ? qsTr("You") : qsTr("Ping"))
        right.name: root.remotes[1]?.name ?? (root.hostingLan ? Lan.localName
                    : root.tournament ? root.opponent.name ?? ""
                    : root.squash ? qsTr("Wall")
                    : root.againstComputer ? qsTr("CPU") : qsTr("Pong"))
        right.computer: root.againstComputer

        // The replay shows the deciding rally only
        onServed: root.effect({ e: "serve" })

        onServeCountdownChanged: {
            const second = Math.ceil(match.serveCountdown)
            if (second > 0 && second !== root.kickoffSecond)
                SoundEffects.play(SoundEffects.CountdownTick)
            root.kickoffSecond = second
        }

        onPaddleHitBall: (ball, side, smash, perfect) => {
            root.effect({ e: "hit", side: side, smash: smash, perfect: perfect,
                          speed: ball.velocity.length() / match.serveSpeed })
            if (perfect && side === Match.LeftSide && ++root.leftPerfects >= 5)
                root.achieve("perfectionist")

            // A hard smash returned with a hard smash, once a rally
            if (smash >= 0.5) {
                if (root.lastSmash === (side === Match.LeftSide ? Match.RightSide : Match.LeftSide)
                    && !root.doubleSmashCalled) {
                    root.doubleSmashCalled = true
                    root.callout(qsTr("Double smash!"))
                }
                root.lastSmash = side
            }
            else {
                root.lastSmash = Match.NoSide
            }

            // The smash is spent
            if (side === Match.LeftSide)
                root.leftCharge = 0.0
            else
                root.rightCharge = 0.0
        }

        onSpecialUsed: (side) => root.effect({ e: "special", side: side })
        onBallCaught: (ball, side) => root.effect({ e: "catch", side: side })

        // Every fifth hit of a rally gets a louder cheer
        onRallyChanged: {
            const rally = match.rally
            if (rally >= 20 && root.arenaId === "elevators")
                root.achieve("goingUp")
            if (rally >= 25 && root.squash)
                root.achieve("squashPro")
            if (rally < 5 || rally % 5 !== 0)
                return
            const level = Math.min(4, rally / 5)
            SoundEffects.play(SoundEffects.RallyMilestone, 1.0 + 0.12 * (level - 1))
            banner.show(qsTr("Rally %1").arg(rally) + "!".repeat(level - 1),
                        level < 2 ? Theme.text : level < 3 ? Theme.ball : Theme.title, 1.0 + 0.25 * level)
        }

        onMatchPointChanged: {
            if (match.matchPoint && match.state !== Match.Finished && !root.solo)
                banner.show(qsTr("Match point"), Theme.accent, 1.3)
        }

        onPointScored: (scorer, ball) => {
            root.countPoint(scorer)
            if (scorer === Match.LeftSide && ball.smashed)
                root.achieve("smashGoal")
            root.effect({ e: "goal", scorer: scorer })

            if (!ball.extra) {
                root.resetBall()
                computer.reset()
            }
        }

        // Not for the set that wins the match, the results say that
        onSetFinished: (winner) => {
            root.behind = [0, 0]
            root.comebackCalled = [false, false]
            const player = winner === Match.LeftSide ? match.left : match.right
            if (player.sets < match.setsToWin)
                root.effect({ e: "set", name: player.name })
        }

        // Every pause starts on resume, a wall down is built anew for
        // the next kickoff
        onStateChanged: {
            if (match.state === Match.Paused) {
                root.pausedBy = root.pauser !== "" ? root.pauser : root.lanGame ? Lan.localName : ""
                root.pauser = ""
                root.currentPauseItem = 0
            }
            // Pauses and the end get to the network too, no steps run then
            root.sendState()
            if (match.state === Match.Serving && root.bricks && brickModel.count === 0)
                root.buildBricks()
        }

        // A point for a brick can end the set, the ball goes back then
        onPointAwarded: (scorer) => {
            root.countPoint(scorer)
            if (match.state !== Match.Playing) {
                root.resetBall()
                computer.reset()
            }
        }

        onFinished: {
            if (root.remote)
                return
            root.effect({ e: "finished" })
            root.sendState()
            const won = match.winner === match.left
            SoundEffects.play(root.againstComputer && !won ? SoundEffects.Lose : SoundEffects.Win)
            if (root.endless)
                root.newHighScore = Stats.recordEndless(root.endlessScore)
            else if (root.squash)
                root.newHighScore = Stats.recordSquash(match.longestRally)
            // On the list everybody shares, with a name
            if (root.endless || root.squash) {
                const board = root.endless ? "endless" : "squash"
                const score = root.endless ? root.endlessScore : match.longestRally
                if (GameSettings.playerName !== "" && score > 0)
                    HighScores.submit(board, GameSettings.playerName, score)
                else
                    HighScores.refresh(board)
            }
            else
                Stats.recordMatch(root.againstComputer, root.difficulty, won, match.longestRally)
            if (root.mode === GameScene.Ladder && won)
                Stats.recordLadder(root.ladderStage + 1)
            if (root.tournament) {
                Tournament.recordResult(won)
                if (Tournament.champion) {
                    Stats.recordTournamentWin()
                    root.achieve("champion")
                }
            }

            if (won && root.againstComputer && !root.solo) {
                root.achieve("firstWin")
                if (root.difficulty === ComputerPlayer.Hard)
                    root.achieve("beatHard")
                if (root.leftConceded === 0)
                    root.achieve("shutout")
                if (root.leftDeficit >= 3)
                    root.achieve("comeback")
                if (!root.rules.modifiers)
                    root.achieve("purist")
            }

            // The deciding rally once more, then the results
            root.startReplay()
        }
    }

    Dash {
        id: leftDash
        onDashed: root.dashed(leftPaddle)
    }

    Dash {
        id: rightDash
        onDashed: root.dashed(rightPaddle)
    }

    function dashed(paddle) {
        effect({ e: "dash", side: paddle === leftPaddle ? Match.LeftSide : Match.RightSide })
    }

    onEndlessScoreChanged: {
        if (endless && endlessScore >= 100)
            achieve("endurance")
    }

    // The music plays during the match, busier and faster with the rally
    // and slower in slow motion
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
        value: match.state === Match.Playing ? 1 + Math.min(3, Math.floor(match.rally / 4)) : 0
    }

    Binding {
        target: SoundEffects
        property: "musicTempo"
        when: root.active
        value: (112 + Math.min(40, 1.5 * match.rally)) * (0.6 + 0.4 * root.timeScale)
    }

    Connections {
        target: Stats
        enabled: root.active
        function onAchievementUnlocked(name) {
            root.newAchievements = root.newAchievements.concat([name])
            SoundEffects.play(SoundEffects.Achievement)
            achievementBanner.show(qsTr("Achievement: %1").arg(name), Theme.title, 0.8)
        }
    }

    ComputerPlayer {
        id: computer

        difficulty: root.difficulty
        personality: root.tournament ? root.opponent.personality ?? ComputerPlayer.Balanced : ComputerPlayer.Balanced
        paddleSpeed: root.paddleSpeed
        paddleX: root.paddleX - 0.5 * root.paddleWidth - root.ballRadius
        paddleReach: 0.5 * root.rightPaddleLength + root.ballRadius
        fieldTop: root.ballLimit
        fieldBottom: -root.ballLimit
    }

    Modifiers {
        id: modifiers

        match: match
        enabled: root.rules.modifiers && !root.squash
        // Inside the narrowest field, clear of the paddles
        readonly property real spawnHeight: root.innerHeight + 2.0 * root.fieldInset
                                            - 2.0 * maxFieldInset - 3.0
        spawnArea: Qt.rect(-8.0, -0.5 * spawnHeight, 16.0, spawnHeight)

        onCollected: (index, side, position) => {
            root.effect({ e: "pickup", def: modifiers.definition(index).id, x: position.x, y: position.y })
        }
        onEffectsChanged: {
            root.leftEffects = activeEffects(Match.LeftSide)
            root.rightEffects = activeEffects(Match.RightSide)
        }
    }

    // A moment after the replay, then the results, unless the game was
    // left meanwhile
    Timer {
        id: resultsDelay
        interval: 900
        onTriggered: {
            if (root.active)
                root.phong.showResults()
        }
    }

    PhysicsWorld {
        scene: root
        running: root.active && root.running && !root.remote
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
                glow: 0.6 + 1.2 * root.rallyHeat
                lighting: DefaultMaterial.NoLighting
            }
        }
    }

    // Soft shadows on the floor, the key light comes from above in front
    Node {
        visible: GraphicsSettings.floor && GraphicsSettings.shadows && !root.replaying
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
            opacity: 0.45 * mainBall.visibility
        }

        Repeater3D {
            model: match.extraBalls

            delegate: Shadow {
                required property int index
                readonly property var body: extraBalls.objectAt(index)
                position: body ? Qt.vector3d(body.x + 0.3, body.y - 0.6, 0) : Qt.vector3d(0, 0, 0)
                size: 2.2 * root.ballRadius
                opacity: body ? 0.45 * body.visibility : 0.0
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
                color: Qt.tint(Theme.wall, Qt.rgba(Theme.title.r, Theme.title.g, Theme.title.b, 0.6 * root.rallyHeat))
                glow: 0.35 + 0.8 * root.rallyHeat
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
                color: Qt.tint(Theme.wall, Qt.rgba(Theme.title.r, Theme.title.g, Theme.title.b, 0.6 * root.rallyHeat))
                glow: 0.35 + 0.8 * root.rallyHeat
                shininess: 0.5
            }
        }
    }

    // Arena obstacles, round bumpers and blocks
    Repeater3D {
        model: root.arena.bumpers ?? []

        delegate: DynamicRigidBody {
            id: bumper

            required property var modelData
            readonly property bool obstacle: true
            property real glow: 0.0

            function flash() {
                bumperGlow.restart()
            }

            // Kinematic, some move on a path
            isKinematic: true
            position: kinematicPosition
            kinematicPosition: root.obstaclePosition(modelData, root.arenaTime)
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

        delegate: DynamicRigidBody {
            id: block

            required property var modelData
            readonly property bool obstacle: true
            property real glow: 0.0

            function flash() {
                blockGlow.restart()
            }

            isKinematic: true
            position: kinematicPosition
            kinematicPosition: root.obstaclePosition(modelData, root.arenaTime)
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
        visible: !root.replaying
        color: Theme.leftPlayer
        charge: root.leftCharge
        dash: root.remote ? root.remotePaddles[6] : leftDash.direction
        dashCooldown: root.remote ? root.remotePaddles[8] : leftDash.cooldown
        magnet: match.left.catches > 0
        frozen: match.left.frozen
        reversed: match.left.reversed
        paddleX: -root.paddleX
        paddleY: root.leftPaddleY
        angle: root.leftPaddleAngle
        length: root.leftPaddleLength
        width: root.paddleWidth
    }

    PaddleBody {
        id: rightPaddle
        // Squash has a wall instead
        visible: !root.squash && !root.replaying
        color: Theme.rightPlayer
        charge: root.rightCharge
        dash: root.remote ? root.remotePaddles[7] : rightDash.direction
        dashCooldown: root.remote ? root.remotePaddles[9] : rightDash.cooldown
        magnet: match.right.catches > 0
        frozen: match.right.frozen
        reversed: match.right.reversed
        paddleX: root.squash ? 1000 : root.paddleX
        paddleY: root.rightPaddleY
        angle: root.rightPaddleAngle
        length: root.rightPaddleLength
        width: root.paddleWidth
    }

    // The front wall of squash, parked far away otherwise
    DynamicRigidBody {
        id: squashWall

        property real glow: 0.0
        function flash() {
            squashWallGlow.restart()
        }

        isKinematic: true
        position: kinematicPosition
        kinematicPosition: Qt.vector3d(root.paddleX, root.squash ? 0 : 1000, 0)
        physicsMaterial: bouncy
        sendContactReports: true
        collisionShapes: BoxShape {
            extents: Qt.vector3d(root.paddleWidth, root.innerHeight, 1.0)
        }

        Model {
            visible: root.squash
            source: "#Cube"
            scale: Qt.vector3d(root.paddleWidth / 100, root.innerHeight / 100, 0.01)
            materials: PhongMaterial {
                color: Qt.tint(Theme.wall, Qt.rgba(1, 1, 1, 0.6 * squashWall.glow))
                glow: 0.35 + 0.8 * root.rallyHeat + squashWall.glow
                shininess: 0.5
            }
        }

        NumberAnimation {
            id: squashWallGlow
            target: squashWall
            property: "glow"
            from: 1.0
            to: 0.0
            duration: 250
            easing.type: Easing.OutQuad
        }
    }

    ListModel {
        id: brickModel
    }

    Repeater3D {
        model: brickModel

        delegate: DynamicRigidBody {
            id: brickBody

            required property int brickId
            required property real brickY
            required property bool item
            readonly property bool brick: true
            readonly property color color: item ? Theme.tint("#ffd24d") : Theme.block

            // A narrowed field hides the outer ones behind the walls
            visible: Math.abs(brickY) < root.wallY - 0.5
            isKinematic: true
            position: kinematicPosition
            kinematicPosition: Qt.vector3d(0, brickY, 0)
            physicsMaterial: bouncy
            sendContactReports: true
            collisionShapes: BoxShape {
                extents: Qt.vector3d(root.brickWidth, root.brickHeight, 1.0)
            }

            Model {
                source: "#Cube"
                scale: Qt.vector3d(root.brickWidth / 100, (root.brickHeight - 0.1) / 100, 0.01)
                materials: PhongMaterial {
                    color: brickBody.color
                    glow: brickBody.item ? 0.6 : 0.15
                    shininess: 0.5
                }
            }

            // Bricks with a modifier inside say so
            Text3D {
                visible: brickBody.item
                z: 0.5
                scale: Qt.vector3d(0.7, 0.7, 0.7)
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                depth: 0.2
                color: Theme.background
                text: "?"
            }

            Vector3dAnimation on scale {
                from: Qt.vector3d(0, 0, 0)
                to: Qt.vector3d(1, 1, 1)
                duration: 300
                easing.type: Easing.OutBack
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
        visible: !root.replaying
        ball: match.ball
        timeScale: root.timeScale
        radius: root.ballRadius
        trailSpeed: match.serveSpeed
        onContact: (body, normals) => root.ballContact(mainBall, body, normals)
    }

    Repeater3D {
        id: extraBalls
        model: match.extraBalls

        delegate: BallBody {
            id: extraBall
            visible: !root.replaying
            timeScale: root.timeScale
            radius: root.ballRadius
            trailSpeed: match.serveSpeed
            onContact: (body, normals) => root.ballContact(extraBall, body, normals)
        }
    }

    // Two linked portals, a ball flying into one comes out of the other
    Repeater3D {
        model: modifiers.portals

        delegate: TriggerBody {
            id: portal

            required property var modelData
            required property int index
            readonly property color color: Theme.tint(root.portalColors[index])

            position: Qt.vector3d(modelData.x, modelData.y, 0)
            collisionShapes: SphereShape {
                diameter: 1.6
            }
            onBodyEntered: (body) => {
                const ballBody = body as BallBody
                if (ballBody)
                    root.queuePortal(ballBody, portal.index)
            }
            onBodyExited: (body) => {
                const ballBody = body as BallBody
                if (ballBody && ballBody.portalLock === portal.index)
                    ballBody.portalLock = -1
            }

            Disc {
                z: -0.3
                radius: 1.25
                thickness: 0.2
                color: portal.color
                glow: 0.9
            }

            Disc {
                z: -0.15
                radius: 0.95
                thickness: 0.2
                color: Theme.background
                glow: 0.0
                shininess: 0.0
            }

            // Sparks circling inside
            Node {
                NumberAnimation on eulerRotation.z {
                    from: portal.index ? 360 : 0
                    to: portal.index ? 0 : 360
                    duration: 1500
                    loops: Animation.Infinite
                }

                Discs {
                    radius: 0.12
                    thickness: 0.1
                    items: {
                        const items = []
                        for (let index = 0; index < 6; ++index) {
                            const angle = index * Math.PI / 3
                            items.push({ x: 0.7 * Math.cos(angle), y: 0.7 * Math.sin(angle), color: portal.color, glow: 1.0 })
                        }
                        return items
                    }
                }
            }

            Vector3dAnimation on scale {
                from: Qt.vector3d(0, 0, 0)
                to: Qt.vector3d(1, 1, 1)
                duration: 300
                easing.type: Easing.OutBack
            }
        }
    }

    // A gravity well, arms of dots spiralling into a dark core
    Node {
        id: gravityWell

        readonly property color color: Theme.tint("#7a5cff")

        visible: modifiers.gravityStrength > 0.0
        position: Qt.vector3d(modifiers.gravityWell.x, modifiers.gravityWell.y, -0.2)

        Disc {
            radius: 0.6
            sphere: true
            color: Theme.background
            glow: 0.0
            shininess: 1.0
        }

        Node {
            NumberAnimation on eulerRotation.z {
                running: gravityWell.visible
                from: 360
                to: 0
                duration: 2000
                loops: Animation.Infinite
            }

            Discs {
                radius: 0.16
                thickness: 0.1
                items: {
                    // Three arms, each dot a bit further out, further round
                    // and smaller
                    const items = []
                    for (let index = 0; index < 18; ++index) {
                        const arm = index % 3
                        const step = Math.floor(index / 3)
                        const angle = arm * 2.0 * Math.PI / 3 + step * 0.45
                        const distance = 0.9 + step * 0.45
                        items.push({ x: distance * Math.cos(angle), y: distance * Math.sin(angle),
                                     scale: 1.0 - 0.09375 * step, color: gravityWell.color, glow: 1.0 - 0.12 * step })
                    }
                    return items
                }
            }
        }
    }

    // The fog a ghost ball disappears in, dark with glowing edges
    Node {
        visible: modifiers.ghostBall
        z: -0.4

        Model {
            source: "#Rectangle"
            scale: Qt.vector3d(2.0 * root.ghostHalfWidth / 100, 2.0 * root.wallY / 100, 1)
            opacity: 0.8
            materials: DefaultMaterial {
                diffuseColor: Theme.background
                lighting: DefaultMaterial.NoLighting
            }
        }

        Repeater3D {
            model: [-1, 1]

            delegate: Model {
                required property int modelData
                x: modelData * root.ghostHalfWidth
                source: "#Cube"
                scale: Qt.vector3d(0.08 / 100, 2.0 * root.wallY / 100, 0.001)
                materials: PhongMaterial {
                    color: Theme.dimmed
                    glow: 0.6
                    lighting: DefaultMaterial.NoLighting
                }
            }
        }
    }

    Sparks {
        id: sparks
    }

    // Instant replay of the deciding rally, from the recorded positions
    FrameAnimation {
        running: root.replaying
        // The first frame after a start may report the time since the
        // last run
        onTriggered: {
            if (!root.advanceReplay(Math.min(frameTime, 0.05)))
                root.endReplay()
        }
    }

    Node {
        id: replay

        readonly property var frame: root.replayFrame

        visible: root.replaying && frame !== null

        Repeater3D {
            model: 4

            delegate: Node {
                id: replayBall

                required property int index
                readonly property var ball: replay.frame?.balls[index]

                visible: ball !== undefined
                position: ball ? Qt.vector3d(ball.x, ball.y, 0) : Qt.vector3d(0, 0, 0)
                opacity: ball?.shown ?? 1.0

                Disc {
                    sphere: true
                    radius: root.ballRadius
                    color: replayBall.ball?.extra ? Theme.extraBall : Theme.ball
                    glow: 0.35
                    shininess: 1.0
                }
            }
        }

        // Where the main ball was a moment ago
        Repeater3D {
            model: 5

            delegate: Disc {
                required property int index
                readonly property var frame: root.recording[root.replayIndex - 2 * (index + 1)]
                readonly property var ball: frame?.balls[0]

                visible: ball !== undefined
                position: ball ? Qt.vector3d(ball.x, ball.y, -0.3) : Qt.vector3d(0, 0, 0)
                opacity: 0.4 * (1.0 - index / 5) * (ball?.shown ?? 1.0)
                radius: root.ballRadius * (1.0 - 0.1 * index)
                thickness: 0.1
                color: Theme.ball
            }
        }

        component ReplayPaddle: Model {
            id: replayPaddle
            property real length
            property color color
            source: "#Cube"
            scale: Qt.vector3d(root.paddleWidth / 100, length / 100, 0.01)
            materials: PhongMaterial {
                color: replayPaddle.color
                glow: 0.6
                shininess: 0.7
            }
        }

        ReplayPaddle {
            position: Qt.vector3d(-root.paddleX, replay.frame?.leftY ?? 0, 0)
            eulerRotation.z: replay.frame?.leftAngle ?? 0
            length: replay.frame?.leftLength ?? 1
            color: Theme.leftPlayer
        }

        ReplayPaddle {
            visible: !root.squash
            position: Qt.vector3d(root.paddleX, replay.frame?.rightY ?? 0, 0)
            eulerRotation.z: replay.frame?.rightAngle ?? 0
            length: replay.frame?.rightLength ?? 1
            color: Theme.rightPlayer
        }

        Text3D {
            id: replayLabel
            x: -0.5 * root.stageWidth + 2.0
            y: 0.5 * root.stageHeight - 2.6
            z: 1.0
            color: Theme.accent
            glow: 0.8
            text: qsTr("Replay")

            SequentialAnimation on opacity {
                running: replay.visible
                loops: Animation.Infinite
                NumberAnimation { from: 1.0; to: 0.3; duration: 500 }
                NumberAnimation { from: 0.3; to: 1.0; duration: 500 }
            }
        }

        Text3D {
            x: 0.5 * root.stageWidth - 2.0
            y: 0.5 * root.stageHeight - 2.6
            z: 1.0
            scale: Qt.vector3d(0.5, 0.5, 0.5)
            horizontalAlignment: Text.AlignRight
            color: Theme.dimmed
            text: qsTr("[any key] skip")
        }
    }

    // Kickoff countdown around the ball and where the ball will go
    Node {
        id: kickoff

        readonly property real fraction: match.serveDelay > 0.0 ? match.serveCountdown / match.serveDelay : 0.0
        readonly property int dots: 16

        visible: match.state === Match.Serving
        z: 0.6

        // A clock of dots going out one after another
        Discs {
            radius: 0.15
            thickness: 0.2
            items: {
                const items = []
                const shown = Math.ceil(kickoff.fraction * kickoff.dots)
                for (let index = 0; index < shown; ++index) {
                    const angle = 0.5 * Math.PI - index * 2.0 * Math.PI / kickoff.dots
                    items.push({ x: 1.7 * Math.cos(angle), y: 1.7 * Math.sin(angle), color: Theme.text, glow: 0.25 })
                }
                return items
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
    FloatingText {
        id: pickupPopup
    }

    FloatingText {
        id: perfectPopup
        textScale: 0.5
    }

    FloatingText {
        id: brickPopup
        textScale: 0.8
    }

    // Arena name at the start, set winners
    Banner {
        id: banner
        y: 3.0
        z: 1.5
    }

    Banner {
        id: calloutBanner
        y: 6.0
        z: 1.5
    }

    Banner {
        id: achievementBanner
        y: -5.5
        z: 1.5
    }

    // Beside the field on wide windows
    PlayerPanel {
        visible: root.sideLayout
        x: -0.5 * root.stageWidth - 2.8
        name: match.left.name
        color: Theme.leftPlayer
        score: root.endless ? root.endlessScore : root.squash ? match.rally : match.left.score
        sets: match.left.sets
        setsToWin: match.setsToWin
        effects: root.leftEffects
        power: match.left.power
        lives: root.solo ? root.lives : 0
        livesLeft: root.lives - match.right.score
    }

    PlayerPanel {
        visible: root.sideLayout
        x: 0.5 * root.stageWidth + 2.8
        name: match.right.name
        color: Theme.rightPlayer
        score: root.solo ? "" : match.right.score
        sets: match.right.sets
        setsToWin: match.setsToWin
        effects: root.rightEffects
        power: match.right.power
        showPower: !root.squash

        Text3D {
            y: -3.6
            scale: Qt.vector3d(0.4, 0.4, 0.4)
            horizontalAlignment: Text.AlignHCenter
            color: Theme.dimmed
            text: root.statusLine
        }
    }

    // Scoreboard
    Node {
        visible: !root.sideLayout
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

        Node {
            visible: !root.solo

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
        }

        // Endless and squash: the score or the rally, and the balls left
        Text3D {
            visible: root.solo
            horizontalAlignment: Text.AlignHCenter
            text: root.endless ? root.endlessScore : match.rally
        }

        Discs {
            sphere: true
            radius: 0.3
            items: {
                const items = []
                for (let index = 0; root.solo && index < root.lives; ++index)
                    items.push({ x: 3.0 + index * 1.0, y: 0.35, glow: 0.4,
                                 color: index < root.lives - match.right.score ? Theme.leftPlayer : Theme.goal })
                return items
            }
        }

        Text3D {
            id: rightName
            x: 0.5 * root.stageWidth
            horizontalAlignment: Text.AlignRight
            color: Theme.rightPlayer
            glow: 0.5
            text: match.right.name
        }

        // Won sets as dots above the scores, both sides in one go
        Discs {
            radius: 0.22
            thickness: 0.2
            items: {
                const items = []
                for (let index = 0; match.setsToWin > 1 && index < match.setsToWin; ++index) {
                    items.push({ x: -2.4 - index * 0.7, y: 1.5, glow: 0.25,
                                 color: index < match.left.sets ? Theme.title : Theme.goal })
                    items.push({ x: 2.4 + index * 0.7, y: 1.5, glow: 0.25,
                                 color: index < match.right.sets ? Theme.title : Theme.goal })
                }
                return items
            }
        }
    }

    // Power bars under the field, a segment for every hit
    Node {
        visible: !root.sideLayout
        y: -0.5 * root.stageHeight - 1.15

        PowerBar {
            x: -0.5 * root.stageWidth
            power: match.left.power
            color: Theme.leftPlayer
        }

        PowerBar {
            visible: !root.squash
            x: 0.5 * root.stageWidth
            direction: -1
            power: match.right.power
            color: Theme.rightPlayer
        }
    }

    // Under the field, see statusLine
    Text3D {
        visible: !root.sideLayout
        x: -0.5 * root.stageWidth
        y: -0.5 * root.stageHeight - 2.2
        scale: Qt.vector3d(0.5, 0.5, 0.5)
        color: Theme.dimmed
        text: root.statusLine
    }

    // Pause overlay
    Node {
        id: pauseOverlay
        visible: match.state === Match.Paused || root.askLeave
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
            text: root.askLeave ? qsTr("Leave?") : qsTr("Paused")
        }

        // On the network, who it was
        Text3D {
            visible: match.state === Match.Paused && !root.askLeave && root.pausedBy !== ""
            y: 0.9
            scale: Qt.vector3d(0.6, 0.6, 0.6)
            horizontalAlignment: Text.AlignHCenter
            color: Theme.dimmed
            text: qsTr("%1 paused").arg(root.pausedBy)
        }

        Repeater3D {
            model: root.pauseItems

            delegate: Text3D {
                id: pauseItem

                required property var modelData
                required property int index

                y: -1.0 - index * 2.0
                horizontalAlignment: Text.AlignHCenter
                color: index === root.currentPauseItem ? Theme.text : Theme.unselected
                text: modelData.text
                clickable: pauseOverlay.visible
                onClicked: {
                    SoundEffects.play(SoundEffects.MenuSelect)
                    pauseItem.modelData.activate()
                }

                // Like the markers of the menus, a flat disc glows too much
                Disc {
                    visible: pauseItem.index === root.currentPauseItem
                    position: Qt.vector3d(-0.5 * pauseItem.textWidth - 1.0, 0.35, 0)
                    radius: 0.35
                    sphere: true
                }
            }
        }
    }
}
