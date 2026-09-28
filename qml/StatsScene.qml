pragma ComponentBehavior: Bound

import QtQuick
import QtQuick3D
import Phong

Scene {
    id: root

    // Resetting needs a second confirmation
    property bool confirming: false

    readonly property var rows: [
        [qsTr("Ladder best"), [qsTr("-"), qsTr("Easy"), qsTr("Normal"), qsTr("Champion")][Stats.ladderBest]],
        [qsTr("Vs easy"), record(0)],
        [qsTr("Vs normal"), record(1)],
        [qsTr("Vs hard"), record(2)],
        [qsTr("2 player matches"), Stats.twoPlayerMatches],
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

    onActiveChanged: confirming = false

    onKeyPressed: (event) => {
        event.accepted = true
        switch (event.key) {
            case Qt.Key_Escape:
                phong.previousScene()
                break
            case Qt.Key_Enter:
            case Qt.Key_Return:
            case Qt.Key_Space:
                if (!event.isAutoRepeat)
                    reset()
                break
        }
    }
    onPointerPressed: (id, x, y) => phong.clickableAt(x, y)?.clicked()

    Text3D {
        y: 7.5
        scale: Qt.vector3d(2, 2, 2)
        horizontalAlignment: Text.AlignHCenter
        text: qsTr("Stats")
    }

    Repeater3D {
        model: root.rows

        delegate: Node {
            id: row

            required property var modelData
            required property int index

            y: 3.5 - index * 1.8

            Text3D {
                x: -11.0
                color: Theme.dimmed
                text: row.modelData[0]
            }

            Text3D {
                x: 11.0
                horizontalAlignment: Text.AlignRight
                text: row.modelData[1]
            }
        }
    }

    Text3D {
        id: resetItem
        y: -8.5
        horizontalAlignment: Text.AlignHCenter
        color: root.confirming ? Theme.accent : Theme.text
        text: root.confirming ? qsTr("Really reset?") : qsTr("Reset stats")
        clickable: true
        onClicked: root.reset()

        Disc {
            position: Qt.vector3d(-0.5 * resetItem.textWidth - 1.0, 0.35, 0)
            radius: 0.4
        }
    }

    Text3D {
        y: -11
        scale: Qt.vector3d(0.5, 0.5, 0.5)
        horizontalAlignment: Text.AlignHCenter
        text: qsTr("[Enter] reset   [Esc] back")
        color: Theme.dimmed
    }
}
