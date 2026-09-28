pragma ComponentBehavior: Bound

import QtQuick
import QtQuick3D
import Phong

Scene {
    id: root

    property int currentItem: 0

    // Each row cycles through its values, restore has none
    readonly property var rows: [
        {
            label: qsTr("Points to win"),
            values: [3, 5, 7, 11, 15, 21],
            names: ["3", "5", "7", "11", "15", "21"],
            get: () => GameSettings.pointsToWin,
            set: (value) => GameSettings.pointsToWin = value
        },
        {
            label: qsTr("Ball speed"),
            values: [GameSettings.Slow, GameSettings.Medium, GameSettings.Fast],
            names: [qsTr("Slow"), qsTr("Medium"), qsTr("Fast")],
            get: () => GameSettings.ballSpeed,
            set: (value) => GameSettings.ballSpeed = value
        },
        {
            label: qsTr("Paddle size"),
            values: [GameSettings.Small, GameSettings.Regular, GameSettings.Large],
            names: [qsTr("Small"), qsTr("Regular"), qsTr("Large")],
            get: () => GameSettings.paddleSize,
            set: (value) => GameSettings.paddleSize = value
        },
        {
            label: qsTr("CPU level"),
            values: [ComputerPlayer.Easy, ComputerPlayer.Normal, ComputerPlayer.Hard],
            names: [qsTr("Easy"), qsTr("Normal"), qsTr("Hard")],
            get: () => GameSettings.difficulty,
            set: (value) => GameSettings.difficulty = value
        },
        {
            label: qsTr("Modifiers"),
            values: [true, false],
            names: [qsTr("On"), qsTr("Off")],
            get: () => GameSettings.modifiers,
            set: (value) => GameSettings.modifiers = value
        },
        {
            label: qsTr("Restore defaults"),
            values: [],
            names: [],
            get: () => undefined,
            set: (value) => GameSettings.restoreDefaults()
        }
    ]

    function change(row, step) {
        const values = row.values
        if (values.length === 0) {
            row.set()
            return
        }

        // Unknown values (e.g. edited config) start over at the first entry
        const index = values.indexOf(row.get())
        const next = index < 0 ? 0 : (index + step + values.length) % values.length
        row.set(values[next])
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
                currentItem = Math.min(currentItem + 1, rows.length - 1)
                break
            case Qt.Key_Left:
                if (rows[currentItem].values.length)
                    change(rows[currentItem], -1)
                break
            case Qt.Key_Right:
                if (rows[currentItem].values.length)
                    change(rows[currentItem], 1)
                break
            case Qt.Key_Enter:
            case Qt.Key_Return:
            case Qt.Key_Space:
                change(rows[currentItem], 1)
                break
        }
    }
    onPointerPressed: (id, x, y) => phong.clickableAt(x, y)?.clicked()

    Text3D {
        y: 7.5
        scale: Qt.vector3d(2, 2, 2)
        horizontalAlignment: Text.AlignHCenter
        text: qsTr("Settings")
    }

    Repeater3D {
        model: root.rows

        delegate: Node {
            id: row

            required property var modelData
            required property int index

            readonly property bool selected: index === root.currentItem
            // Depend on the settings so the value text updates
            readonly property var value: {
                GameSettings.pointsToWin; GameSettings.ballSpeed
                GameSettings.paddleSize; GameSettings.difficulty
                GameSettings.modifiers
                return modelData.get()
            }
            readonly property string valueName: {
                const index = modelData.values.indexOf(value)
                return index < 0 ? String(value ?? "") : modelData.names[index]
            }

            y: 3.0 - index * 2.0

            Disc {
                visible: row.selected
                position: Qt.vector3d(-12.0, 0.35, 0)
                radius: 0.4
            }

            Text3D {
                x: -11.0
                text: row.modelData.label
                clickable: true
                onClicked: {
                    root.currentItem = row.index
                    root.change(row.modelData, 1)
                }
            }

            // The value stays in place, the arrows wrap around it
            Text3D {
                id: valueText
                x: 10.0
                visible: row.modelData.values.length > 0
                horizontalAlignment: Text.AlignRight
                color: row.selected ? Theme.text : Theme.dimmed
                text: row.valueName
                clickable: visible
                onClicked: {
                    root.currentItem = row.index
                    root.change(row.modelData, 1)
                }
            }

            Text3D {
                x: valueText.x - valueText.textWidth - 0.7
                visible: valueText.visible && row.selected
                horizontalAlignment: Text.AlignRight
                text: "<"
                clickable: visible
                onClicked: root.change(row.modelData, -1)
            }

            Text3D {
                x: valueText.x + 0.7
                visible: valueText.visible && row.selected
                text: ">"
                clickable: visible
                onClicked: root.change(row.modelData, 1)
            }
        }
    }

    Text3D {
        y: -11
        scale: Qt.vector3d(0.5, 0.5, 0.5)
        horizontalAlignment: Text.AlignHCenter
        text: qsTr("[Up/Down] select   [Left/Right] change   [Esc] back")
        color: Theme.dimmed
    }
}
