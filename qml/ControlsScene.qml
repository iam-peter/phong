pragma ComponentBehavior: Bound

import QtQuick
import QtQuick3D
import Phong

// The keys of the players. A row takes the next key pressed, a key in use
// by another action swaps with it.
OptionsScene {
    id: root

    title: qsTr("Controls")

    readonly property var actions: [
        [KeySettings.LeftUp, qsTr("Left up")],
        [KeySettings.LeftDown, qsTr("Left down")],
        [KeySettings.LeftSmash, qsTr("Left smash")],
        [KeySettings.LeftSpecial, qsTr("Left special")],
        [KeySettings.RightUp, qsTr("Right up")],
        [KeySettings.RightDown, qsTr("Right down")],
        [KeySettings.RightSmash, qsTr("Right smash")],
        [KeySettings.RightSpecial, qsTr("Right special")],
        [KeySettings.Pause, qsTr("Pause")]
    ]

    rows: actions.map((action) => ({
        label: action[1],
        values: [],
        capture: true,
        get: () => KeySettings.keyNames[action[0]],
        set: (key) => KeySettings.setKey(action[0], key)
    })).concat([{
        label: qsTr("Restore defaults"),
        values: [],
        set: () => KeySettings.restoreDefaults()
    }])

    Text3D {
        y: -10.2
        scale: Qt.vector3d(0.5, 0.5, 0.5)
        horizontalAlignment: Text.AlignHCenter
        color: Theme.dimmed
        text: qsTr("Against the computer both sets move your paddle, [Space] also smashes")
    }
}
