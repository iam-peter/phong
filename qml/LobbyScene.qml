pragma ComponentBehavior: Bound

import QtQuick
import QtQuick3D
import Phong

// Before a game of several players: who plays which side. The keyboard is
// the first player, a gamepad joins with A and leaves with B, players on
// the network join while the game is open on the LAN, the free sides of
// the polygon are played by the computer. On a joined machine it shows the
// lobby of the host.
Scene {
    id: root

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

    // Joined to a host: its lobby as it sends it
    property bool remote: false
    property var remoteSlots: []
    property int remoteSlot: -1

    readonly property bool hostingLan: Lan.role === Lan.Host
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

    readonly property var items: {
        const items = []
        if (remote) {
            items.push({ text: qsTr("Leave"), activate: () => root.back() })
            return items
        }
        if (party) {
            items.push({ text: qsTr("Players"), value: players, cycles: true, change: (step) => root.cyclePlayers(step) })
            items.push({ text: qsTr("Keyboards"), value: keyboards, cycles: true, change: () => root.cycleKeyboards() })
        }
        if (Lan.canHost)
            items.push({ text: hostingLan ? qsTr("LAN open") : qsTr("LAN closed"), cycles: true,
                         change: () => root.toggleLan() })
        items.push({ text: qsTr("Start"), activate: () => root.start() })
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
                return qsTr("%1 on the LAN").arg(slot.name)
            case "host":
                return qsTr("%1, the host").arg(slot.name)
            default:
                return qsTr("Computer")
        }
    }

    // What the players on the network see of the slots
    function sharedSlots() {
        return slots.map((slot) => {
            if (slot.kind === "remote")
                return { kind: "remote", name: slot.name, id: slot.id }
            if (slot.kind === "cpu")
                return { kind: "cpu" }
            return { kind: "host", name: Lan.machineName }
        })
    }

    function shareLobby() {
        if (!hostingLan)
            return
        const shared = sharedSlots()
        for (const joiner of joiners) {
            if (joiner.kind === "remote")
                Lan.send(joiner.id, { t: "lobby", party: party, players: players, slots: shared,
                                      slot: slots.indexOf(joiner) })
        }
        Lan.setInfo({ mode: party ? "party" : "classic", players: players,
                      open: capacity - joiners.length })
    }

    onSlotsChanged: shareLobby()

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

    function toggleLan() {
        SoundEffects.play(SoundEffects.MenuSelect)
        if (hostingLan) {
            for (const joiner of joiners.filter((j) => j.kind === "remote"))
                dropJoiner(joiner)
            Lan.leave()
        }
        else {
            Lan.host(Lan.machineName, { mode: party ? "party" : "classic", players: players, open: capacity })
        }
    }

    function dropJoiner(joiner) {
        joiners = joiners.filter((j) => j !== joiner)
        if (joiner.kind === "remote")
            Lan.kick(joiner.id)
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
        remote = false
        phong.returnTo(root.menuScene)
    }

    function start() {
        if (remote)
            return
        SoundEffects.play(SoundEffects.MenuSelect)
        if (party) {
            phong.startParty(players, slots)
            return
        }
        phong.startTwoPlayers(slots[0], slots[1])
    }

    // A lobby on this machine, after one of a host maybe
    function open(asParty) {
        remote = false
        remoteSlots = []
        joiners = []
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
        if (active)
            currentItem = remote ? 0 : items.findIndex((item) => item.activate !== undefined && !item.cycles)
    }

    // Gamepads join, leave and start here instead of navigating
    menuNavigation: remote

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

    Connections {
        target: Lan
        enabled: root.hostingLan && !root.remote
        function onPeerJoined(peer, name) {
            if (root.joiners.length >= root.capacity) {
                Lan.send(peer, { t: "full" })
                Lan.kick(peer)
                return
            }
            root.joiners = root.joiners.concat([{ kind: "remote", id: peer, name: name }])
            SoundEffects.play(SoundEffects.Pickup)
        }
        function onPeerLeft(peer) {
            root.joiners = root.joiners.filter((j) => j.kind !== "remote" || j.id !== peer)
        }
    }

    onKeyPressed: (event) => {
        event.accepted = true
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
    onPointerPressed: (id, x, y) => phong.clickableAt(x, y)?.clicked()

    Text3D {
        y: 8.6
        scale: Qt.vector3d(1.8, 1.8, 1.8)
        horizontalAlignment: Text.AlignHCenter
        color: Theme.title
        glow: 0.8
        text: root.party ? qsTr("%1 Players").arg(root.players) : qsTr("2 Players")
    }

    // The sides and who plays them
    Repeater3D {
        model: root.slots

        delegate: Node {
            id: slot

            required property var modelData
            required property int index

            y: 5.8 - index * 1.4

            Disc {
                position: Qt.vector3d(-11.0, 0.3, 0)
                radius: 0.35
                sphere: true
                color: root.colors[slot.index]
                glow: 0.5
            }

            Text3D {
                x: -10.0
                scale: Qt.vector3d(0.75, 0.75, 0.75)
                color: root.colors[slot.index]
                text: root.party ? qsTr("Player %1").arg(slot.index + 1)
                                 : slot.index === 0 ? qsTr("Left") : qsTr("Right")
            }

            Text3D {
                x: 11.0
                scale: Qt.vector3d(0.75, 0.75, 0.75)
                horizontalAlignment: Text.AlignRight
                color: slot.modelData.kind === "cpu" ? Theme.dimmed
                       : root.remote && slot.index === root.remoteSlot ? Theme.title : Theme.text
                text: root.remote && slot.index === root.remoteSlot ? qsTr("You") : root.slotName(slot.modelData)
            }
        }
    }

    Text3D {
        y: -2.8
        scale: Qt.vector3d(0.5, 0.5, 0.5)
        horizontalAlignment: Text.AlignHCenter
        color: root.hostingLan ? Theme.title : Theme.dimmed
        text: root.remote ? qsTr("Waiting for the host to start")
              : root.hostingLan ? qsTr("Open on the LAN, from a browser join %1").arg(Lan.addresses.slice(0, 2).join(qsTr(" or ")))
              : Lan.error !== "" ? Lan.error
              : ""
    }

    Text3D {
        y: -3.8
        visible: !root.remote
        scale: Qt.vector3d(0.5, 0.5, 0.5)
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

            y: -5.4 - index * 1.3
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
        y: -11.4
        scale: Qt.vector3d(0.5, 0.5, 0.5)
        horizontalAlignment: Text.AlignHCenter
        color: Theme.dimmed
        text: qsTr("[Up/Down] select   [Enter] confirm   [Esc] back")
    }
}
