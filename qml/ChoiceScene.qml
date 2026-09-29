pragma ComponentBehavior: Bound

import QtQuick
import QtQuick3D
import Phong

// A list of things to play, each with a line saying what it is: the solo
// modes, the local ones, and hosting or joining online. Headers group the
// entries, entries that can't be played now stay dimmed and are skipped.
//
// An entry: { text, detail, activate } and for a choice beside it
// { value, change(step) }, { enabled: false } to leave it out of reach.
// A header: { header: "..." }.
Scene {
    id: root

    property string title
    property var entries: []
    // The entries and a way back at the end, for fingers and the mouse
    readonly property var choices: entries.concat([{ text: qsTr("Back"),
                                                     activate: () => phong.previousScene() }])
    // Under the title, e.g. a server waking up
    property string status: ""
    property color statusColor: Theme.dimmed
    property int currentItem: firstSelectable()

    // The entries with their height, a header and a detail take room
    readonly property var rows: {
        const rows = []
        let y = status !== "" ? 4.6 : 5.4
        for (const entry of choices) {
            if (entry.header !== undefined) {
                y -= 0.5
                rows.push({ entry: entry, y: y })
                y -= 1.6
                continue
            }
            rows.push({ entry: entry, y: y })
            y -= entry.detail ? 2.7 : 1.8
        }
        return rows
    }

    function selectable(index) {
        const entry = choices[index]
        return entry !== undefined && entry.header === undefined && entry.enabled !== false
    }

    function firstSelectable() {
        for (let i = 0; i < choices.length; ++i) {
            if (selectable(i))
                return i
        }
        return 0
    }

    function move(step) {
        for (let i = currentItem + step; i >= 0 && i < choices.length; i += step) {
            if (selectable(i)) {
                currentItem = i
                SoundEffects.play(SoundEffects.MenuMove)
                return
            }
        }
    }

    function change(index, step) {
        const entry = choices[index]
        if (!entry?.change || !selectable(index))
            return
        entry.change(step)
        SoundEffects.play(SoundEffects.MenuMove)
    }

    function activate(index) {
        if (!selectable(index))
            return
        SoundEffects.play(SoundEffects.MenuSelect)
        currentItem = index
        choices[index].activate()
    }

    // Something that went away takes the selection to the next one
    onEntriesChanged: {
        if (!selectable(currentItem))
            currentItem = firstSelectable()
    }

    onKeyPressed: (event) => {
        event.accepted = true
        switch (event.key) {
            case Qt.Key_Escape:
                SoundEffects.play(SoundEffects.MenuSelect)
                phong.previousScene()
                break
            case Qt.Key_Up:
                move(-1)
                break
            case Qt.Key_Down:
                move(1)
                break
            case Qt.Key_Left:
            case Qt.Key_Right:
                change(currentItem, event.key === Qt.Key_Left ? -1 : 1)
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
        text: root.title
    }

    Text3D {
        visible: root.status !== ""
        y: 6.5
        scale: Qt.vector3d(0.55, 0.55, 0.55)
        horizontalAlignment: Text.AlignHCenter
        color: root.statusColor
        text: root.status
    }

    Repeater3D {
        model: root.rows

        delegate: Node {
            id: row

            required property var modelData
            required property int index

            readonly property var entry: modelData.entry
            readonly property bool header: entry.header !== undefined
            readonly property bool selected: index === root.currentItem
            readonly property bool enabled: entry.enabled !== false
            readonly property bool arrows: selected && entry.change !== undefined

            y: modelData.y

            Text3D {
                visible: row.header
                scale: Qt.vector3d(0.6, 0.6, 0.6)
                horizontalAlignment: Text.AlignHCenter
                color: Theme.title
                glow: 0.4
                text: row.entry.header ?? ""
            }

            Text3D {
                id: label
                visible: !row.header
                horizontalAlignment: Text.AlignHCenter
                color: !row.enabled ? Theme.dimmed : row.selected ? Theme.text : Theme.unselected
                text: (row.entry.text ?? "") + (row.entry.value !== undefined ? "  " + row.entry.value : "")
                clickable: visible && row.enabled
                onClicked: root.activate(row.index)
            }

            Text3D {
                visible: row.arrows
                x: -0.5 * label.textWidth - 0.9
                horizontalAlignment: Text.AlignRight
                color: Theme.title
                text: "<"
                clickable: visible
                hitLeft: 3.0
                hitRight: 0.5
                hitVertical: 0.35
                onClicked: root.change(row.index, -1)
            }

            Text3D {
                visible: row.arrows
                x: 0.5 * label.textWidth + 0.9
                color: Theme.title
                text: ">"
                clickable: visible
                hitLeft: 0.5
                hitRight: 3.0
                hitVertical: 0.35
                onClicked: root.change(row.index, 1)
            }

            Disc {
                visible: row.selected && !row.header
                sphere: true
                position: Qt.vector3d(-0.5 * label.textWidth - (row.arrows ? 2.8 : 1.0), 0.35, 0)
                radius: 0.35
            }

            Text3D {
                visible: !row.header && (row.entry.detail ?? "") !== ""
                y: -0.95
                scale: Qt.vector3d(0.5, 0.5, 0.5)
                horizontalAlignment: Text.AlignHCenter
                color: Theme.dimmed
                text: row.entry.detail ?? ""
            }
        }
    }

    Text3D {
        y: -11.4
        scale: Qt.vector3d(0.5, 0.5, 0.5)
        horizontalAlignment: Text.AlignHCenter
        color: Theme.dimmed
        text: root.choices.some((entry) => entry.change !== undefined)
              ? qsTr("[Up/Down] select   [Left/Right] change   [Enter] play   [Esc] back")
              : qsTr("[Up/Down] select   [Enter] play   [Esc] back")
    }
}
