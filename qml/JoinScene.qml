pragma ComponentBehavior: Bound

import QtQuick
import QtQuick3D
import Phong

// Joining a game: the hosts found on the LAN, one by its address, or a
// room on the server by its code. Browsers can't look for hosts, they take
// the address or the code.
Scene {
    id: root

    property Scene menuScene
    property int currentItem: 0
    property string address: GameSettings.lanAddress
    property string code: ""
    // Why the last try didn't work
    property string error: ""

    readonly property bool joining: Lan.role === Lan.Joining
    readonly property var items: {
        const items = Lan.games.map((game) => ({
            text: game.name,
            // A game going on or without a free side can be watched
            detail: qsTr("%1 players, %2").arg(game.info.mode === "party" ? game.info.players : 2)
                    .arg(game.info.playing ? qsTr("playing, to watch")
                         : game.info.open > 0 ? qsTr("%1 free").arg(game.info.open) : qsTr("full, to watch")),
            activate: () => root.join(game.url)
        }))
        // Only with a server, see OnlineService
        if (OnlineService.available)
            items.push({ text: qsTr("Room code"), code: true, activate: () => root.joinRoom() })
        items.push({ text: qsTr("Address"), address: true, activate: () => root.join(root.address) })
        items.push({ text: qsTr("Back"), activate: () => root.back() })
        return items
    }

    function join(url) {
        if (url === "" || joining)
            return
        SoundEffects.play(SoundEffects.MenuSelect)
        error = ""
        if (url === address)
            GameSettings.lanAddress = address
        Lan.join(url, Lan.localName)
    }

    // A room on the server
    function joinRoom() {
        if (code.length < 4 || joining)
            return
        SoundEffects.play(SoundEffects.MenuSelect)
        error = ""
        Lan.joinOnline(OnlineService.server, code, Lan.localName)
    }

    function back() {
        SoundEffects.play(SoundEffects.MenuSelect)
        Lan.leave()
        phong.returnTo(root.menuScene)
    }

    onActiveChanged: {
        if (active) {
            error = ""
            currentItem = 0
            OnlineService.refresh()
            Lan.startBrowsing()
        }
        else {
            Lan.stopBrowsing()
        }
    }

    onKeyPressed: (event) => {
        event.accepted = true
        const item = items[currentItem]
        switch (event.key) {
            case Qt.Key_Escape:
                back()
                return
            case Qt.Key_Up:
                currentItem = Math.max(currentItem - 1, 0)
                SoundEffects.play(SoundEffects.MenuMove)
                return
            case Qt.Key_Down:
                currentItem = Math.min(currentItem + 1, items.length - 1)
                SoundEffects.play(SoundEffects.MenuMove)
                return
            case Qt.Key_Enter:
            case Qt.Key_Return:
                if (!event.isAutoRepeat)
                    item.activate()
                return
            case Qt.Key_Backspace:
                if (item.address)
                    address = address.slice(0, -1)
                else if (item.code)
                    code = code.slice(0, -1)
                return
        }

        // The address and the code are typed in their rows
        if (item.address && !event.gamepad && /^[0-9A-Za-z.:\-]$/.test(event.text ?? ""))
            address += event.text
        else if (item.code && !event.gamepad && code.length < 4 && /^[A-Za-z]$/.test(event.text ?? ""))
            code += event.text.toUpperCase()
    }
    onPointerPressed: (id, x, y) => phong.clickableAt(x, y)?.clicked()

    Text3D {
        y: 8.6
        scale: Qt.vector3d(1.8, 1.8, 1.8)
        horizontalAlignment: Text.AlignHCenter
        color: Theme.title
        glow: 0.8
        text: qsTr("Join")
    }

    Text3D {
        y: 6.4
        scale: Qt.vector3d(0.55, 0.55, 0.55)
        horizontalAlignment: Text.AlignHCenter
        color: root.error !== "" ? Theme.accent : Theme.dimmed
        text: root.joining ? qsTr("Joining...")
              : root.error !== "" ? root.error
              : Lan.canHost ? (Lan.games.length ? qsTr("Games on the LAN") : qsTr("Looking for games on the LAN"))
              : qsTr("Type the address the host shows")
    }

    Repeater3D {
        model: root.items

        delegate: Node {
            id: row

            required property var modelData
            required property int index

            readonly property bool selected: index === root.currentItem

            y: 4.4 - index * 1.6

            Disc {
                visible: row.selected
                sphere: true
                position: Qt.vector3d(-12.0, 0.35, 0)
                radius: 0.35
            }

            Text3D {
                x: -11.0
                color: row.selected ? Theme.text : Theme.dimmed
                text: row.modelData.text
                clickable: true
                onClicked: {
                    root.currentItem = row.index
                    row.modelData.activate()
                }
            }

            // The address as typed, with a cursor
            Text3D {
                visible: row.modelData.address ?? false
                x: 11.0
                horizontalAlignment: Text.AlignRight
                color: row.selected ? Theme.title : Theme.text
                text: (root.address !== "" ? root.address : qsTr("host:45455")) + (row.selected ? "_" : "")
            }

            Text3D {
                visible: row.modelData.code ?? false
                x: 11.0
                horizontalAlignment: Text.AlignRight
                color: row.selected ? Theme.title : Theme.text
                text: (root.code !== "" ? root.code : qsTr("ABCD")) + (row.selected ? "_" : "")
            }

            Text3D {
                visible: row.modelData.detail !== undefined
                x: 11.0
                scale: Qt.vector3d(0.6, 0.6, 0.6)
                horizontalAlignment: Text.AlignRight
                color: Theme.dimmed
                text: row.modelData.detail ?? ""
            }
        }
    }

    Text3D {
        y: -11.4
        scale: Qt.vector3d(0.5, 0.5, 0.5)
        horizontalAlignment: Text.AlignHCenter
        color: Theme.dimmed
        text: qsTr("[Up/Down] select   type the code or the address   [Enter] join   [Esc] back")
    }
}
