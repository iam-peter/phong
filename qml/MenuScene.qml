pragma ComponentBehavior: Bound

import QtQuick
import QtQuick3D
import Phong

Scene {
    id: root

    property Scene settingsScene
    property Scene statsScene
    property int currentItem: 0

    // The game modes are cycled through like a setting. Three players and
    // more play on a polygon of their own.
    readonly property var modes: [
        { name: qsTr("1 Player"), mode: GameScene.OnePlayer },
        { name: qsTr("2 Players"), lobby: "classic" },
        { name: qsTr("3-6 Players"), lobby: "party" },
        { name: qsTr("Ladder"), mode: GameScene.Ladder },
        { name: qsTr("Endless"), mode: GameScene.Endless },
        { name: qsTr("Tournament"), mode: GameScene.Tournament },
        { name: qsTr("Bricks"), mode: GameScene.Bricks },
        { name: qsTr("Squash"), mode: GameScene.Squash }
    ]
    readonly property int mode: Math.min(Math.max(GameSettings.mode, 0), modes.length - 1)

    function play(entry) {
        if (entry.lobby)
            phong.openLobby(entry.lobby === "party")
        else
            phong.startGame(entry.mode)
    }

    function cycleMode(step) {
        GameSettings.mode = (mode + step + modes.length) % modes.length
        SoundEffects.play(SoundEffects.MenuMove)
    }

    readonly property var items: {
        const items = [
            { text: modes[mode].name, cycles: true, activate: () => root.play(root.modes[root.mode]) },
            { text: qsTr("Join game"), activate: () => phong.openJoin() },
            { text: qsTr("Settings"), activate: () => phong.nextScene(root.settingsScene) },
            { text: qsTr("Stats"), activate: () => phong.nextScene(root.statsScene) }
        ]
        // Closing the tab is how you quit a web page
        if (Qt.platform.os !== "wasm")
            items.push({ text: qsTr("Quit"), activate: () => Qt.quit() })
        return items
    }

    onKeyPressed: (event) => {
        event.accepted = true
        switch (event.key) {
            case Qt.Key_Escape:
                phong.previousScene()
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
                if (items[currentItem].cycles)
                    cycleMode(-1)
                break
            case Qt.Key_Right:
                if (items[currentItem].cycles)
                    cycleMode(1)
                break
            case Qt.Key_Enter:
            case Qt.Key_Return:
            case Qt.Key_Space:
                SoundEffects.play(SoundEffects.MenuSelect)
                items[currentItem].activate()
                break
        }
    }
    onPointerPressed: (id, x, y) => phong.clickableAt(x, y)?.clicked()

    // Title, the O of P(H)ONG is a ball
    Node {
        x: 0.8
        scale: Qt.vector3d(2, 2, 2)

        Text3D {
            position: Qt.vector3d(-1, 2, 0)
            horizontalAlignment: Text.AlignRight
            text: "p(h)"
            glow: 0.5
        }

        // The O is a Phong shaded ball
        Disc {
            position: Qt.vector3d(0.1, 2.5, 0)
            radius: 0.7
            sphere: true
            glow: 0.2
            shininess: 1.0
        }

        Text3D {
            position: Qt.vector3d(1, 2, 0)
            text: "ng"
            glow: 0.5
        }
    }

    Repeater3D {
        model: root.items

        delegate: Text3D {
            id: item

            required property var modelData
            required property int index

            y: 0.5 - index * 2.0
            horizontalAlignment: Text.AlignHCenter
            text: modelData.text
            clickable: true
            onClicked: {
                SoundEffects.play(SoundEffects.MenuSelect)
                root.currentItem = item.index
                item.modelData.activate()
            }

            // The mode cycles like a setting, the arrows wrap around it
            readonly property bool arrows: item.index === root.currentItem && (modelData.cycles ?? false)

            Text3D {
                visible: item.arrows
                x: -0.5 * item.textWidth - 0.9
                horizontalAlignment: Text.AlignRight
                color: Theme.title
                text: "<"
                clickable: visible
                onClicked: root.cycleMode(-1)
            }

            Text3D {
                visible: item.arrows
                x: 0.5 * item.textWidth + 0.9
                color: Theme.title
                text: ">"
                clickable: visible
                onClicked: root.cycleMode(1)
            }

            // Selection marker
            Disc {
                visible: item.index === root.currentItem
                sphere: true
                position: Qt.vector3d(-0.5 * item.textWidth - (item.arrows ? 2.8 : 1.0), 0.35, 0)
                radius: 0.35
            }
        }
    }

    Text3D {
        y: -11
        scale: Qt.vector3d(0.5, 0.5, 0.5)
        horizontalAlignment: Text.AlignHCenter
        text: root.currentItem === 0 ? qsTr("[Left/Right] mode   [Enter] play")
                                : qsTr("[Up/Down] select   [Enter] confirm")
        color: Theme.dimmed
    }
}
