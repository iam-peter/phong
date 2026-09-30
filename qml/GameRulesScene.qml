pragma ComponentBehavior: Bound

import QtQuick
import Phong

OptionsScene {
    id: root

    enum RuleMode {
        Party = 100
    }

    property int mode: GameScene.OnePlayer
    property string modeName: ""
    property var launch: null
    property var profile: ({})
    readonly property int playIndex: rows.findIndex((row) => row.play === true)

    title: qsTr("%1 Rules").arg(modeName)

    function reload() {
        profile = GameSettings.rules(mode)
    }

    function setRule(name, value) {
        GameSettings.setRule(mode, name, value)
        reload()
    }

    function rule(name) {
        return profile[name]
    }

    readonly property bool scored: mode === GameScene.OnePlayer || mode === GameScene.TwoPlayers
                                   || mode === GameScene.Ladder || mode === GameScene.Tournament
                                   || mode === GameScene.Bricks
    readonly property bool hasSets: mode === GameScene.OnePlayer || mode === GameScene.TwoPlayers
                                    || mode === GameScene.Tournament || mode === GameScene.Bricks
    readonly property bool hasCpu: mode === GameScene.OnePlayer || mode === GameScene.Bricks
                                   || mode === GameRulesScene.Party
    readonly property bool hasArena: mode !== GameScene.Bricks && mode !== GameScene.Squash
                                     && mode !== GameRulesScene.Party
    readonly property bool hasPaddleSize: mode !== GameRulesScene.Party
    readonly property bool hasModifiers: mode !== GameScene.Squash

    rows: {
        const rows = []
        if (scored) {
            rows.push({
                label: qsTr("Points to win"), values: [3, 5, 7, 11, 15, 21],
                names: ["3", "5", "7", "11", "15", "21"],
                get: () => root.rule("pointsToWin"),
                set: (value) => root.setRule("pointsToWin", value)
            })
        }
        if (hasSets) {
            rows.push({
                label: qsTr("Sets"), values: [1, 2, 3],
                names: [qsTr("Single"), qsTr("Best of 3"), qsTr("Best of 5")],
                get: () => root.rule("setsToWin"),
                set: (value) => root.setRule("setsToWin", value)
            })
        }
        if (scored) {
            rows.push({
                label: qsTr("Win by two"), values: [false, true], names: [qsTr("Off"), qsTr("On")],
                get: () => root.rule("winByTwo"),
                set: (value) => root.setRule("winByTwo", value)
            })
        }
        rows.push({
            label: qsTr("Kickoff"), values: [1, 2, 3], names: [qsTr("1 s"), qsTr("2 s"), qsTr("3 s")],
            get: () => root.rule("kickoffTime"),
            set: (value) => root.setRule("kickoffTime", value)
        })
        rows.push({
            label: qsTr("Ball speed"), values: [GameSettings.Slow, GameSettings.Medium, GameSettings.Fast],
            names: [qsTr("Slow"), qsTr("Medium"), qsTr("Fast")],
            get: () => root.rule("ballSpeed"),
            set: (value) => root.setRule("ballSpeed", value)
        })
        if (hasPaddleSize) {
            rows.push({
                label: qsTr("Paddle size"),
                values: [GameSettings.Small, GameSettings.Regular, GameSettings.Large],
                names: [qsTr("Small"), qsTr("Regular"), qsTr("Large")],
                get: () => root.rule("paddleSize"),
                set: (value) => root.setRule("paddleSize", value)
            })
        }
        if (hasCpu) {
            rows.push({
                label: qsTr("CPU level"),
                values: [ComputerPlayer.Easy, ComputerPlayer.Normal, ComputerPlayer.Hard],
                names: [qsTr("Easy"), qsTr("Normal"), qsTr("Hard")],
                get: () => root.rule("difficulty"),
                set: (value) => root.setRule("difficulty", value)
            })
        }
        if (hasArena) {
            rows.push({
                label: qsTr("Arena"),
                values: ["random"].concat(Arenas.arenas.map((arena) => arena.id)),
                names: [qsTr("Random")].concat(Arenas.arenas.map((arena) => arena.name)),
                get: () => root.rule("arena"),
                set: (value) => root.setRule("arena", value)
            })
        }
        if (hasModifiers) {
            rows.push({
                label: qsTr("Modifiers"), values: [true, false], names: [qsTr("On"), qsTr("Off")],
                get: () => root.rule("modifiers"),
                set: (value) => root.setRule("modifiers", value)
            })
        }
        rows.push({
            label: qsTr("Restore rule defaults"), values: [], names: [],
            set: () => { GameSettings.restoreRules(root.mode); root.reload() }
        })
        if (launch) {
            rows.push({
                label: qsTr("Play"), values: [], names: [], play: true,
                set: () => root.launch()
            })
        }
        return rows
    }

    onActiveChanged: {
        if (active)
            reload()
    }
}