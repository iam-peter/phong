pragma ComponentBehavior: Bound

import QtQuick
import QtQuick3D
import QtQuick3D.Helpers
import QtQuick3D.Physics
import Phong

// Three to six players on a regular polygon, you at the bottom against
// the computer. The middle of each side is a goal, posts keep the goals
// apart. A player out of balls gets a wall instead of the goal, the last
// one left wins. The paddles smash, dash and use their specials like on
// the classic field.
Scene {
    id: root

    property int players: 3
    property Scene menuScene
    // Who plays each side: { kind: "keyboard", keys }, { kind: "pad", pad }
    // or { kind: "cpu" }. keys is the party key set, 0 or 1. Sides without
    // one are played by the computer.
    property var controllers: [{ kind: "keyboard" }]

    function controller(side) {
        return controllers[side] ?? { kind: "cpu" }
    }

    // A single keyboard player has both key sets
    readonly property int keyboards: controllers.filter((c) => c.kind === "keyboard").length

    // The side a key set plays, -1 for none
    function sideOfKeys(set) {
        if (keyboards === 1)
            return controllers.findIndex((c) => c.kind === "keyboard")
        return controllers.findIndex((c) => c.kind === "keyboard" && (c.keys ?? 0) === set)
    }

    // On the network the host runs the game and sends its state, the other
    // machines show it, their side at the bottom, and send their input
    property bool remote: false
    property int localSlot: 0
    // Names from the host, the same for everybody
    property var names: []
    // A joined machine without a side watches
    readonly property bool spectating: remote && localSlot < 0
    readonly property real fieldRotation: remote && localSlot >= 0 ? -localSlot * 360 / players : 0
    // Host: players on the network who watch, by id, and the sides of the
    // players who left by their token, they get them back when they join
    // again
    property var spectators: []
    property var departed: ({})
    // Host: the game was started on the LAN, it takes the ones who join
    property bool lanGame: false
    readonly property bool hosting: Lan.role === Lan.Host
                                    && (controllers.some((c) => c.kind === "remote") || spectators.length > 0)
    // Host: the latest input of every remote player by id
    property var remoteInputs: ({})
    // Host: what happened since the last state sent
    property var netEvents: []
    // Remote: the latest state from the host, its age and the input sent
    property vector2d remoteBall: Qt.vector2d(0, 0)
    // Offset, wind up, dash and dash cooldown of every paddle
    property var remotePaddles: []
    property real remoteAge: 0.0
    // The input sent in the last moments, [seconds, move], for the own
    // paddle before the host's state has it
    property var inputHistory: []
    // Seconds to the host and back, as measured
    readonly property real roundTrip: Math.max(0, Math.min(Lan.latency, 500)) / 1000

    function predictOffset(offset, length, charge, window) {
        const limit = paddleLimit(length)
        let left = window
        for (let i = inputHistory.length - 1; i >= 0 && left > 0; --i) {
            const dt = Math.min(inputHistory[i][0], left)
            offset = Math.max(-limit, Math.min(limit, offset + inputHistory[i][1] * paddleSpeed * (1.0 - 0.4 * charge) * dt))
            left -= dt
        }
        return offset
    }
    property real sentInput: 0.0
    property bool sentCharging: false
    property real sinceSent: 0.0
    // Remote: asking before leaving
    property bool askLeave: false

    function playerName(side) {
        if (remote)
            return side === localSlot ? qsTr("You") : names[side] ?? ""
        const controller = root.controller(side)
        if (controller.kind === "keyboard")
            return keyboards > 1 ? qsTr("Keys %1").arg((controller.keys ?? 0) + 1) : qsTr("You")
        if (controller.kind === "remote")
            return controller.name
        if (controller.kind === "pad")
            return qsTr("Pad %1").arg(controllers.slice(0, side + 1).filter((c) => c.kind === "pad").length)
        return qsTr("CPU %1").arg(side)
    }

    // The names as the others see them
    function sharedNames() {
        const names = []
        for (let side = 0; side < players; ++side)
            names.push(controller(side).kind !== "keyboard" ? playerName(side)
                       : keyboards > 1 ? qsTr("%1 %2").arg(Lan.localName).arg((controller(side).keys ?? 0) + 1)
                       : Lan.localName)
        return names
    }

    function netEvent(event) {
        if (hosting)
            netEvents.push(event)
    }

    function sendState() {
        if (!hosting)
            return
        const paddles = []
        for (let i = 0; i < sides.count; ++i) {
            const side = sides.objectAt(i)
            paddles.push(side ? [side.offset, side.charge, side.dash.direction, side.dash.cooldown] : [0, 0, 0, 0])
        }
        Lan.sendAll({ t: "state", match: match.snapshot(), modifiers: modifiers.snapshot(), ball: [ball.x, ball.y],
                      paddles: paddles, events: netEvents })
        netEvents = []
    }

    // The gamepad playing a side here, if any
    function padOf(side) {
        if (remote)
            return side === localSlot && Gamepads.count > 0 ? Gamepads.pads[0] : null
        const controller = root.controller(side)
        return controller.kind === "pad" ? controller.pad : null
    }

    function rumble(side, strength, duration) {
        const pad = padOf(side)
        if (pad)
            Gamepads.rumble(pad, strength, duration)
    }

    // A point of the field where it is on the screen, the field turns on
    // the network
    function onScreen(x, y) {
        const angle = fieldRotation * Math.PI / 180
        return Qt.vector3d(x * Math.cos(angle) - y * Math.sin(angle), x * Math.sin(angle) + y * Math.cos(angle), 0)
    }

    property real shakeAmount: 0.0
    property vector2d shakeOffset: Qt.vector2d(0, 0)

    function shake(amount) {
        shakeAmount = Math.max(shakeAmount, amount)
    }

    FrameAnimation {
        running: root.active && root.shakeAmount > 0.01
        onTriggered: {
            root.shakeAmount *= Math.exp(-7.0 * Math.min(frameTime, 0.05))
            root.shakeOffset = root.shakeAmount > 0.01 ? Qt.vector2d((Math.random() - 0.5) * root.shakeAmount,
                                                                     (Math.random() - 0.5) * root.shakeAmount)
                                                       : Qt.vector2d(0, 0)
        }
    }

    // Sounds and sparks of what happened, here or at the host
    function playEvent(event) {
        const paddle = event.p !== undefined ? sides.objectAt(event.p)?.paddle : null
        const pitch = 1.0 + 0.04 * Math.min(match.rally, 20)
        switch (event.e) {
            case "serve":
                SoundEffects.play(SoundEffects.Serve)
                break
            case "hit": {
                paddle?.flash()
                const at = Qt.vector3d(ball.x, ball.y, 0.5)
                if (event.perfect) {
                    SoundEffects.play(SoundEffects.Perfect, pitch)
                    sparks.burst(at, Theme.text, 30)
                    perfectPopup.show(qsTr("Perfect!"), Theme.text, onScreen(ball.x, ball.y))
                }
                if (event.smash >= 0.25) {
                    rumble(event.p, 0.5 + 0.5 * event.smash, 160)
                    SoundEffects.play(SoundEffects.Smash, 0.8 + 0.4 * event.smash)
                    sparks.burst(at, Theme.text, Math.round(20 + 40 * event.smash))
                    shake(0.3 + 0.5 * event.smash)
                }
                else {
                    rumble(event.p, event.perfect ? 0.45 : 0.25, 70)
                    SoundEffects.play(SoundEffects.PaddleHit, pitch)
                    sparks.burst(at, Theme.ball, 10)
                }
                break
            }
            case "special":
                rumble(event.p, 0.6, 220)
                SoundEffects.play(SoundEffects.Special)
                if (paddle)
                    sparks.burst(Qt.vector3d(paddle.x, paddle.y, 0.5), paddle.color, 40)
                shake(0.2)
                break
            case "catch":
                rumble(event.p, 0.4, 120)
                paddle?.flash()
                SoundEffects.play(SoundEffects.Catch)
                sparks.burst(Qt.vector3d(ball.x, ball.y, 0.5), Theme.tint(paddle?.magnetColor ?? "#ff3333"), 16)
                break
            case "shield":
                SoundEffects.play(SoundEffects.ShieldHit)
                sparks.burst(Qt.vector3d(ball.x, ball.y, 0.5), Theme.shield, 30)
                shake(0.35)
                break
            case "pickup": {
                const definition = modifiers.definition(modifiers.findDefinition(event.def))
                pickupPopup.show(definition.name, Theme.tint(definition.color), onScreen(event.x, event.y))
                sparks.burst(Qt.vector3d(event.x, event.y, 0.5), Theme.tint(definition.color), 24)
                if (definition.effect === Modifiers.Freeze)
                    SoundEffects.play(SoundEffects.Freeze)
                else if (definition.target === Modifiers.Opponent || definition.effect === Modifiers.GhostBall)
                    SoundEffects.play(SoundEffects.Curse)
                else
                    SoundEffects.play(SoundEffects.Pickup)
                break
            }
            case "dash":
                SoundEffects.play(SoundEffects.Dash)
                if (paddle)
                    sparks.burst(Qt.vector3d(paddle.x, paddle.y, 0.5), paddle.color, 10)
                break
            case "wall":
                SoundEffects.play(SoundEffects.WallHit)
                break
            case "goal":
                rumble(event.p, 1.0, 350)
                SoundEffects.play(SoundEffects.Goal)
                sparks.burst(Qt.vector3d(ball.x, ball.y, 0.5), root.colors[event.p], 50)
                shake(0.6)
                ball.clearTrail()
                break
            case "out":
                banner.show(qsTr("%1 out").arg(playerName(event.p)), root.colors[event.p])
                break
            case "left":
                banner.show(qsTr("%1 left").arg(event.name), Theme.dimmed)
                break
            case "back":
                banner.show(qsTr("%1 is back").arg(event.name), Theme.title)
                break
        }
    }

    // The game from the host, again after a rematch
    function startRemote(message) {
        players = message.players
        localSlot = message.slot
        names = message.names
        place = 0
        askLeave = false
        remotePaddles = []
        releaseInput()
        remoteBall = Qt.vector2d(0, 0)
        for (let i = 0; i < sides.count; ++i)
            sides.objectAt(i)?.reset()
        ball.clearTrail()
        match.applySnapshot(message.match ?? {})
        modifiers.applySnapshot(message.modifiers ?? {})
    }

    function applyRemote(message) {
        match.applySnapshot(message.match)
        modifiers.applySnapshot(message.modifiers ?? {})
        remoteBall = Qt.vector2d(message.ball[0], message.ball[1])
        remotePaddles = message.paddles ?? []
        remoteAge = 0.0
        for (const event of message.events ?? [])
            playEvent(event)

        // Out, or the winner
        if (spectating)
            return
        if (place === 0 && match.livesLeft.length > localSlot && !match.isAlive(localSlot)) {
            place = match.alive + 1
            SoundEffects.play(SoundEffects.Lose)
            recordResult(false)
        }
        else if (place === 0 && match.state === PartyMatch.Finished && match.winner === localSlot) {
            place = 1
            SoundEffects.play(SoundEffects.Win)
            recordResult(true)
        }
    }

    // Remote: the picture follows the host, the input goes there
    FrameAnimation {
        running: root.active && root.remote
        onTriggered: {
            const dt = Math.min(frameTime, 0.05)
            root.remoteAge += dt

            // The ball flies on from the last state to where it is at the
            // host by now, half a round trip later, a moment at most
            const age = match.state === PartyMatch.Playing
                      ? Math.min(root.remoteAge + 0.5 * root.roundTrip, 0.1 + Math.min(0.5 * root.roundTrip, 0.1)) : 0.0
            const v = match.ball.velocity
            ball.position = Qt.vector3d(root.remoteBall.x + v.x * age, root.remoteBall.y + v.y * age, 0)
            ball.hidden = root.ghosted()
            ball.advance(dt)

            // The own side is at the bottom here, right is along it
            const pad = Gamepads.count > 0 ? Gamepads.pads[0] : null
            let input = root.keyVector(0).x
            if (input === 0 && pad && pad.direction.x !== 0)
                input = Math.max(-1, Math.min(1, pad.direction.x * 1.4))
            else if (input === 0 && !isNaN(root.pointerX))
                input = Math.max(-1, Math.min(1, (root.pointerX - (sides.objectAt(root.localSlot)?.offset ?? 0)) / 0.5))
            const charging = root.keysCharging(0) || (pad?.isPressed(KeySettings.padButton(KeySettings.PadSmash)) ?? false)

            // The own paddle answers the input right away: the host's
            // position after the input it can't have seen yet
            const own = root.spectating ? null : match.player(root.localSlot)
            if (own) {
                root.inputHistory.push([dt, (own.reversed ? -1 : 1) * input])
                if (root.inputHistory.length > 240)
                    root.inputHistory.shift()
            }
            const predicting = own && !own.frozen && root.running && match.heldBy !== root.localSlot
            for (let i = 0; i < sides.count; ++i) {
                const side = sides.objectAt(i)
                if (!side)
                    continue
                let target = root.remotePaddles[i]?.[0] ?? 0
                if (predicting && i === root.localSlot)
                    target = root.predictOffset(target, side.length, root.remotePaddles[i]?.[1] ?? 0,
                                                root.roundTrip + root.remoteAge)
                side.offset += (target - side.offset) * Math.min(1.0, 25.0 * dt)
            }
            if (root.spectating)
                return
            root.sinceSent += dt
            if (input !== root.sentInput || charging !== root.sentCharging || root.sinceSent > 0.2) {
                Lan.sendToHost({ t: "input", move: input, charging: charging })
                root.sentInput = input
                root.sentCharging = charging
                root.sinceSent = 0.0
            }
        }
    }

    Connections {
        target: Lan
        enabled: root.active && root.hosting
        function onReceived(peer, message) {
            const side = root.controllers.findIndex((c) => c.kind === "remote" && c.id === peer)
            if (side < 0)
                return
            if (message.t === "input") {
                const inputs = root.remoteInputs
                inputs[peer] = Math.max(-1, Math.min(1, Number(message.move) || 0))
                root.remoteInputs = inputs
                const charging = message.charging === true
                if (charging !== (sides.objectAt(side)?.charging ?? false))
                    root.setCharging(side, charging)
            }
            else if (message.t === "action" && root.running) {
                const direction = Number(message.d) > 0 ? 1 : -1
                if (message.a === "special")
                    match.useSpecial(side)
                else if (message.a === "tap")
                    root.dash(side, direction, false)
                else if (message.a === "dash")
                    root.dash(side, direction, true)
            }
        }
    }

    Connections {
        target: Lan
        enabled: root.lanGame && Lan.role === Lan.Host
        function onPeerLeft(peer) {
            root.spectators = root.spectators.filter((id) => id !== peer)

            // The computer takes over until they are back
            const side = root.controllers.findIndex((c) => c.kind === "remote" && c.id === peer)
            if (side < 0)
                return
            root.netEvent({ e: "left", name: root.playerName(side) })
            root.playEvent({ e: "left", name: root.playerName(side) })
            const token = root.controllers[side].token ?? ""
            if (token !== "") {
                const departed = root.departed
                departed[token] = { side: side, name: root.controllers[side].name }
                root.departed = departed
            }
            const controllers = root.controllers.slice()
            controllers[side] = { kind: "cpu" }
            root.controllers = controllers
        }

        // A player who left gets the side back, everybody else watches
        function onPeerJoined(peer, name, token) {
            const back = root.departed[token]
            if (back !== undefined && root.controller(back.side).kind === "cpu") {
                const departed = root.departed
                delete departed[token]
                root.departed = departed
                const controllers = root.controllers.slice()
                controllers[back.side] = { kind: "remote", id: peer, name: back.name, token: token }
                root.controllers = controllers
                root.netEvent({ e: "back", name: back.name })
                root.playEvent({ e: "back", name: back.name })
                root.sendStart(peer, back.side)
                return
            }
            root.spectators = root.spectators.concat([peer])
            root.sendStart(peer, -1)
        }
    }

    // The game so far for a player on the network, slot -1 watches
    function sendStart(peer, slot) {
        Lan.send(peer, { t: "start", party: true, players: players, slot: slot, names: sharedNames(),
                         match: match.snapshot(), modifiers: modifiers.snapshot() })
    }

    // Gamepads work the pause menu and the end, not the game
    menuNavigation: match.state === PartyMatch.Paused || place > 0 || askLeave

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
            if (!pressed || pad !== Gamepads.pads[0] || place > 0)
                return
            if (action === KeySettings.PadPause)
                askLeave = !askLeave
            else if (spectating)
                return
            else if (action === KeySettings.PadSpecial && running)
                Lan.sendToHost({ t: "action", a: "special" })
            else if (action === KeySettings.PadDash && running && Math.abs(pad.direction.x) > 0.3)
                Lan.sendToHost({ t: "action", a: "dash", d: pad.direction.x > 0 ? 1 : -1 })
            return
        }
        if (action === KeySettings.PadPause) {
            if (pressed && place === 0) {
                if (match.state === PartyMatch.Paused)
                    match.resume()
                else if (running)
                    match.pause()
            }
            return
        }

        const side = controllers.findIndex((c) => c.kind === "pad" && c.pad === pad)
        if (side < 0 || !running)
            return
        switch (action) {
            case KeySettings.PadSmash:
                setCharging(side, pressed)
                break
            case KeySettings.PadSpecial:
                if (pressed)
                    match.useSpecial(side)
                break
            case KeySettings.PadDash: {
                // A dash the way the stick points along the side
                const along = pad.direction.dotProduct(tangent(side))
                if (pressed && Math.abs(along) > 0.3)
                    dash(side, along > 0 ? 1 : -1, true)
                break
            }
        }
    }

    Connections {
        target: Gamepads
        enabled: root.active
        function onButtonPressed(pad, button) { root.gamepadButton(pad, button, true) }
        function onButtonReleased(pad, button) { root.gamepadButton(pad, button, false) }
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
    // Seconds to wind up a full smash, how fast keys slide a held ball
    // along the paddle and how long the computer holds one
    readonly property real chargeTime: 0.6
    readonly property real holdSlide: 2.0
    readonly property real computerHoldTime: 0.45

    // A paddle may reach a little in front of the posts
    function paddleLimit(length) {
        return 0.5 * (goalWidth - length) + 0.4
    }
    // Names and balls left sit this far out from the middle
    readonly property real labelDistance: apothem + wallThickness + 1.2

    readonly property var colors: [Theme.leftPlayer, Theme.rightPlayer, Theme.tint("#ffd24d"),
                                   Theme.tint("#66ff66"), Theme.tint("#ff8c1a"), Theme.tint("#a64dff")]
    readonly property bool running: match.state === PartyMatch.Serving || match.state === PartyMatch.Playing

    // Out of the game: the place, 1 is the winner, 0 while still in
    property int place: 0

    // The party actions held down, by KeySettings.PartyAction
    property var pressed: []
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
    contentHalfWidth: sideLayout ? extentX + 8.5 : Math.max(extentX + 3.5, 18.5)
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
    viewOffset: Qt.vector3d(shakeOffset.x, middleY + shakeOffset.y, 0)

    // Where the keys of a set point on the screen, both sets on a joined
    // machine or for a single keyboard player
    function keyVector(set) {
        const sets = remote || keyboards <= 1 ? [0, 1] : [set]
        let x = 0
        let y = 0
        for (const s of sets) {
            const base = s * 6
            x += (pressed[base + 1] ? 1 : 0) - (pressed[base] ? 1 : 0)
            y += (pressed[base + 2] ? 1 : 0) - (pressed[base + 3] ? 1 : 0)
        }
        return Qt.vector2d(Math.max(-1, Math.min(1, x)), Math.max(-1, Math.min(1, y)))
    }

    function keysCharging(set) {
        const sets = remote || keyboards <= 1 ? [0, 1] : [set]
        return sets.some((s) => pressed[s * 6 + 4] === true)
    }

    // How far a direction on the screen pushes the paddle along its side
    function along(side, direction) {
        return Math.max(-1, Math.min(1, direction.dotProduct(tangent(side)) * 1.4))
    }

    // Letting go of the smash throws a held ball
    function setCharging(side, on) {
        const body = sides.objectAt(side)
        if (!body)
            return
        if (!on && body.charging && match.heldBy === side)
            match.releaseBall(body.charge)
        body.charging = on
    }

    // A tap of a direction along the side, twice dashes. Right away for
    // the dash button of a pad.
    function dash(side, direction, now) {
        const player = match.player(side)
        const body = sides.objectAt(side)
        if (!body || player.frozen || !match.isAlive(side))
            return
        const way = direction * (player.reversed ? -1 : 1)
        if (now)
            body.dash.trigger(way)
        else
            body.dash.tap(way)
    }

    // The computer puts a caught ball where it rolled and lets go
    function computerHold(side, body, dt) {
        const slide = holdSlide * dt
        const offset = match.holdOffset
        match.aimHeldBall(offset + Math.max(-slide, Math.min(slide, body.holdAim - offset)))
        if (match.holdTime <= match.maxHoldTime - computerHoldTime)
            match.releaseBall(body.charge)
    }

    function windUp(charge, charging, dt) {
        return charging ? Math.min(1.0, charge + dt / chargeTime) : Math.max(0.0, charge - 3.0 * dt)
    }

    function start() {
        remote = false
        place = 0
        releaseInput()
        for (let i = 0; i < sides.count; ++i)
            sides.objectAt(i)?.reset()
        resetBall()
        match.start()
        modifiers.reset()

        // Everybody on the network starts with it
        lanGame = Lan.role === Lan.Host
        if (lanGame) {
            controllers.forEach((controller, side) => {
                if (controller.kind === "remote")
                    sendStart(controller.id, side)
            })
            for (const peer of spectators)
                sendStart(peer, -1)
            Lan.setInfo({ mode: "party", players: players, open: 0, playing: true })
        }
    }

    // The stats of this machine, won when one of its players won
    function recordResult(won) {
        Stats.recordParty(won, match.longestRally)
        if (!won)
            return
        const achieved = []
        if (Stats.unlock("lastStanding"))
            achieved.push(Stats.achievements.find((a) => a.id === "lastStanding").name)
        if (players === 6 && Stats.unlock("fullHouse"))
            achieved.push(Stats.achievements.find((a) => a.id === "fullHouse").name)
        if (achieved.length > 0)
            banner.show(qsTr("Achievement: %1").arg(achieved.join(", ")), Theme.title)
    }

    function leave() {
        lanGame = false
        spectators = []
        departed = {}
        match.stop()
        releaseInput()
        Lan.leave()
        phong.returnTo(root.menuScene)
    }

    function releaseInput() {
        pressed = []
        pointerX = NaN
        for (let i = 0; i < sides.count; ++i) {
            const side = sides.objectAt(i)
            if (side)
                side.charging = false
        }
    }

    function resetBall() {
        ball.reset(Qt.vector3d(0, 0, 0), Qt.vector3d(0, 0, 0))
        ball.setLinearVelocity(Qt.vector3d(0, 0, 0))
        ball.clearTrail()
    }

    // The ball flew through an item, collected after the physics step
    property var pendingItems: []

    function queueCollect(itemId) {
        pendingItems.push(itemId)
        Qt.callLater(collectPending)
    }

    function collectPending() {
        const items = pendingItems
        pendingItems = []
        for (const item of items)
            modifiers.collect(item)
    }

    // A ghost ball can't be seen in the middle
    readonly property real ghostRadius: 0.45 * apothem

    function ghosted() {
        return modifiers.ghostBall && match.state === PartyMatch.Playing
               && Qt.vector2d(ball.x, ball.y).length() < ghostRadius
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
        if (other.shieldOf !== undefined && other.shieldOf >= 0) {
            if (match.shieldHit(other.shieldOf)) {
                playEvent({ e: "shield" })
                netEvent({ e: "shield" })
            }
        }
        else if (other.paddleOf !== undefined) {
            // A full power bar spent catches the ball
            const player = other.paddleOf
            const side = sides.objectAt(player)
            const along = Qt.vector2d(ball.x, ball.y).dotProduct(tangent(player))
            const offset = (along - side.offset) / (0.5 * side.length + ballRadius)
            if (match.player(player).catches > 0 && match.catchBall(player, offset))
                return
            match.paddleHit(player, offset, side.velocity, side.charge)
        }
        else if (other.wall === true && normals.length > 0) {
            // Pointing to the ball, whichever way the report has it
            let n = Qt.vector2d(normals[0].x, normals[0].y)
            if (n.dotProduct(Qt.vector2d(ball.x - other.x, ball.y - other.y)) < 0)
                n = n.times(-1)
            if (match.bounce(n)) {
                playEvent({ e: "wall" })
                netEvent({ e: "wall" })
            }
        }
    }

    function step(dt) {
        // A held ball about to go is thrown with the wind up so far
        if (match.heldBy >= 0 && match.holdTime <= dt)
            match.releaseBall(sides.objectAt(match.heldBy)?.charge ?? 0)

        // The ball waits for the kickoff in the middle, see GameScene
        if (match.state === PartyMatch.Serving && (Math.abs(ball.x) > 0.01 || Math.abs(ball.y) > 0.01))
            resetBall()

        match.advance(dt)
        if (!running)
            return

        modifiers.advance(dt)
        // A gravity well bends the flight
        if (modifiers.gravityStrength > 0.0)
            match.attract(Qt.vector2d(ball.x, ball.y), modifiers.gravityWell, modifiers.gravityStrength, dt)

        const position = Qt.vector2d(ball.x, ball.y)
        const velocity = match.ball.velocity
        for (let i = 0; i < sides.count; ++i) {
            const side = sides.objectAt(i)
            if (!side || !match.isAlive(i))
                continue

            side.dash.advance(dt)
            const player = match.player(i)
            // Reversed controls swap the directions
            const sign = player.reversed ? -1 : 1
            const held = match.heldBy === i
            const controller = root.controller(i)
            let input = 0
            if (controller.kind === "keyboard") {
                input = sign * along(i, keyVector(controller.keys ?? 0))
                // A pointer steers the bottom side
                if (input === 0 && i === 0 && !isNaN(pointerX))
                    input = Math.max(-1, Math.min(1, (sign * pointerX - side.offset) / 0.5))
            }
            else if (controller.kind === "remote") {
                input = sign * (root.remoteInputs[controller.id] ?? 0)
            }
            else if (controller.kind === "pad") {
                // Pushed along the side, whichever way it runs on screen
                if (controller.pad)
                    input = sign * along(i, controller.pad.direction)
            }
            else {
                // The computer plays in the frame of its side
                const n = normal(i)
                const t = tangent(i)
                side.computer.confusion = player.reversed ? 1.0 : 0.0
                side.computer.blind = ghosted()
                side.computer.update(dt, Qt.vector2d(position.dotProduct(n), position.dotProduct(t)),
                                     Qt.vector2d(velocity.dotProduct(n), velocity.dotProduct(t)), side.offset)
                input = side.computer.direction
                side.charging = side.computer.charging
                if (side.computer.wantsDash && !held && !player.frozen)
                    side.dash.trigger(input >= 0 ? 1 : -1)
                // It spends a full power bar right away
                if (player.power >= 1.0 && player.catches === 0)
                    match.useSpecial(i)
            }

            // A paddle holding the ball stands, the input aims it
            if (held) {
                if (controller.kind === "cpu")
                    computerHold(i, side, dt)
                else
                    match.aimHeldBall(match.holdOffset + input * holdSlide * dt)
                input = 0
            }
            else if (side.dash.active) {
                input = side.dash.direction * side.dash.boost
            }
            if (player.frozen)
                input = 0

            side.charge = windUp(side.charge, side.charging, dt)
            const limit = paddleLimit(side.length)
            const offset = Math.max(-limit, Math.min(limit, side.offset + input * paddleSpeed * (1.0 - 0.4 * side.charge) * dt))
            side.velocity = (offset - side.offset) / dt
            side.offset = offset
        }

        // A held ball sits on the face of its paddle
        if (match.heldBy >= 0) {
            const side = sides.objectAt(match.heldBy)
            if (side) {
                ball.reset(sidePoint(match.heldBy, paddleDistance - 0.5 * paddleWidth - ballRadius - 0.05,
                                     side.offset + match.holdOffset * 0.5 * side.length), Qt.vector3d(0, 0, 0))
            }
        }

        ball.hidden = ghosted()
        ball.advance(dt)
        sendState()
    }

    onActiveChanged: {
        if (!active) {
            releaseInput()
            match.stop()
        }
    }

    // Others on the network play on
    onFocusLost: {
        releaseInput()
        if (!hosting && !remote)
            match.pause()
    }

    onKeyPressed: (event) => {
        event.accepted = true
        if (event.isAutoRepeat)
            return

        // On the network only the host pauses, the others may leave
        if (remote) {
            if (askLeave || place > 0 || match.state === PartyMatch.Paused) {
                if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter)
                    leave()
                else if (event.key === Qt.Key_Escape && askLeave)
                    askLeave = false
                else if (event.key === Qt.Key_Escape)
                    leave()
                return
            }
            if (isPauseKey(event.key)) {
                askLeave = true
                return
            }
        }

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
            if (isPauseKey(event.key)) {
                SoundEffects.play(SoundEffects.MenuSelect)
                match.resume()
                return
            }
            switch (event.key) {
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

        const action = KeySettings.partyAction(event.key)
        if (action >= 0) {
            partyKey(action, true)
            return
        }
        if (isPauseKey(event.key)) {
            currentPauseItem = 0
            match.pause()
        }
    }

    // The keys of a set as a hint, two short lines
    function keysHint(set) {
        const names = KeySettings.partyKeyNames
        return [KeySettings.partySetName(set) + qsTr(" move"),
                qsTr("%1 smash  %2 special").arg(names[set * 6 + 4]).arg(names[set * 6 + 5])]
    }

    // The pause key of the controls, unless a party key has it too
    function isPauseKey(key) {
        return key === Qt.Key_Escape || (key === KeySettings.key(KeySettings.Pause) && KeySettings.partyAction(key) < 0)
    }

    onKeyReleased: (event) => {
        event.accepted = true
        if (event.isAutoRepeat)
            return
        const action = KeySettings.partyAction(event.key)
        if (action >= 0)
            partyKey(action, false)
    }

    // The direction of each of the four direction keys of a set
    readonly property var keyDirections: [Qt.vector2d(-1, 0), Qt.vector2d(1, 0), Qt.vector2d(0, 1), Qt.vector2d(0, -1)]

    function partyKey(action, down) {
        const keys = pressed
        keys[action] = down
        pressed = keys

        const set = Math.floor(action / 6)
        const key = action % 6
        if (remote) {
            if (!down || !running || spectating)
                return
            // The own side is at the bottom, right along it
            if (key < 4 && keyDirections[key].x !== 0)
                Lan.sendToHost({ t: "action", a: "tap", d: keyDirections[key].x })
            else if (key === 5)
                Lan.sendToHost({ t: "action", a: "special" })
            return
        }

        const side = sideOfKeys(set)
        if (side < 0)
            return
        if (key === 4) {
            setCharging(side, down || keysCharging(set))
            return
        }
        if (!down || !running)
            return
        if (key === 5) {
            match.useSpecial(side)
            return
        }
        // Tapping a direction along the side twice dashes
        const way = keyDirections[key].dotProduct(tangent(side))
        if (Math.abs(way) > 0.3)
            dash(side, way > 0 ? 1 : -1, false)
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
        paddleSpeed: root.paddleSpeed

        onServed: {
            root.playEvent({ e: "serve" })
            root.netEvent({ e: "serve" })
        }
        onPaddleHitBall: (player, smash, perfect) => {
            const event = { e: "hit", p: player, smash: smash, perfect: perfect }
            root.playEvent(event)
            root.netEvent(event)
        }
        onSpecialUsed: (player) => {
            root.playEvent({ e: "special", p: player })
            root.netEvent({ e: "special", p: player })
        }
        onBallCaught: (player) => {
            root.playEvent({ e: "catch", p: player })
            root.netEvent({ e: "catch", p: player })
        }
        onGoalScored: (player) => {
            root.playEvent({ e: "goal", p: player })
            root.netEvent({ e: "goal", p: player })
            root.resetBall()
            for (let i = 0; i < sides.count; ++i)
                sides.objectAt(i)?.computer.reset()
        }
        // Paused and ended states need to get out too, no steps run then
        onStateChanged: root.sendState()
        onPlayerOut: (player) => {
            root.playEvent({ e: "out", p: player })
            root.netEvent({ e: "out", p: player })
            // The last human out ends it, the place is the players left
            // and them
            const humans = root.controllers.filter((c, side) => c.kind !== "cpu" && match.isAlive(side))
            if (root.controller(player).kind !== "cpu" && humans.length === 0) {
                root.place = match.alive + 1
                SoundEffects.play(SoundEffects.Lose)
                root.recordResult(false)
                match.stop()
            }
        }
        onFinished: {
            if (root.controller(match.winner).kind !== "cpu") {
                root.place = 1
                SoundEffects.play(SoundEffects.Win)
                const local = root.controller(match.winner).kind
                root.recordResult(local === "keyboard" || local === "pad")
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

    PartyModifiers {
        id: modifiers

        match: match
        enabled: GameSettings.modifiers
        // Clear of the paddles
        spawnRadius: 0.55 * root.apothem

        onCollected: (index, collector, position) => {
            const event = { e: "pickup", def: modifiers.definition(index).id, x: position.x, y: position.y }
            root.playEvent(event)
            root.netEvent(event)
        }
    }

    PhysicsWorld {
        scene: root
        running: root.active && root.running && !root.remote
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

    // Everything on the field, turned on the network so that each player
    // has their own side at the bottom. The host doesn't turn it, the
    // physics doesn't like a turned world.
    Node {
        id: field
        eulerRotation.z: root.fieldRotation

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

        // Lines from the middle to the corners, in one draw call like
        // the posts and the goal lines below
        Instances {
            visible: GraphicsSettings.floor
            source: "#Cube"
            meshScale: Qt.vector3d(0.01, 0.01, 0.01)
            lighting: DefaultMaterial.NoLighting
            items: {
                const items = []
                for (let index = 0; index < root.players; ++index) {
                    const end = root.corner(index, root.apothem)
                    items.push({ x: 0.5 * end.x, y: 0.5 * end.y, z: -0.65, angle: Math.atan2(end.y, end.x) * 180 / Math.PI,
                                 sx: end.length(), sy: 0.08, sz: 0.1, color: Theme.grid, glow: 0.6 })
                }
                return items
            }
        }

        // The posts of all sides
        Instances {
            source: "#Cube"
            meshScale: Qt.vector3d(0.01, 0.01, 0.01)
            shininess: 0.5
            items: {
                const items = []
                const along = 0.5 * root.sideLength - 0.5 * root.postLength + 0.4
                for (let index = 0; index < root.players; ++index) {
                    for (const end of [-along, along]) {
                        const at = root.sidePoint(index, root.apothem + 0.5 * root.wallThickness, end)
                        items.push({ x: at.x, y: at.y, angle: root.sideAngle(index) * 180 / Math.PI + 90,
                                     sx: root.postLength + 0.8, sy: root.wallThickness, sz: 1.0, color: Theme.wall, glow: 0.35 })
                    }
                }
                return items
            }
        }

        // The goal lines glow in the colors of the players still in
        Instances {
            source: "#Cube"
            meshScale: Qt.vector3d(0.01, 0.01, 0.01)
            lighting: DefaultMaterial.NoLighting
            items: {
                const items = []
                for (let index = 0; index < root.players; ++index) {
                    if ((match.livesLeft[index] ?? 1) <= 0)
                        continue
                    const at = root.sidePoint(index, root.apothem + 0.2, 0)
                    items.push({ x: at.x, y: at.y, z: -0.45, angle: root.sideAngle(index) * 180 / Math.PI + 90,
                                 sx: root.goalWidth, sy: 0.15, sz: 0.1, color: root.colors[index], glow: 0.8 })
                }
                return items
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
                readonly property Player player: match.player(index)
                readonly property real length: root.paddleLength * player.paddleScale
                property real offset: 0.0
                // Along the side, per second, for the spin
                property real velocity: 0.0
                property real charge: 0.0
                property bool charging: false
                // Where the computer puts a caught ball, rolled on the catch
                property real holdAim: 0.0
                property alias paddle: paddle
                property alias computer: computer
                property alias dash: dash

                function reset() {
                    offset = 0.0
                    velocity = 0.0
                    charge = 0.0
                    charging = false
                    dash.reset()
                    computer.reset()
                }

                ComputerPlayer {
                    id: computer
                    difficulty: GameSettings.difficulty
                    paddleSpeed: root.paddleSpeed
                    paddleX: root.paddleDistance - 0.5 * root.paddleWidth - root.ballRadius
                    paddleReach: 0.5 * side.length + root.ballRadius
                    fieldTop: 100
                    fieldBottom: -100
                }

                Dash {
                    id: dash
                    onDashed: {
                        root.playEvent({ e: "dash", p: side.index })
                        root.netEvent({ e: "dash", p: side.index })
                    }
                }

                Connections {
                    target: match
                    function onBallCaught(player) {
                        if (player === side.index)
                            side.holdAim = Math.random() * 1.6 - 0.8
                    }
                }

                component Wall: DynamicRigidBody {
                    id: wall
                    property int player: 0
                    property real angle: 0.0
                    property real along: 0.0
                    property real length: 1.0
                    property bool parked: false
                    // The shield of a player, -1 for a wall
                    property int shieldOf: -1
                    readonly property bool wall: shieldOf < 0
                    property color color: Theme.wall
                    property real glow: 0.35
                    property real thickness: root.wallThickness
                    // The posts are drawn all together, see above
                    property bool drawn: true
                    // Of the middle from the middle of the polygon
                    property real distance: root.apothem + 0.5 * root.wallThickness

                    isKinematic: true
                    position: kinematicPosition
                    kinematicPosition: parked ? Qt.vector3d(1000 + 10 * shieldOf, (shieldOf >= 0 ? 2000 : 1000) + 10 * player, 0)
                                              : root.sidePoint(player, distance, along)
                    kinematicEulerRotation: Qt.vector3d(0, 0, angle + 90)
                    // Without physics, on the network, nothing else turns it
                    eulerRotation: kinematicEulerRotation
                    physicsMaterial: bouncy
                    sendContactReports: true
                    collisionShapes: BoxShape {
                        extents: Qt.vector3d(wall.length, wall.thickness, 1.0)
                    }

                    Model {
                        visible: !wall.parked && wall.drawn
                        source: "#Cube"
                        scale: Qt.vector3d(wall.length / 100, wall.thickness / 100, 0.01)
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
                    drawn: false
                }

                Wall {
                    player: side.index
                    angle: side.angle
                    along: 0.5 * root.sideLength - 0.5 * root.postLength + 0.4
                    length: root.postLength + 0.8
                    drawn: false
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

                // A shield stops one goal, just in front of the line
                Wall {
                    player: side.index
                    angle: side.angle
                    shieldOf: side.index
                    parked: !side.alive || !side.player.shielded
                    along: 0
                    length: root.goalWidth + 0.2
                    color: Theme.shield
                    glow: 1.0
                    distance: root.apothem - 0.35
                    thickness: 0.3
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
                    length: side.length
                    width: root.paddleWidth
                    back: 1
                    // On a joined machine as the host sends it
                    charge: root.remote ? (root.remotePaddles[side.index]?.[1] ?? 0) : side.charge
                    dash: root.remote ? (root.remotePaddles[side.index]?.[2] ?? 0) : side.dash.direction
                    dashCooldown: root.remote ? (root.remotePaddles[side.index]?.[3] ?? 0) : side.dash.cooldown
                    magnet: side.player.catches > 0
                    frozen: side.player.frozen
                    reversed: side.player.reversed
                }

                // Name and balls left, outside the side, upright
                Node {
                    visible: !root.sideLayout
                    position: root.sidePoint(side.index, root.labelDistance, 0)
                    eulerRotation.z: -root.fieldRotation

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

                    Discs {
                        radius: 0.24
                        sphere: true
                        items: {
                            const items = []
                            for (let index = 0; index < match.lives; ++index)
                                items.push({ x: 0.5 + index * 0.7, y: 0, glow: 0.4,
                                             color: index < (match.livesLeft[side.index] ?? 0) ? side.color : Theme.goal })
                            return items
                        }
                    }

                    PowerBar {
                        visible: side.alive
                        x: -1.5
                        y: -1.0
                        scale: Qt.vector3d(0.4, 0.4, 0.4)
                        power: side.player.power
                        color: side.color
                    }
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
                    // Upright on a turned field
                    eulerRotation.z: -root.fieldRotation
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

        // The fog a ghost ball disappears in, dark with a glowing edge
        Node {
            visible: modifiers.ghostBall
            z: -0.4

            Disc {
                radius: root.ghostRadius
                thickness: 0.02
                color: Theme.background
                glow: 0.0
            }

            Discs {
                z: 0.05
                radius: 0.08
                thickness: 0.02
                items: {
                    const items = []
                    for (let index = 0; index < 40; ++index) {
                        const angle = index * 2.0 * Math.PI / 40
                        items.push({ x: root.ghostRadius * Math.cos(angle), y: root.ghostRadius * Math.sin(angle),
                                     color: Theme.dimmed, glow: 0.6 })
                    }
                    return items
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
    }

    // Kickoff: the seconds and where the ball goes
    Node {
        visible: match.state === PartyMatch.Serving
        z: 0.6

        Node {
            eulerRotation.z: Math.atan2(match.serveDirection.y, match.serveDirection.x) * 180 / Math.PI
                             + root.fieldRotation

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

    FloatingText {
        id: perfectPopup
    }

    FloatingText {
        id: pickupPopup
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

        // The balls left of everybody in one draw call
        Discs {
            radius: 0.24
            sphere: true
            items: {
                const items = []
                for (let player = 0; player < root.players; ++player) {
                    for (let index = 0; index < match.lives; ++index)
                        items.push({ x: 4.0 + index * 0.7, y: -player * 2.0, glow: 0.4,
                                     color: index < (match.livesLeft[player] ?? 0) ? root.colors[player] : Theme.goal })
                }
                return items
            }
        }

        Repeater3D {
            model: root.players

            delegate: Node {
                id: entry

                required property int index
                readonly property bool alive: (match.livesLeft[index] ?? 1) > 0
                readonly property color color: root.colors[index]
                readonly property Player player: match.player(index)

                y: -index * 2.0

                Text3D {
                    scale: Qt.vector3d(0.6, 0.6, 0.6)
                    verticalAlignment: Text.AlignVCenter
                    color: entry.alive ? entry.color : Theme.dimmed
                    glow: entry.alive ? 0.5 : 0.0
                    text: root.playerName(entry.index)
                }


                PowerBar {
                    visible: entry.alive
                    y: -0.75
                    scale: Qt.vector3d(0.5, 0.5, 0.5)
                    power: entry.player.power
                    color: entry.color
                }
            }
        }
    }

    // In a corner, the polygons leave it free
    Node {
        x: root.sideLayout ? root.extentX + 0.8 : -18.5
        y: root.middleY - (root.sideLayout ? root.extentY - 0.8 : 11.5)
        scale: Qt.vector3d(0.38, 0.38, 0.38)

        Repeater3D {
            id: hints
            model: {
                let lines = [qsTr("%1 players").arg(root.players)]
                if (root.spectating) {
                    lines.push(qsTr("Watching"))
                }
                else if (root.keyboards > 1 && !root.remote) {
                    const one = root.keysHint(0)
                    const two = root.keysHint(1)
                    lines = lines.concat([qsTr("Keys 1: %1").arg(one[0]), one[1], qsTr("Keys 2: %1").arg(two[0]), two[1]])
                }
                else {
                    lines = lines.concat(root.keysHint(0))
                }
                if (!root.spectating)
                    lines.push(qsTr("Tap twice to dash"))
                lines.push(root.remote ? qsTr("[Esc] leave") : qsTr("[Esc] pause"))
                // Over the network the time to the host and back
                if (root.remote && Lan.latency >= 0)
                    lines.push(qsTr("%1 ms").arg(Lan.latency))
                return lines
            }

            delegate: Text3D {
                required property string modelData
                required property int index
                y: (hints.count - 1 - index) * 1.6
                color: Theme.dimmed
                text: modelData
            }
        }
    }

    // Pause and the end, over the field
    Node {
        id: overlay
        // Watchers see the end too
        readonly property bool watchedEnd: root.spectating && match.state === PartyMatch.Finished
        visible: match.state === PartyMatch.Paused || root.place > 0 || root.askLeave || watchedEnd
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
            readonly property bool youWon: root.remote ? match.winner === root.localSlot
                                                       : root.controller(match.winner).kind === "keyboard"
            text: root.place === 1 || (overlay.watchedEnd && !root.askLeave)
                  ? (youWon ? qsTr("You win") : qsTr("%1 wins").arg(root.playerName(match.winner)))
                  : root.place > 1 ? qsTr("Place %1 of %2").arg(root.place).arg(root.players)
                  : root.askLeave ? qsTr("Leave?")
                  : qsTr("Paused")
        }

        Repeater3D {
            model: root.remote ? (root.askLeave ? [qsTr("Stay"), qsTr("Leave")] : ["", qsTr("Leave")])
                   : root.place > 0 ? [qsTr("Again"), qsTr("Menu")] : [qsTr("Resume"), qsTr("Menu")]

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
                    else if (root.remote)
                        root.askLeave = false
                    else if (root.place > 0)
                        root.start()
                    else
                        match.resume()
                }

                Disc {
                    visible: !root.remote && root.place === 0 && item.index === root.currentPauseItem
                    position: Qt.vector3d(-0.5 * item.textWidth - 1.0, 0.35, 0)
                    radius: 0.35
                    sphere: true
                }
            }
        }

        Text3D {
            visible: root.place > 0 || overlay.watchedEnd
            y: -6.0
            scale: Qt.vector3d(0.5, 0.5, 0.5)
            horizontalAlignment: Text.AlignHCenter
            color: Theme.dimmed
            text: root.remote ? qsTr("Waiting for the host   [Esc] leave") : qsTr("[Enter] again   [Esc] menu")
        }
    }
}
