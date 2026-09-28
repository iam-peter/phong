pragma ComponentBehavior: Bound

import QtQuick
import QtQuick3D
import Phong

Scene {
    id: root

    property Match match
    property Scene menuScene
    property int mode: GameScene.OnePlayer
    property int ladderStage: 0
    property int endlessScore: 0
    property bool newHighScore: false
    // Names of the achievements of the match
    property var achievements: []
    property int currentItem: 0
    // Joined to a host on the network
    property bool remote: false

    readonly property bool won: match?.winner === match?.left
    readonly property bool ladder: mode === GameScene.Ladder
    readonly property bool endless: mode === GameScene.Endless
    readonly property bool champion: ladder && won && ladderStage >= 2
    readonly property bool tournament: mode === GameScene.Tournament
    readonly property bool squash: mode === GameScene.Squash

    // The last item always leads back to the menu
    readonly property var items: {
        const menu = { text: qsTr("Menu"), activate: () => { Lan.leave(); phong.returnTo(root.menuScene) } }
        // Joined over the network the host decides on a rematch
        if (remote)
            return [menu]
        if (tournament) {
            if (Tournament.champion)
                return [{ text: qsTr("Bracket"), activate: () => phong.showBracket() }, menu]
            if (won)
                return [{ text: qsTr("Next round"), activate: () => phong.showBracket() }, menu]
            return [{ text: qsTr("Retry"), activate: () => phong.restartTournament() }, menu]
        }
        if (champion)
            return [menu]
        if (ladder && won)
            return [{ text: qsTr("Next level"), activate: () => phong.nextLadderLevel() }, menu]
        if (ladder || endless || squash)
            return [{ text: qsTr("Retry"), activate: () => phong.rematch() }, menu]
        return [{ text: qsTr("Rematch"), activate: () => phong.rematch() }, menu]
    }

    readonly property string headline: {
        const winner = match?.winner
        if (!winner)
            return ""
        if (endless || squash)
            return newHighScore ? qsTr("New high score") : qsTr("Game over")
        if (champion || (tournament && Tournament.champion))
            return qsTr("Champion")
        if (tournament)
            return won ? qsTr("Round won") : qsTr("Knocked out")
        if (ladder && won)
            return qsTr("Level %1 cleared").arg(ladderStage + 1)
        if (mode !== GameScene.TwoPlayers)
            return winner.computer ? qsTr("CPU wins") : qsTr("You win")
        return qsTr("%1 wins").arg(winner.name)
    }

    function activate(index) {
        SoundEffects.play(SoundEffects.MenuSelect)
        items[index].activate()
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
                activate(items.length - 1)
                break
            case Qt.Key_Left:
            case Qt.Key_Up:
                currentItem = 0
                SoundEffects.play(SoundEffects.MenuMove)
                break
            case Qt.Key_Right:
            case Qt.Key_Down:
                currentItem = items.length - 1
                SoundEffects.play(SoundEffects.MenuMove)
                break
            case Qt.Key_Enter:
            case Qt.Key_Return:
            case Qt.Key_Space:
                if (!event.isAutoRepeat)
                    activate(currentItem)
                break
        }
    }
    onPointerPressed: (id, x, y) => phong.clickableAt(x, y)?.clicked()

    Text3D {
        y: 7.0
        scale: Qt.vector3d(2, 2, 2)
        horizontalAlignment: Text.AlignHCenter
        color: Theme.title
        glow: 0.8
        text: root.headline
    }

    // Final score, laid out like the scoreboard
    Node {
        visible: !root.endless && !root.squash
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
        model: {
            if (root.endless)
                return [
                    [qsTr("Score"), root.endlessScore],
                    [qsTr("Best"), Stats.endlessBest],
                    [qsTr("Time survived"), root.formatTime(root.match?.playTime ?? 0)],
                    [qsTr("Longest rally"), root.match?.longestRally ?? 0]
                ]

            if (root.squash)
                return [
                    [qsTr("Longest rally"), root.match?.longestRally ?? 0],
                    [qsTr("Best"), Stats.squashBest],
                    [qsTr("Time"), root.formatTime(root.match?.playTime ?? 0)]
                ]

            const rows = [
                [qsTr("Longest rally"), root.match?.longestRally ?? 0],
                [qsTr("Best rally ever"), Stats.longestRally],
                [qsTr("Match time"), root.formatTime(root.match?.playTime ?? 0)]
            ]
            // Best of three or five, the sets decided it
            if ((root.match?.setsToWin ?? 1) > 1)
                rows.unshift([qsTr("Sets"), root.match.left.sets + " : " + root.match.right.sets])
            return rows
        }

        delegate: Node {
            id: stat

            required property var modelData
            required property int index

            y: -0.5 - index * 1.8

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

    Text3D {
        visible: root.achievements.length > 0
        y: -7.2
        scale: Qt.vector3d(0.6, 0.6, 0.6)
        horizontalAlignment: Text.AlignHCenter
        color: Theme.title
        glow: 0.5
        text: qsTr("Achievement: %1").arg(root.achievements.join(", "))
    }

    Repeater3D {
        model: root.items

        delegate: Text3D {
            id: item

            required property var modelData
            required property int index

            x: root.items.length === 1 ? 0.0 : index === 0 ? -5.0 : 5.0
            y: -9.0
            horizontalAlignment: Text.AlignHCenter
            text: modelData.text
            clickable: true
            onClicked: root.activate(item.index)

            Disc {
                visible: item.index === root.currentItem
                position: Qt.vector3d(-0.5 * item.textWidth - 1.0, 0.35, 0)
                radius: 0.35
                sphere: true
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
