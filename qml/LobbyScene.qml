pragma ComponentBehavior: Bound

import QtQuick
import QtQuick3D
import Phong

// Before a game of several players: who plays which side. The keyboard is
// the first player, a gamepad joins with A and leaves with B, the free
// sides are played by the computer.
Scene {
    id: root

    property Scene menuScene
    // Two players on the classic field, or three to six on a polygon
    property bool party: false
    property int players: party ? GameSettings.partyPlayers : 2
    property int currentItem: 0
    // The joined gamepads, in the order they joined
    property var pads: []

    readonly property var colors: [Theme.leftPlayer, Theme.rightPlayer, Theme.tint("#ffd24d"),
                                   Theme.tint("#66ff66"), Theme.tint("#ff8c1a"), Theme.tint("#a64dff")]

    // The side of every player: with two players the keyboard shares the
    // field, a pad takes the right side first, then the left one
    readonly property var slots: {
        const slots = []
        if (!party) {
            slots.push(pads.length > 1 ? { kind: "pad", pad: pads[1] } : { kind: "keyboard", keys: "left" })
            slots.push(pads.length > 0 ? { kind: "pad", pad: pads[0] } : { kind: "keyboard", keys: "right" })
            return slots
        }
        slots.push({ kind: "keyboard" })
        for (let i = 1; i < players; ++i)
            slots.push(i - 1 < pads.length ? { kind: "pad", pad: pads[i - 1] } : { kind: "cpu" })
        return slots
    }

    readonly property var items: {
        const items = []
        if (party)
            items.push({ text: qsTr("Players"), cycles: true })
        items.push({ text: qsTr("Start"), activate: () => root.start() })
        items.push({ text: qsTr("Back"), activate: () => phong.returnTo(root.menuScene) })
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
                return qsTr("Keyboard")
            case "pad":
                return slot.pad?.name ?? qsTr("Gamepad")
            default:
                return qsTr("Computer")
        }
    }

    function cyclePlayers(step) {
        const count = players - 3 + step
        GameSettings.partyPlayers = 3 + ((count % 4) + 4) % 4
        SoundEffects.play(SoundEffects.MenuMove)
    }

    function join(pad) {
        if (pads.includes(pad) || pads.length >= (party ? players - 1 : 2))
            return
        pads = pads.concat([pad])
        SoundEffects.play(SoundEffects.Pickup)
    }

    function leave(pad) {
        if (!pads.includes(pad))
            return false
        pads = pads.filter((p) => p !== pad)
        SoundEffects.play(SoundEffects.MenuMove)
        return true
    }

    function start() {
        SoundEffects.play(SoundEffects.MenuSelect)
        if (party)
            phong.startParty(players, slots)
        else
            phong.startTwoPlayers(slots[0].kind === "pad" ? slots[0].pad : null,
                                  slots[1].kind === "pad" ? slots[1].pad : null)
    }

    onActiveChanged: {
        if (active)
            currentItem = party ? 1 : 0
    }

    // Gamepads join, leave and start here instead of navigating
    menuNavigation: false

    Connections {
        target: Gamepads
        enabled: root.active
        function onButtonPressed(pad, button) {
            switch (button) {
                case Gamepad.South:
                    root.join(pad)
                    break
                case Gamepad.East:
                    if (!root.leave(pad) && root.pads.length === 0)
                        phong.returnTo(root.menuScene)
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
            root.pads = root.pads.filter((pad) => Gamepads.pads.includes(pad))
        }
    }

    onKeyPressed: (event) => {
        event.accepted = true
        switch (event.key) {
            case Qt.Key_Escape:
                phong.returnTo(root.menuScene)
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
                if (items[currentItem].cycles)
                    cyclePlayers(event.key === Qt.Key_Left ? -1 : 1)
                break
            case Qt.Key_Enter:
            case Qt.Key_Return:
            case Qt.Key_Space:
                if (!event.isAutoRepeat && items[currentItem].activate)
                    items[currentItem].activate()
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

            y: 5.6 - index * 1.5

            Disc {
                position: Qt.vector3d(-11.0, 0.3, 0)
                radius: 0.35
                sphere: true
                color: root.colors[slot.index]
                glow: 0.5
            }

            Text3D {
                x: -10.0
                scale: Qt.vector3d(0.8, 0.8, 0.8)
                color: root.colors[slot.index]
                text: root.party ? qsTr("Player %1").arg(slot.index + 1)
                                 : slot.index === 0 ? qsTr("Left") : qsTr("Right")
            }

            Text3D {
                x: 11.0
                scale: Qt.vector3d(0.8, 0.8, 0.8)
                horizontalAlignment: Text.AlignRight
                color: slot.modelData.kind === "cpu" ? Theme.dimmed : Theme.text
                text: root.slotName(slot.modelData)
            }
        }
    }

    Text3D {
        y: -4.0
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

            y: -6.0 - index * 1.8
            horizontalAlignment: Text.AlignHCenter
            text: modelData.cycles ? modelData.text + "  " + root.players : modelData.text
            clickable: true
            onClicked: {
                root.currentItem = item.index
                if (item.modelData.cycles)
                    root.cyclePlayers(1)
                else
                    item.modelData.activate()
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
