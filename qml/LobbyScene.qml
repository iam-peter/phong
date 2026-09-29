pragma ComponentBehavior: Bound

import QtQuick
import QtQuick3D
import Phong

// Before a game of several players: who plays which side. The keyboard is
// the first player, a gamepad joins with A and leaves with B, players on
// the network join while the game is open on the LAN, the free sides of
// the polygon are played by the computer. On a joined machine it shows the
// lobby of the host.
//
// While they wait every player has a small field to try the paddle in,
// see PracticeLane. Each machine plays its own, the host passes them on,
// so everybody sees the others warm up.
Scene {
    id: root

    contentHalfHeight: 13.9

    property Scene menuScene
    // Two players on the classic field, or three to six on a polygon
    property bool party: false
    property int players: party ? GameSettings.partyPlayers : 2
    property int currentItem: 0
    // Players sharing the keyboard in a party, each with a key set
    property int keyboards: 1
    // Gamepads { kind: "pad", pad } and network players { kind: "remote",
    // id, name } in the order they joined
    property var joiners: []
    // Network players who came when every side was taken, { id, name },
    // they watch
    property var watchers: []

    // Joined to a host: its lobby as it sends it
    property bool remote: false
    property var remoteSlots: []
    property int remoteSlot: -1

    // Typing the name
    property bool typing: false
    property string typed: ""

    // Movement keys held for the practice, by action, see practiceKey()
    property var held: ({})
    // Pointers steering a field, pointer id to the field's index
    property var pointerLanes: ({})
    property real sinceShared: 0.0

    // Open to others, on the LAN or through the server
    readonly property bool hostingLan: Lan.role === Lan.Host
    readonly property bool lanOpen: hostingLan && !Lan.online
    readonly property bool onlineOpen: Lan.online && (Lan.role === Lan.Host || Lan.role === Lan.Joining)
    readonly property var colors: [Theme.leftPlayer, Theme.rightPlayer, Theme.tint("#ffd24d"),
                                   Theme.tint("#66ff66"), Theme.tint("#ff8c1a"), Theme.tint("#a64dff")]

    // The side of every player: with two players the keyboard shares the
    // field, joiners take the right side first, then the left one
    readonly property var slots: {
        if (remote)
            return remoteSlots
        const slots = []
        if (!party) {
            slots.push(joiners.length > 1 ? joiners[1] : { kind: "keyboard", keys: "left" })
            slots.push(joiners.length > 0 ? joiners[0] : { kind: "keyboard", keys: "right" })
            return slots
        }
        for (let set = 0; set < keyboards; ++set)
            slots.push({ kind: "keyboard", keys: set, party: keyboards > 1 })
        for (let i = keyboards; i < players; ++i)
            slots.push(i - keyboards < joiners.length ? joiners[i - keyboards] : { kind: "cpu" })
        return slots
    }
    readonly property int capacity: party ? players - keyboards : 2

    // The name the others see, typed here as well as in the settings
    readonly property var nameItem: ({ text: qsTr("Name"), value: typing ? typed + "_" : GameSettings.playerName || qsTr("none"),
                                       activate: () => root.startTyping() })

    readonly property var items: {
        const items = []
        if (remote) {
            items.push(nameItem)
            items.push({ text: qsTr("Leave"), activate: () => root.back() })
            return items
        }
        if (party) {
            items.push({ text: qsTr("Players"), value: players, cycles: true, change: (step) => root.cyclePlayers(step) })
            items.push({ text: qsTr("Keyboards"), value: keyboards, cycles: true, change: () => root.cycleKeyboards() })
        }
        items.push(nameItem)
        if (Lan.canHost)
            items.push({ text: lanOpen ? qsTr("LAN open") : qsTr("LAN closed"), cycles: true,
                         change: () => root.toggleLan() })
        // Only with a server, see OnlineService
        if (OnlineService.available || onlineOpen)
            items.push({ text: !onlineOpen ? qsTr("Online closed") : Lan.role === Lan.Host ? qsTr("Online open")
                                                                            : qsTr("Online..."),
                         cycles: true, change: () => root.toggleOnline() })
        items.push({ text: qsTr("Start"), starts: true, activate: () => root.start() })
        items.push({ text: qsTr("Back"), activate: () => root.back() })
        return items
    }

    function slotName(slot) {
        switch (slot.kind) {
            case "keyboard":
                if (slot.keys === "left")
                    return qsTr("Keyboard %1").arg(KeySettings.keyNames[KeySettings.LeftUp] + "/"
                                                   + KeySettings.keyNames[KeySettings.LeftDown])
                if (slot.keys === "right")
                    return qsTr("Keyboard %1").arg(KeySettings.keyNames[KeySettings.RightUp] + "/"
                                                   + KeySettings.keyNames[KeySettings.RightDown])
                // Reading the names follows a change of the keys
                if (slot.party)
                    return KeySettings.partyKeyNames.length ? qsTr("Keyboard %1").arg(KeySettings.partySetName(slot.keys)) : ""
                return qsTr("Keyboard")
            case "pad":
                return slot.pad?.name ?? qsTr("Gamepad")
            case "remote":
                return (Lan.online ? qsTr("%1 online") : qsTr("%1 on the LAN")).arg(slot.name)
            case "host":
                return qsTr("%1, the host").arg(slot.name)
            default:
                return qsTr("Computer")
        }
    }

    // Above a field: who plays it
    function displayName(slot, index) {
        switch (slot?.kind) {
            case "remote":
            case "host":
                return slot.name
            case "cpu":
                return qsTr("Computer")
            case "keyboard":
                // The first keyboard is the player of this machine
                if ((party ? slot.keys === 0 : slot.keys === "left") && (hostingLan || GameSettings.playerName !== ""))
                    return Lan.localName
                break
        }
        return party ? qsTr("Player %1").arg(index + 1) : index === 0 ? qsTr("Left") : qsTr("Right")
    }

    // Below the name: how they play
    function slotDetail(slot, index) {
        if (remote && index === remoteSlot)
            return qsTr("You")
        switch (slot?.kind) {
            case "remote":
                return Lan.online ? qsTr("Online") : qsTr("On the LAN")
            case "host":
                return qsTr("The host")
            case "cpu":
                return ""
        }
        return slot ? slotName(slot) : ""
    }

    // The practice fields: facing each other for two, side by side along
    // the bottom for a polygon, as the paddles move in the game
    readonly property real laneY: 2.4
    readonly property real laneDepth: party ? 6.0 : 11.0
    readonly property real laneSpan: party ? Math.min(6.0, 34.0 / Math.max(slots.length, 1) - 1.2) : 6.0
    function laneX(index) {
        if (!party)
            return index === 0 ? -6.0 : 6.0
        return (index - 0.5 * (slots.length - 1)) * (laneSpan + 1.2)
    }

    // Keys the menu needs don't move a paddle
    readonly property var menuKeys: [Qt.Key_Up, Qt.Key_Down, Qt.Key_Left, Qt.Key_Right, Qt.Key_Return,
                                     Qt.Key_Enter, Qt.Key_Space, Qt.Key_Escape]

    // A movement key of the game, for the practice: true if it was one
    function practiceKey(key, pressed) {
        if (menuKeys.includes(key))
            return false
        const action = party ? KeySettings.partyAction(key) : KeySettings.action(key)
        const moves = party ? action >= 0 && action % 6 < 2
                            : [KeySettings.LeftUp, KeySettings.LeftDown, KeySettings.RightUp, KeySettings.RightDown].includes(action)
        if (!moves)
            return false
        const keys = held
        keys[action] = pressed
        held = keys
        return true
    }

    // How a field on this machine is steered, -1 to 1 along its paddle
    function laneInput(index) {
        const on = (action) => held[action] === true ? 1 : 0
        const clamp = (v) => Math.max(-1, Math.min(1, v))
        if (remote) {
            if (index !== remoteSlot)
                return 0
            let move = party ? on(1) - on(0) + on(7) - on(6)
                             : on(KeySettings.LeftUp) + on(KeySettings.RightUp) - on(KeySettings.LeftDown) - on(KeySettings.RightDown)
            const pad = Gamepads.count > 0 ? Gamepads.pads[0] : null
            if (move === 0 && pad)
                move = party ? pad.direction.x : pad.direction.y
            return clamp(move)
        }
        const slot = slots[index]
        switch (slot?.kind) {
            case "keyboard":
                if (party) {
                    const sets = keyboards <= 1 ? [0, 1] : [slot.keys]
                    return clamp(sets.reduce((move, set) => move + on(set * 6 + 1) - on(set * 6), 0))
                }
                return slot.keys === "left" ? on(KeySettings.LeftUp) - on(KeySettings.LeftDown)
                                            : on(KeySettings.RightUp) - on(KeySettings.RightDown)
            case "pad":
                return clamp(party ? slot.pad.direction.x : slot.pad.direction.y)
        }
        return 0
    }

    // The fields of all, to the players on the network
    function shareLanes() {
        const states = []
        for (let i = 0; i < slots.length; ++i) {
            const lane = lanes.objectAt(i)
            states.push(lane ? (lane.local ? lane.state() : lane.target) : null)
        }
        const message = { t: "practice", lanes: states }
        for (const joiner of joiners) {
            if (joiner.kind === "remote")
                Lan.send(joiner.id, message)
        }
        for (const watcher of watchers)
            Lan.send(watcher.id, message)
    }

    // The fields of the others, from the host
    function applyPractice(message) {
        const states = message.lanes ?? []
        for (let i = 0; i < states.length; ++i) {
            if (i !== remoteSlot)
                lanes.objectAt(i)?.applyState(states[i])
        }
    }

    function startTyping() {
        SoundEffects.play(SoundEffects.MenuSelect)
        typed = GameSettings.playerName
        typing = true
        held = {}
    }

    // A player on the network with a new name
    function rename(peer, name) {
        name = String(name ?? "").trim().slice(0, 12)
        if (name === "")
            return
        joiners = joiners.map((j) => j.kind === "remote" && j.id === peer ? Object.assign({}, j, { name: name }) : j)
        watchers = watchers.map((w) => w.id === peer ? { id: w.id, name: name } : w)
        Lan.renamePeer(peer, name)
    }

    // What the players on the network see of the slots
    function sharedSlots() {
        return slots.map((slot) => {
            if (slot.kind === "remote")
                return { kind: "remote", name: slot.name, id: slot.id }
            if (slot.kind === "cpu")
                return { kind: "cpu" }
            return { kind: "host", name: Lan.localName }
        })
    }

    // Only while it's shown, a running game has its own info
    function shareLobby() {
        if (!hostingLan || !active)
            return
        const shared = sharedSlots()
        for (const joiner of joiners) {
            if (joiner.kind === "remote")
                Lan.send(joiner.id, { t: "lobby", party: party, players: players, slots: shared,
                                      slot: slots.indexOf(joiner) })
        }
        for (const watcher of watchers)
            Lan.send(watcher.id, { t: "lobby", party: party, players: players, slots: shared, slot: -1 })
        Lan.setInfo({ mode: party ? "party" : "classic", players: players,
                      open: capacity - joiners.length })
    }

    onSlotsChanged: shareLobby()
    onWatchersChanged: shareLobby()

    function cyclePlayers(step) {
        const count = players - 3 + step
        GameSettings.partyPlayers = 3 + ((count % 4) + 4) % 4
        // Players who no longer fit leave
        while (joiners.length > capacity)
            dropJoiner(joiners[joiners.length - 1])
        SoundEffects.play(SoundEffects.MenuMove)
    }

    // A second player on the keyboard takes a side from the joiners
    function cycleKeyboards() {
        keyboards = keyboards === 1 ? 2 : 1
        while (joiners.length > capacity)
            dropJoiner(joiners[joiners.length - 1])
        SoundEffects.play(SoundEffects.MenuMove)
    }

    // Only one at a time, the LAN or the server
    function closeGame() {
        joiners = joiners.filter((j) => j.kind !== "remote")
        watchers = []
        Lan.leave()
    }

    function toggleLan() {
        SoundEffects.play(SoundEffects.MenuSelect)
        const open = lanOpen
        closeGame()
        if (!open)
            Lan.host(Lan.localName, { mode: party ? "party" : "classic", players: players, open: capacity })
    }

    function toggleOnline() {
        SoundEffects.play(SoundEffects.MenuSelect)
        const open = onlineOpen
        closeGame()
        if (!open) {
            OnlineService.check()
            Lan.hostOnline(OnlineService.server, Lan.localName, {})
        }
    }

    // A player on the network who no longer fits watches instead
    function dropJoiner(joiner) {
        joiners = joiners.filter((j) => j !== joiner)
        if (joiner.kind === "remote")
            watchers = watchers.concat([{ id: joiner.id, name: joiner.name }])
    }

    function join(pad) {
        if (joiners.some((j) => j.pad === pad) || joiners.length >= capacity)
            return
        joiners = joiners.concat([{ kind: "pad", pad: pad }])
        SoundEffects.play(SoundEffects.Pickup)
    }

    function leave(pad) {
        const joiner = joiners.find((j) => j.pad === pad)
        if (!joiner)
            return false
        dropJoiner(joiner)
        SoundEffects.play(SoundEffects.MenuMove)
        return true
    }

    function back() {
        SoundEffects.play(SoundEffects.MenuSelect)
        Lan.leave()
        joiners = []
        watchers = []
        remote = false
        phong.returnTo(root.menuScene)
    }

    function start() {
        if (remote)
            return
        SoundEffects.play(SoundEffects.MenuSelect)
        const watching = watchers.map((w) => w.id)
        if (party) {
            phong.startParty(players, slots, watching)
            return
        }
        phong.startTwoPlayers(slots[0], slots[1], watching)
    }

    // A lobby on this machine, after one of a host maybe
    function open(asParty) {
        remote = false
        remoteSlots = []
        joiners = []
        watchers = []
        keyboards = 1
        party = asParty
        players = Qt.binding(() => root.party ? GameSettings.partyPlayers : 2)
    }

    // A client gets the lobby of the host
    function showRemote(message) {
        remote = true
        party = message.party
        players = message.players
        remoteSlots = message.slots
        remoteSlot = message.slot
    }

    onActiveChanged: {
        typing = false
        held = {}
        pointerLanes = {}
        // The server may have moved since
        if (active) {
            OnlineService.refresh()
            // Wakes a sleeping server before somebody wants a room
            OnlineService.check()
            currentItem = remote ? 0 : items.findIndex((item) => item.starts === true)
        }
    }

    // Gamepads join, leave and start here instead of navigating, and try
    // the paddle
    menuNavigation: false

    Connections {
        target: Gamepads
        enabled: root.active && !root.remote
        function onButtonPressed(pad, button) {
            switch (button) {
                case Gamepad.South:
                    root.join(pad)
                    break
                case Gamepad.East:
                    if (!root.leave(pad) && !root.joiners.some((j) => j.kind === "pad"))
                        root.back()
                    break
                case Gamepad.Start:
                    root.start()
                    break
                case Gamepad.DpadLeft:
                case Gamepad.DpadRight:
                    if (root.party)
                        root.cyclePlayers(button === Gamepad.DpadLeft ? -1 : 1)
                    break
            }
        }
        function onPadsChanged() {
            // Unplugged pads leave
            root.joiners = root.joiners.filter((j) => j.kind !== "pad" || Gamepads.pads.includes(j.pad))
        }
    }

    // On a joined machine the first gamepad plays, [B] leaves
    Connections {
        target: Gamepads
        enabled: root.active && root.remote
        function onButtonPressed(pad, button) {
            if (button === Gamepad.East && pad === Gamepads.pads[0])
                root.back()
        }
    }

    // Names and practice from the players on the network
    Connections {
        target: Lan
        enabled: root.hostingLan && !root.remote && root.active
        function onReceived(peer, message) {
            switch (message.t) {
                case "practice": {
                    const index = root.slots.findIndex((slot) => slot.kind === "remote" && slot.id === peer)
                    if (index >= 0)
                        lanes.objectAt(index)?.applyState(message.s)
                    break
                }
                case "name":
                    root.rename(peer, message.name)
                    break
            }
        }
        // The host's own name, for the others to see
        function onNameChanged() {
            root.shareLobby()
        }
    }

    Connections {
        target: Lan
        enabled: root.remote
        function onNameChanged() {
            Lan.sendToHost({ t: "name", name: Lan.localName })
        }
    }

    // The fields: this machine's played, the others followed, and shared
    // a few times a second
    FrameAnimation {
        running: root.active
        onTriggered: {
            const dt = Math.min(frameTime, 0.05)
            for (let i = 0; i < lanes.count; ++i) {
                const lane = lanes.objectAt(i)
                if (!lane)
                    continue
                if (lane.local)
                    lane.input = root.laneInput(i)
                lane.step(dt)
            }
            root.sinceShared += dt
            if (root.sinceShared < 1 / 15)
                return
            root.sinceShared = 0.0
            if (root.remote) {
                const own = lanes.objectAt(root.remoteSlot)
                if (own)
                    Lan.sendToHost({ t: "practice", s: own.state() })
            }
            else if (root.hostingLan) {
                root.shareLanes()
            }
        }
    }

    Sparks {
        id: sparks
    }

    // While a game runs its scene takes the players who join
    Connections {
        target: Lan
        enabled: root.hostingLan && !root.remote && root.active
        function onPeerJoined(peer, name, token) {
            if (root.joiners.length >= root.capacity) {
                root.watchers = root.watchers.concat([{ id: peer, name: name }])
                return
            }
            root.joiners = root.joiners.concat([{ kind: "remote", id: peer, name: name, token: token }])
            SoundEffects.play(SoundEffects.Pickup)
        }
    }

    // A lost connection to the server takes the players with it
    Connections {
        target: Lan
        enabled: !root.remote
        function onRoleChanged() {
            if (Lan.role === Lan.NoRole) {
                root.joiners = root.joiners.filter((j) => j.kind !== "remote")
                root.watchers = []
            }
        }
    }

    Connections {
        target: Lan
        enabled: root.hostingLan && !root.remote
        function onPeerLeft(peer) {
            root.joiners = root.joiners.filter((j) => j.kind !== "remote" || j.id !== peer)
            root.watchers = root.watchers.filter((w) => w.id !== peer)
        }
    }

    onKeyPressed: (event) => {
        event.accepted = true
        if (typing) {
            if (event.gamepad && event.key !== Qt.Key_Escape && event.key !== Qt.Key_Return)
                return
            if (event.key === Qt.Key_Escape) {
                typing = false
            }
            else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                GameSettings.playerName = typed
                SoundEffects.play(SoundEffects.MenuSelect)
                typing = false
            }
            else if (event.key === Qt.Key_Backspace) {
                typed = typed.slice(0, -1)
            }
            else if (event.text.length === 1 && event.text >= " " && typed.length < 12) {
                typed += event.text
            }
            return
        }
        if (!event.isAutoRepeat && practiceKey(event.key, true))
            return
        const item = items[currentItem]
        switch (event.key) {
            case Qt.Key_Escape:
                back()
                break
            case Qt.Key_Up:
                currentItem = Math.max(currentItem - 1, 0)
                SoundEffects.play(SoundEffects.MenuMove)
                break
            case Qt.Key_Down:
                currentItem = Math.min(currentItem + 1, items.length - 1)
                SoundEffects.play(SoundEffects.MenuMove)
                break
            case Qt.Key_Left:
            case Qt.Key_Right:
                if (item?.cycles)
                    item.change(event.key === Qt.Key_Left ? -1 : 1)
                break
            case Qt.Key_Enter:
            case Qt.Key_Return:
            case Qt.Key_Space:
                if (event.isAutoRepeat)
                    break
                if (item?.activate)
                    item.activate()
                else if (item?.cycles)
                    item.change(1)
                break
        }
    }
    onKeyReleased: (event) => {
        event.accepted = true
        if (!event.isAutoRepeat)
            practiceKey(event.key, false)
    }

    // A click on the menu, or a finger in the own field steering the paddle
    onPointerPressed: (id, x, y) => {
        const clickable = phong.clickableAt(x, y)
        if (clickable) {
            clickable.clicked()
            return
        }
        const at = root.mapPositionToScene(phong.toScene(x, y, root))
        for (let i = 0; i < lanes.count; ++i) {
            const lane = lanes.objectAt(i)
            if (lane?.local && !lane.computer && lane.contains(at)) {
                const steering = pointerLanes
                steering[id] = i
                pointerLanes = steering
                lane.pointer = lane.fromScene(at).y
                return
            }
        }
    }
    onPointerMoved: (id, x, y) => {
        const lane = lanes.objectAt(pointerLanes[id] ?? -1)
        if (lane)
            lane.pointer = lane.fromScene(root.mapPositionToScene(phong.toScene(x, y, root))).y
    }
    onPointerReleased: (id) => {
        const lane = lanes.objectAt(pointerLanes[id] ?? -1)
        if (lane)
            lane.pointer = NaN
        const steering = pointerLanes
        delete steering[id]
        pointerLanes = steering
    }

    Text3D {
        y: 8.6
        scale: Qt.vector3d(1.8, 1.8, 1.8)
        horizontalAlignment: Text.AlignHCenter
        color: Theme.title
        glow: 0.8
        text: root.party ? qsTr("%1 Players").arg(root.players) : qsTr("2 Players")
    }

    // The code others join the room with
    Text3D {
        visible: root.onlineOpen && root.hostingLan && !root.remote
        y: 6.2
        scale: Qt.vector3d(0.9, 0.9, 0.9)
        horizontalAlignment: Text.AlignHCenter
        color: Theme.title
        glow: 0.6
        text: qsTr("Room code %1").arg(Lan.roomCode)
    }

    // The sides, who plays them, and their fields to practice in
    Repeater3D {
        id: lanes
        // By count, a new player doesn't start the others' fields over
        model: root.slots.length

        delegate: PracticeLane {
            id: lane

            required property int index
            readonly property var slot: root.slots[index] ?? null

            position: Qt.vector3d(root.laneX(index), root.laneY, 0)
            depth: root.laneDepth
            span: root.laneSpan
            orientation: root.party ? PracticeLane.Bottom : index === 0 ? PracticeLane.Left : PracticeLane.Right
            color: root.colors[index]
            local: root.remote ? index === root.remoteSlot : (slot?.kind ?? "remote") !== "remote"
            computer: !root.remote && slot?.kind === "cpu"
            onLocalChanged: reset()
            onHit: (position) => {
                sparks.burst(root.mapPositionFromScene(position), lane.color, 10)
                if (lane.local && !lane.computer)
                    SoundEffects.play(SoundEffects.PaddleHit)
            }

            // Who plays it, and how
            Text3D {
                readonly property real fitting: Math.min(0.7, (lane.screenWidth - 0.3) / Math.max(textWidth, 0.1))
                y: -0.5 * lane.screenHeight - 1.0
                scale: Qt.vector3d(fitting, fitting, fitting)
                horizontalAlignment: Text.AlignHCenter
                color: lane.color
                glow: 0.3
                text: root.displayName(lane.slot, lane.index)
            }

            Text3D {
                readonly property real fitting: Math.min(0.45, (lane.screenWidth - 0.3) / Math.max(textWidth, 0.1))
                y: -0.5 * lane.screenHeight - 1.75
                scale: Qt.vector3d(fitting, fitting, fitting)
                horizontalAlignment: Text.AlignHCenter
                color: root.remote && lane.index === root.remoteSlot ? Theme.title : Theme.dimmed
                text: root.slotDetail(lane.slot, lane.index)
            }
        }
    }

    Text3D {
        y: -3.1
        scale: Qt.vector3d(0.5, 0.5, 0.5)
        horizontalAlignment: Text.AlignHCenter
        color: root.hostingLan ? Theme.title : Theme.dimmed
        readonly property string watching: root.watchers.length > 0 ? qsTr(", %n watching", "", root.watchers.length) : ""
        text: root.remote ? (root.remoteSlot < 0 ? qsTr("Every side is taken, you watch once the host starts")
                                                 : qsTr("Waiting for the host to start"))
              : root.lanOpen ? qsTr("Open on the LAN, from a browser join %1").arg(Lan.addresses.slice(0, 2).join(qsTr(" or ")))
                               + watching
              : root.onlineOpen && root.hostingLan ? qsTr("Open online, others join with the room code") + watching
              // A sleeping server takes a while, say so rather than nothing
              : OnlineService.status !== "" && OnlineService.available ? OnlineService.status
              : root.onlineOpen ? qsTr("Opening a room on the server")
              : Lan.error !== "" ? Lan.error
              : ""
    }

    // How to try the paddle: the movement keys the menu doesn't use
    Text3D {
        y: -3.8
        scale: Qt.vector3d(0.45, 0.45, 0.45)
        horizontalAlignment: Text.AlignHCenter
        color: Theme.dimmed
        readonly property string keys: root.party
            ? (KeySettings.partyKeyNames.length > 1 ? KeySettings.partyKeyNames[0] + "/" + KeySettings.partyKeyNames[1] : "")
            : KeySettings.keyNames[KeySettings.LeftUp] + "/" + KeySettings.keyNames[KeySettings.LeftDown]
        text: qsTr("Warm up while you wait: [%1], a gamepad, or drag in your field").arg(keys)
    }

    Text3D {
        y: -4.4
        visible: !root.remote
        scale: Qt.vector3d(0.45, 0.45, 0.45)
        horizontalAlignment: Text.AlignHCenter
        color: Theme.dimmed
        text: Gamepads.available ? qsTr("A gamepad joins with [A] and leaves with [B], [Start] starts")
                                 : qsTr("Gamepads need a build with SDL 3")
    }

    Repeater3D {
        model: root.items

        delegate: Text3D {
            id: item

            required property var modelData
            required property int index

            readonly property bool selected: index === root.currentItem

            y: -5.5 - index * 1.08
            scale: Qt.vector3d(0.9, 0.9, 0.9)
            horizontalAlignment: Text.AlignHCenter
            text: modelData.text + (modelData.value !== undefined ? "  " + modelData.value : "")
            clickable: true
            onClicked: {
                root.currentItem = item.index
                if (item.modelData.activate)
                    item.modelData.activate()
                else
                    item.modelData.change(1)
            }

            Disc {
                visible: item.selected
                position: Qt.vector3d(-0.5 * item.textWidth - (item.modelData.cycles ? 2.4 : 1.0), 0.35, 0)
                radius: 0.35
                sphere: true
            }

            Text3D {
                visible: item.selected && (item.modelData.cycles ?? false)
                x: -0.5 * item.textWidth - 0.9
                horizontalAlignment: Text.AlignRight
                color: Theme.title
                text: "<"
            }

            Text3D {
                visible: item.selected && (item.modelData.cycles ?? false)
                x: 0.5 * item.textWidth + 0.9
                color: Theme.title
                text: ">"
            }
        }
    }

    Text3D {
        y: -13.3
        scale: Qt.vector3d(0.5, 0.5, 0.5)
        horizontalAlignment: Text.AlignHCenter
        color: Theme.dimmed
        text: root.typing ? qsTr("Type your name   [Enter] keep it   [Esc] cancel")
                          : qsTr("[Up/Down] select   [Enter] confirm   [Esc] back")
    }
}
