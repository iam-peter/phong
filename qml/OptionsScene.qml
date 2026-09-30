pragma ComponentBehavior: Bound

import QtQuick
import QtQuick3D
import Phong

// A screen of options. Each row cycles through its values, rows without
// values are actions, e.g. to restore the defaults.
Scene {
    id: root

    property string title
    // { label, values, names, get, set } or { label, values: [], set, hint },
    // or { label, values: [], capture: true, get, set } for a key: set gets
    // the next key pressed, with padCapture: true the next gamepad button,
    // or { label, values: [], text: true, maxLength, get, set } for a text
    // typed and confirmed with Enter
    property var rows: []
    // The rows and a way back at the end, for fingers and the mouse
    readonly property var entries: rows.concat([{ label: qsTr("Back"), values: [], back: true,
                                                  set: () => phong.previousScene() }])
    property int currentItem: 0
    // The row waiting for its key, -1 for none
    property int capturing: -1
    readonly property bool padCapturing: capturing >= 0 && (entries[capturing]?.padCapture ?? false)
    readonly property bool textCapturing: capturing >= 0 && (entries[capturing]?.text ?? false)
    // The text typed so far
    property string typed: ""
    textEntry: textCapturing
    enteredText: typed
    // The button that was just taken doesn't navigate the menu as well
    property bool swallowNavigation: false
    menuNavigation: !padCapturing && !swallowNavigation

    Connections {
        target: Gamepads
        enabled: root.active && root.padCapturing
        function onButtonPressed(pad, button) {
            root.swallowNavigation = true
            Qt.callLater(() => root.swallowNavigation = false)
            if (!root.entries[root.capturing].set(button)) {
                SoundEffects.play(SoundEffects.Curse)
                return
            }
            SoundEffects.play(SoundEffects.MenuSelect)
            root.capturing = -1
        }
    }

    // The row is the current one. A tap passes the copy of it its delegate
    // has, the index is the one to go by.
    function change(row, step) {
        const values = row.values
        if (row.text) {
            SoundEffects.play(SoundEffects.MenuSelect)
            typed = row.get() ?? ""
            capturing = currentItem
            return
        }
        if (row.capture) {
            SoundEffects.play(SoundEffects.MenuSelect)
            capturing = currentItem
            return
        }
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

    onActiveChanged: capturing = -1

    onKeyPressed: (event) => {
        event.accepted = true
        if (textCapturing) {
            if (event.gamepad && event.key !== Qt.Key_Escape && event.key !== Qt.Key_Return)
                return
            const row = entries[capturing]
            if (event.key === Qt.Key_Escape) {
                capturing = -1
            }
            else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                row.set(typed)
                SoundEffects.play(SoundEffects.MenuSelect)
                capturing = -1
            }
            else if (event.key === Qt.Key_Backspace) {
                typed = typed.slice(0, -1)
            }
            else if (event.text.length === 1 && event.text >= " " && typed.length < (row.maxLength ?? 40)) {
                typed += event.text
            }
            return
        }
        if (capturing >= 0) {
            // A key has to come from the keyboard, a button from a pad,
            // Escape cancels either
            if (event.isAutoRepeat || (event.gamepad && event.key !== Qt.Key_Escape))
                return
            if (padCapturing) {
                if (event.key === Qt.Key_Escape) {
                    SoundEffects.play(SoundEffects.MenuSelect)
                    capturing = -1
                }
                return
            }
            // Escape keeps the key, reserved keys are refused
            if (event.key !== Qt.Key_Escape && !entries[capturing].set(event.key)) {
                SoundEffects.play(SoundEffects.Curse)
                return
            }
            SoundEffects.play(SoundEffects.MenuSelect)
            capturing = -1
            return
        }

        switch (event.key) {
            case Qt.Key_Escape:
                phong.previousScene()
                break
            case Qt.Key_Up:
                currentItem = Math.max(currentItem - 1, 0)
                SoundEffects.play(SoundEffects.MenuMove)
                break
            case Qt.Key_Down:
                currentItem = Math.min(currentItem + 1, entries.length - 1)
                SoundEffects.play(SoundEffects.MenuMove)
                break
            case Qt.Key_Left:
                if (entries[currentItem].values.length)
                    change(entries[currentItem], -1)
                break
            case Qt.Key_Right:
                if (entries[currentItem].values.length)
                    change(entries[currentItem], 1)
                break
            case Qt.Key_Enter:
            case Qt.Key_Return:
            case Qt.Key_Space:
                change(entries[currentItem], 1)
                break
        }
    }
    onPointerPressed: (id, x, y) => phong.clickableAt(x, y)?.clicked()

    MenuTitle {
        text: root.title
    }

    MenuList {
        id: menuList
        count: root.entries.length

        Repeater3D {
            model: root.entries

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

                // Long texts shrink into the room right of the label
                readonly property real valueRoom: 10.0 - (labelText.x + labelText.textWidth) - 1.0
                function fit(width: real): vector3d {
                    const s = Math.min(1.0, valueRoom / Math.max(width, 0.001))
                    return Qt.vector3d(s, s, s)
                }

                y: menuList.yFor(index)
                scale: menuList.itemScale

            MenuItem {
                id: labelText
                x: -11.0
                centered: false
                selected: row.selected
                unselectedColor: Qt.tint(Theme.text, Qt.rgba(Theme.background.r, Theme.background.g, Theme.background.b, 0.25))
                text: row.modelData.label
                onClicked: {
                    root.currentItem = row.index
                    root.change(row.modelData, 1)
                }
            }

            MenuValue {
                id: valueText
                x: 10.0
                visible: row.modelData.values.length > 0
                selected: row.selected
                cycles: true
                text: row.valueName
                onClicked: {
                    root.currentItem = row.index
                    root.change(row.modelData, 1)
                }
                onDecreased: root.change(row.modelData, -1)
                onIncreased: root.change(row.modelData, 1)
            }

            // A key, or the request for one
            Text3D {
                x: 10.0
                visible: row.modelData.capture ?? false
                horizontalAlignment: Text.AlignRight
                color: root.capturing === row.index ? Theme.title : row.selected ? Theme.text : Theme.dimmed
                text: root.capturing === row.index ? (row.modelData.padCapture ? qsTr("Press a button") : qsTr("Press a key"))
                                                   : row.modelData.get?.() ?? ""
                clickable: visible
                onClicked: {
                    root.currentItem = row.index
                    root.change(row.modelData, 1)
                }
            }

            MenuTextInput {
                id: typedText
                x: 10.0
                scale: row.fit(typedText.textWidth)
                visible: row.modelData.text ?? false
                selected: row.selected
                editing: root.capturing === row.index
                text: root.capturing === row.index ? root.typed
                      : (row.modelData.get?.() ?? "") || (row.modelData.placeholder ?? "")
                onClicked: {
                    root.currentItem = row.index
                    root.change(row.modelData, 1)
                }
            }

                // Actions leading to another screen say so
                Text3D {
                    id: hintText
                    x: 10.0
                    scale: row.fit(hintText.textWidth)
                    visible: (row.modelData.hint ?? "") !== ""
                    horizontalAlignment: Text.AlignRight
                    color: row.selected ? Theme.title : Theme.dimmed
                    text: row.modelData.hint ?? ""
                }
            }
        }
    }

    MenuHint {
        text: qsTr("[Up/Down] select   [Left/Right] change   [Esc] back")
    }
}
