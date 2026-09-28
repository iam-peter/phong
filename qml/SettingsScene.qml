pragma ComponentBehavior: Bound

import QtQuick
import QtQuick3D
import Phong

OptionsScene {
    id: root

    property Scene graphicsScene
    property Scene controlsScene

    title: qsTr("Settings")

    rows: [
        {
            label: qsTr("Points to win"),
            values: [3, 5, 7, 11, 15, 21],
            names: ["3", "5", "7", "11", "15", "21"],
            get: () => GameSettings.pointsToWin,
            set: (value) => GameSettings.pointsToWin = value
        },
        {
            label: qsTr("Sets"),
            values: [1, 2, 3],
            names: [qsTr("Single"), qsTr("Best of 3"), qsTr("Best of 5")],
            get: () => GameSettings.setsToWin,
            set: (value) => GameSettings.setsToWin = value
        },
        {
            label: qsTr("Win by two"),
            values: [false, true],
            names: [qsTr("Off"), qsTr("On")],
            get: () => GameSettings.winByTwo,
            set: (value) => GameSettings.winByTwo = value
        },
        {
            label: qsTr("Kickoff"),
            values: [1, 2, 3],
            names: [qsTr("1 s"), qsTr("2 s"), qsTr("3 s")],
            get: () => GameSettings.kickoffTime,
            set: (value) => GameSettings.kickoffTime = value
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
            label: qsTr("Arena"),
            values: ["random"].concat(Arenas.arenas.map((arena) => arena.id)),
            names: [qsTr("Random")].concat(Arenas.arenas.map((arena) => arena.name)),
            get: () => GameSettings.arena,
            set: (value) => GameSettings.arena = value
        },
        {
            label: qsTr("Modifiers"),
            values: [true, false],
            names: [qsTr("On"), qsTr("Off")],
            get: () => GameSettings.modifiers,
            set: (value) => GameSettings.modifiers = value
        },
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
            label: qsTr("Restore defaults"),
            values: [],
            names: [],
            get: () => undefined,
            set: (value) => GameSettings.restoreDefaults()
        }
    ]
}
