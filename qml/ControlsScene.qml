pragma ComponentBehavior: Bound

import QtQuick
import QtQuick3D
import Phong

// The keys of the players and the buttons of the pads, a page each for the
// classic field, the polygon and the gamepads. A row takes the next key
// pressed, a key in use by another action of the page swaps with it.
OptionsScene {
    id: root

    title: qsTr("Controls")

    property int page: 0

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

    readonly property var partyActions: [
        [KeySettings.PartyOneLeft, qsTr("Keys 1 left")],
        [KeySettings.PartyOneRight, qsTr("Keys 1 right")],
        [KeySettings.PartyOneUp, qsTr("Keys 1 up")],
        [KeySettings.PartyOneDown, qsTr("Keys 1 down")],
        [KeySettings.PartyOneSmash, qsTr("Keys 1 smash")],
        [KeySettings.PartyOneSpecial, qsTr("Keys 1 special")],
        [KeySettings.PartyTwoLeft, qsTr("Keys 2 left")],
        [KeySettings.PartyTwoRight, qsTr("Keys 2 right")],
        [KeySettings.PartyTwoUp, qsTr("Keys 2 up")],
        [KeySettings.PartyTwoDown, qsTr("Keys 2 down")],
        [KeySettings.PartyTwoSmash, qsTr("Keys 2 smash")],
        [KeySettings.PartyTwoSpecial, qsTr("Keys 2 special")]
    ]

    readonly property var padActions: [
        [KeySettings.PadSmash, qsTr("Pad smash")],
        [KeySettings.PadSpecial, qsTr("Pad special")],
        [KeySettings.PadDash, qsTr("Pad dash")],
        [KeySettings.PadPause, qsTr("Pad pause")]
    ]

    readonly property var pageRow: ({
        label: qsTr("Page"),
        values: [0, 1, 2],
        names: [qsTr("2 players"), qsTr("3-6 players"), qsTr("Gamepads")],
        get: () => root.page,
        set: (page) => root.page = page
    })

    readonly property var pageRows: {
        switch (page) {
            case 1:
                return partyActions.map((action) => ({
                    label: action[1],
                    values: [],
                    capture: true,
                    get: () => KeySettings.partyKeyNames[action[0]],
                    set: (key) => KeySettings.setPartyKey(action[0], key)
                }))
            case 2:
                return padActions.map((action) => ({
                    label: action[1],
                    values: [],
                    capture: true,
                    padCapture: true,
                    get: () => KeySettings.padButtonNames[action[0]],
                    set: (button) => KeySettings.setPadButton(action[0], button)
                }))
            default:
                return actions.map((action) => ({
                    label: action[1],
                    values: [],
                    capture: true,
                    get: () => KeySettings.keyNames[action[0]],
                    set: (key) => KeySettings.setKey(action[0], key)
                }))
        }
    }

    rows: [pageRow].concat(pageRows).concat([{
        label: qsTr("Restore defaults"),
        values: [],
        set: () => KeySettings.restoreDefaults()
    }])

    onActiveChanged: {
        if (active)
            page = 0
    }

    Text3D {
        y: -10.2
        scale: Qt.vector3d(0.5, 0.5, 0.5)
        horizontalAlignment: Text.AlignHCenter
        color: Theme.dimmed
        text: [qsTr("Against the computer both sets move your paddle, [Space] also smashes"),
               qsTr("The directions push the paddle along its side, alone both sets are yours"),
               qsTr("The d-pad and Back stay fixed, the shoulders dash unless mapped")][root.page]
    }
}
