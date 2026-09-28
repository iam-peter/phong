pragma ComponentBehavior: Bound

import QtQuick
import QtQuick3D

Scene {
    id: root

    property Scene settingsScene
    property int currentItem: 0

    readonly property var items: {
        const items = [
            { text: qsTr("1 Player"), activate: () => phong.startGame(GameScene.OnePlayer) },
            { text: qsTr("2 Players"), activate: () => phong.startGame(GameScene.TwoPlayers) },
            { text: qsTr("Settings"), activate: () => phong.nextScene(root.settingsScene) }
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
                break
            case Qt.Key_Down:
                currentItem = Math.min(currentItem + 1, items.length - 1)
                break
            case Qt.Key_Enter:
            case Qt.Key_Return:
            case Qt.Key_Space:
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
        }

        Disc {
            position: Qt.vector3d(0.1, 2.5, 0)
            radius: 0.7
        }

        Text3D {
            position: Qt.vector3d(1, 2, 0)
            text: "ng"
        }
    }

    Repeater3D {
        model: root.items

        delegate: Text3D {
            id: item

            required property var modelData
            required property int index

            y: index * -2
            horizontalAlignment: Text.AlignHCenter
            text: modelData.text
            clickable: true
            onClicked: {
                root.currentItem = item.index
                item.modelData.activate()
            }

            // Selection marker
            Disc {
                visible: item.index === root.currentItem
                position: Qt.vector3d(-0.5 * item.textWidth - 1.0, 0.35, 0)
                radius: 0.4
            }
        }
    }

    Text3D {
        y: -11
        scale: Qt.vector3d(0.5, 0.5, 0.5)
        horizontalAlignment: Text.AlignHCenter
        text: qsTr("[Up/Down] select   [Enter] confirm")
        color: Theme.dimmed
    }
}
