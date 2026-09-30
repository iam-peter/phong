pragma ComponentBehavior: Bound

import QtQuick
import QtQuick3D
import Phong

// The tournament bracket between the matches, from the quarterfinals on
// the left to the champion on the right
Scene {
    id: root

    property Scene menuScene
    property int currentItem: 0

    readonly property var roundNames: [qsTr("Quarterfinal"), qsTr("Semifinal"), qsTr("Final")]
    readonly property var personalities: [
        qsTr("An all-rounder"),
        qsTr("Returns everything straight back, never smashes"),
        qsTr("Brushes every ball to curve it"),
        qsTr("Winds up almost every return"),
        qsTr("Sends the ball through the modifiers")
    ]

    // Columns of the rounds and where the names sit, a match between two
    // names leads to the middle of them in the next round
    readonly property var columns: [-17.5, -8.5, 0.5, 9.5]
    readonly property real nameWidth: 3.6
    function slotY(round, slot) {
        if (round === 0)
            return 6.2 - slot * 1.6
        return 0.5 * (slotY(round - 1, 2 * slot) + slotY(round - 1, 2 * slot + 1))
    }

    readonly property var items: {
        const menu = { text: qsTr("Menu"), activate: () => phong.returnToMenu() }
        if (Tournament.running)
            return [{ text: qsTr("Play"), activate: () => phong.playTournamentMatch() }, menu]
        return [menu]
    }

    readonly property string info: {
        if (Tournament.champion)
            return qsTr("You won the tournament")
        if (!Tournament.running)
            return qsTr("Knocked out")
        return qsTr("%1 against %2").arg(roundNames[Tournament.round]).arg(Tournament.opponent.name)
    }

    function activate(index) {
        SoundEffects.play(SoundEffects.MenuSelect)
        items[index].activate()
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

    MenuTitle {
        text: qsTr("Tournament")
    }

    // Names, the rounds from left to right
    Repeater3D {
        model: Tournament.rounds

        delegate: Node {
            id: column

            required property var modelData
            required property int index

            Repeater3D {
                model: column.modelData

                delegate: Node {
                    id: entrant

                    required property var modelData
                    required property int index

                    readonly property bool open: modelData.name === undefined
                    readonly property bool lost: modelData.result === Tournament.Lost
                    readonly property bool champion: column.index === Tournament.rounds.length - 1

                    position: Qt.vector3d(root.columns[column.index], root.slotY(column.index, index), 0)

                    Text3D {
                        visible: !entrant.open
                        y: -0.25
                        scale: Qt.vector3d(0.55, 0.55, 0.55)
                        color: entrant.champion ? Theme.title
                               : entrant.modelData.player ? Theme.leftPlayer
                               : entrant.lost ? Theme.dimmed : Theme.text
                        glow: entrant.champion ? 0.8 : 0.1
                        text: entrant.modelData.name ?? ""
                    }

                    // An open place is a short dash
                    Model {
                        visible: entrant.open
                        x: 0.5 * root.nameWidth
                        source: "#Cube"
                        scale: Qt.vector3d(root.nameWidth / 100, 0.08 / 100, 0.001)
                        materials: PhongMaterial {
                            color: Theme.dimmed
                        }
                    }
                }
            }
        }
    }

    // Lines from each pair of names to the winner's place
    Repeater3D {
        model: [0, 1, 2]

        delegate: Node {
            id: round

            required property int modelData
            readonly property real lineX: root.columns[modelData + 1] - 0.6

            Repeater3D {
                model: 4 >> round.modelData

                delegate: Node {
                    id: pair

                    required property int index
                    readonly property real top: root.slotY(round.modelData, 2 * index)
                    readonly property real bottom: root.slotY(round.modelData, 2 * index + 1)
                    readonly property real startX: root.columns[round.modelData] + root.nameWidth

                    component Line: Model {
                        property real fromX
                        property real fromY
                        property real toX
                        property real toY
                        position: Qt.vector3d(0.5 * (fromX + toX), 0.5 * (fromY + toY), -0.2)
                        source: "#Cube"
                        scale: Qt.vector3d(Math.max(0.08, toX - fromX) / 100, Math.max(0.08, Math.abs(toY - fromY)) / 100,
                                           0.001)
                        materials: PhongMaterial {
                            color: Theme.grid
                            glow: 0.4
                        }
                    }

                    Line { fromX: pair.startX; toX: round.lineX; fromY: pair.top; toY: pair.top }
                    Line { fromX: pair.startX; toX: round.lineX; fromY: pair.bottom; toY: pair.bottom }
                    Line { fromX: round.lineX; toX: round.lineX; fromY: pair.bottom; toY: pair.top }
                }
            }
        }
    }

    Text3D {
        y: -6.9
        scale: Qt.vector3d(0.8, 0.8, 0.8)
        horizontalAlignment: Text.AlignHCenter
        color: Tournament.running ? Theme.text : Theme.title
        text: root.info
    }

    Text3D {
        y: -8.0
        scale: Qt.vector3d(0.5, 0.5, 0.5)
        horizontalAlignment: Text.AlignHCenter
        visible: Tournament.running
        color: Theme.dimmed
        text: root.personalities[Tournament.opponent.personality ?? 0] ?? ""
    }

    Repeater3D {
        model: root.items

        delegate: MenuItem {
            id: item

            required property var modelData
            required property int index

            x: root.items.length === 1 ? 0.0 : index === 0 ? -5.0 : 5.0
            y: -9.8
            selected: item.index === root.currentItem
            text: modelData.text
            onClicked: root.activate(item.index)
        }
    }

    MenuHint {
        text: qsTr("[Left/Right] select   [Enter] confirm   [Esc] menu")
    }
}
