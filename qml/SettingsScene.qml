pragma ComponentBehavior: Bound

import QtQuick
import QtQuick3D
import Phong

OptionsScene {
    id: root

    property Scene graphicsScene
    property Scene controlsScene
    property Scene onlineScene

    title: qsTr("Settings")

    rows: [
        {
            label: qsTr("Sound"),
            values: [true, false],
            names: [qsTr("On"), qsTr("Off")],
            get: () => GameSettings.sound,
            set: (value) => GameSettings.sound = value
        },
        {
            label: qsTr("Music"),
            values: [true, false],
            names: [qsTr("On"), qsTr("Off")],
            get: () => GameSettings.music,
            set: (value) => GameSettings.music = value
        },
        {
            label: qsTr("Music volume"),
            values: [25, 50, 75, 100],
            names: ["25%", "50%", "75%", "100%"],
            get: () => GameSettings.musicVolume,
            set: (value) => GameSettings.musicVolume = value
        },
        {
            label: qsTr("Rumble"),
            values: [true, false],
            names: [qsTr("On"), qsTr("Off")],
            get: () => GameSettings.rumble,
            set: (value) => GameSettings.rumble = value
        },
        {
            label: qsTr("Graphics"),
            values: [],
            names: [],
            hint: ">",
            set: () => phong.nextScene(root.graphicsScene)
        },
        {
            label: qsTr("Controls"),
            values: [],
            names: [],
            hint: ">",
            set: () => phong.nextScene(root.controlsScene)
        },
        {
            label: qsTr("Online"),
            values: [],
            names: [],
            hint: ">",
            set: () => phong.nextScene(root.onlineScene)
        },
        {
            label: qsTr("Restore app defaults"),
            values: [],
            names: [],
            get: () => undefined,
            set: () => GameSettings.restoreAppDefaults()
        }
    ]
}
