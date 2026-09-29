pragma ComponentBehavior: Bound

import QtQuick
import QtQuick3D
import Phong

Scene {
    id: root

    property Scene achievementsScene
    // Resetting needs a second confirmation
    property bool confirming: false
    property int currentItem: 0

    readonly property var rows: [
        [qsTr("Ladder best"), [qsTr("-"), qsTr("Easy"), qsTr("Normal"), qsTr("Champion")][Stats.ladderBest]],
        [qsTr("Vs easy"), record(0)],
        [qsTr("Vs normal"), record(1)],
        [qsTr("Vs hard"), record(2)],
        [qsTr("Endless best"), Stats.endlessBest],
        [qsTr("Tournaments won"), Stats.tournamentsWon],
        [qsTr("Squash best"), Stats.squashBest],
        [qsTr("Achievements"), qsTr("%1 of %2").arg(Stats.unlockedCount).arg(Stats.achievements.length)],
        [qsTr("2 player matches"), Stats.twoPlayerMatches],
        [qsTr("3-6 player games"), qsTr("%1 played  %2 won").arg(Stats.partyGames).arg(Stats.partyWins)],
        [qsTr("Longest rally"), Stats.longestRally]
    ]

    function record(difficulty) {
        const record = Stats.records[difficulty]
        return qsTr("%1 won  %2 lost").arg(record.wins).arg(record.losses)
    }

    function reset() {
        SoundEffects.play(SoundEffects.MenuSelect)
        if (!confirming) {
            confirming = true
            return
        }

        Stats.reset()
        confirming = false
    }

    function activate(index) {
        if (index === 0) {
            SoundEffects.play(SoundEffects.MenuSelect)
            confirming = false
            phong.nextScene(achievementsScene)
        }
        else {
            reset()
        }
    }

    onActiveChanged: confirming = false

    onKeyPressed: (event) => {
        event.accepted = true
        switch (event.key) {
            case Qt.Key_Escape:
                phong.previousScene()
                break
            case Qt.Key_Left:
            case Qt.Key_Up:
                currentItem = 0
                confirming = false
                SoundEffects.play(SoundEffects.MenuMove)
                break
            case Qt.Key_Right:
            case Qt.Key_Down:
                currentItem = 1
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
        y: 8.6
        scale: Qt.vector3d(1.8, 1.8, 1.8)
        horizontalAlignment: Text.AlignHCenter
        color: Theme.title
        glow: 0.8
        text: qsTr("Stats")
    }

    Repeater3D {
        model: root.rows

        delegate: Node {
            id: row

            required property var modelData
            required property int index

            y: 6.0 - index * 1.25
            scale: Qt.vector3d(0.85, 0.85, 0.85)

            Text3D {
                x: -13.0
                color: Theme.dimmed
                text: row.modelData[0]
            }

            Text3D {
                x: 13.0
                horizontalAlignment: Text.AlignRight
                text: row.modelData[1]
            }
        }
    }

    Text3D {
        id: achievementsItem
        x: -6.0
        y: -9.0
        horizontalAlignment: Text.AlignHCenter
        color: root.currentItem === 0 ? Theme.text : Theme.unselected
        text: qsTr("Achievements")
        clickable: true
        onClicked: {
            root.currentItem = 0
            root.activate(0)
        }

        Disc {
            visible: root.currentItem === 0
            position: Qt.vector3d(-0.5 * achievementsItem.textWidth - 1.0, 0.35, 0)
            radius: 0.35
            sphere: true
        }
    }

    Text3D {
        id: resetItem
        x: 6.0
        y: -9.0
        horizontalAlignment: Text.AlignHCenter
        color: root.confirming ? Theme.accent : root.currentItem === 1 ? Theme.text : Theme.unselected
        text: root.confirming ? qsTr("Really reset?") : qsTr("Reset stats")
        clickable: true
        onClicked: {
            root.currentItem = 1
            root.activate(1)
        }

        Disc {
            visible: root.currentItem === 1
            position: Qt.vector3d(-0.5 * resetItem.textWidth - 1.0, 0.35, 0)
            radius: 0.35
            sphere: true
        }
    }

    Text3D {
        y: -11
        scale: Qt.vector3d(0.5, 0.5, 0.5)
        horizontalAlignment: Text.AlignHCenter
        text: qsTr("[Left/Right] select   [Enter] confirm   [Esc] back")
        color: Theme.dimmed
    }
}
