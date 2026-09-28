pragma ComponentBehavior: Bound

import QtQuick
import QtQuick3D
import Phong

// A screen of options. Each row cycles through its values, rows without
// values are actions, e.g. to restore the defaults.
Scene {
    id: root

    property string title
    // { label, values, names, get, set } or { label, values: [], set, hint }
    property var rows: []
    property int currentItem: 0

    // The rows fit between the title and the hint
    readonly property real rowSpacing: Math.min(2.0, 15.0 / Math.max(rows.length, 1))

    function change(row, step) {
        const values = row.values
        if (values.length === 0) {
            SoundEffects.play(SoundEffects.MenuSelect)
            row.set()
            return
        }

        // Unknown values (e.g. edited config) start over at the first entry
        const index = values.indexOf(row.get())
        const next = index < 0 ? 0 : (index + step + values.length) % values.length
        row.set(values[next])
        SoundEffects.play(SoundEffects.MenuSelect)
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
                currentItem = Math.min(currentItem + 1, rows.length - 1)
                SoundEffects.play(SoundEffects.MenuMove)
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
        y: 8.6
        scale: Qt.vector3d(1.8, 1.8, 1.8)
        horizontalAlignment: Text.AlignHCenter
        color: Theme.title
        glow: 0.8
        text: root.title
    }

    Repeater3D {
        model: root.rows

        delegate: Node {
            id: row

            required property var modelData
            required property int index

            readonly property bool selected: index === root.currentItem
            // Reading the setting in get() makes the value text follow it
            readonly property var value: modelData.values.length ? modelData.get() : undefined
            readonly property string valueName: {
                const index = modelData.values.indexOf(value)
                return index < 0 ? String(value ?? "") : modelData.names[index]
            }

            y: 6.0 - index * root.rowSpacing

            Disc {
                visible: row.selected
                sphere: true
                position: Qt.vector3d(-12.0, 0.35, 0)
                radius: 0.35
            }

            Text3D {
                x: -11.0
                color: row.selected ? Theme.text : Qt.tint(Theme.text, Qt.rgba(Theme.background.r, Theme.background.g, Theme.background.b, 0.25))
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
                color: Theme.title
                text: "<"
                clickable: visible
                onClicked: root.change(row.modelData, -1)
            }

            Text3D {
                x: valueText.x + 0.7
                visible: valueText.visible && row.selected
                color: Theme.title
                text: ">"
                clickable: visible
                onClicked: root.change(row.modelData, 1)
            }

            // Actions leading to another screen say so
            Text3D {
                x: 10.0
                visible: (row.modelData.hint ?? "") !== ""
                horizontalAlignment: Text.AlignRight
                color: row.selected ? Theme.title : Theme.dimmed
                text: row.modelData.hint ?? ""
            }
        }
    }

    Text3D {
        y: -11.4
        scale: Qt.vector3d(0.5, 0.5, 0.5)
        horizontalAlignment: Text.AlignHCenter
        text: qsTr("[Up/Down] select   [Left/Right] change   [Esc] back")
        color: Theme.dimmed
    }
}
