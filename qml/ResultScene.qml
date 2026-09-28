pragma ComponentBehavior: Bound

import QtQuick
import QtQuick3D
import Phong

Scene {
    id: root

    property Match match
    property Scene menuScene
    property int mode: GameScene.OnePlayer
    property int currentItem: 0

    readonly property var items: [
        { text: qsTr("Rematch"), activate: () => phong.rematch() },
        { text: qsTr("Menu"), activate: () => phong.returnTo(root.menuScene) }
    ]

    readonly property string headline: {
        const winner = match?.winner
        if (!winner)
            return ""
        if (mode === GameScene.OnePlayer)
            return winner.computer ? qsTr("CPU wins") : qsTr("You win")
        return qsTr("%1 wins").arg(winner.name)
    }

    function formatTime(seconds) {
        const total = Math.round(seconds)
        const remainder = total % 60
        return Math.floor(total / 60) + ":" + (remainder < 10 ? "0" : "") + remainder
    }

    onActiveChanged: {
        if (active)
            currentItem = 0
    }

    onKeyPressed: (event) => {
        event.accepted = true
        switch (event.key) {
            case Qt.Key_Escape:
                items[1].activate()
                break
            case Qt.Key_Left:
            case Qt.Key_Up:
                currentItem = 0
                break
            case Qt.Key_Right:
            case Qt.Key_Down:
                currentItem = 1
                break
            case Qt.Key_Enter:
            case Qt.Key_Return:
            case Qt.Key_Space:
                if (!event.isAutoRepeat)
                    items[currentItem].activate()
                break
        }
    }
    onPointerPressed: (id, x, y) => phong.clickableAt(x, y)?.clicked()

    Text3D {
        y: 7.0
        scale: Qt.vector3d(2, 2, 2)
        horizontalAlignment: Text.AlignHCenter
        color: Theme.title
        text: root.headline
    }

    // Final score, laid out like the scoreboard
    Node {
        y: 2.5
        scale: Qt.vector3d(2, 2, 2)

        Text3D {
            x: -1.0
            horizontalAlignment: Text.AlignRight
            text: root.match?.left.score ?? 0
        }

        Text3D {
            horizontalAlignment: Text.AlignHCenter
            text: ":"
        }

        Text3D {
            x: 1.0
            text: root.match?.right.score ?? 0
        }

        Text3D {
            x: -8.5
            y: 0.1
            scale: Qt.vector3d(0.5, 0.5, 0.5)
            color: Theme.dimmed
            text: root.match?.left.name ?? ""
        }

        Text3D {
            x: 8.5
            y: 0.1
            scale: Qt.vector3d(0.5, 0.5, 0.5)
            horizontalAlignment: Text.AlignRight
            color: Theme.dimmed
            text: root.match?.right.name ?? ""
        }
    }

    Repeater3D {
        model: [
            [qsTr("Longest rally"), root.match?.longestRally ?? 0],
            [qsTr("Paddle hits"), root.match?.totalHits ?? 0],
            [qsTr("Match time"), root.formatTime(root.match?.playTime ?? 0)]
        ]

        delegate: Node {
            id: stat

            required property var modelData
            required property int index

            y: -1.0 - index * 2.0

            Text3D {
                x: -9.0
                color: Theme.dimmed
                text: stat.modelData[0]
            }

            Text3D {
                x: 9.0
                horizontalAlignment: Text.AlignRight
                text: stat.modelData[1]
            }
        }
    }

    Repeater3D {
        model: root.items

        delegate: Text3D {
            id: item

            required property var modelData
            required property int index

            x: index === 0 ? -5.0 : 5.0
            y: -9.0
            horizontalAlignment: Text.AlignHCenter
            text: modelData.text
            clickable: true
            onClicked: item.modelData.activate()

            Disc {
                visible: item.index === root.currentItem
                position: Qt.vector3d(-0.5 * item.textWidth - 1.0, 0.35, 0)
                radius: 0.4
            }
        }
    }

    Text3D {
        y: -11.5
        scale: Qt.vector3d(0.5, 0.5, 0.5)
        horizontalAlignment: Text.AlignHCenter
        text: qsTr("[Left/Right] select   [Enter] confirm   [Esc] menu")
        color: Theme.dimmed
    }
}
