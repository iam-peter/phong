pragma Singleton

import QtQuick

// Shared look of all scenes
QtObject {
    // The wasm platform ships DejaVu Sans Mono but knows no "monospace" alias
    readonly property string fontFamily: Qt.platform.os === "wasm" ? "DejaVu Sans Mono" : "monospace"

    readonly property color text: Qt.rgba(1.0, 1.0, 1.0, 1.0)
    readonly property color dimmed: Qt.rgba(0.55, 0.55, 0.55, 1.0)
    readonly property color title: Qt.rgba(1.0, 0.0, 1.0, 1.0)
    readonly property color accent: Qt.rgba(1.0, 0.0, 0.0, 1.0)
    readonly property color ball: Qt.rgba(1.0, 1.0, 0.0, 1.0)
    readonly property color paddle: Qt.rgba(0.0, 1.0, 0.0, 1.0)
    readonly property color wall: Qt.rgba(1.0, 1.0, 1.0, 1.0)
    readonly property color goal: Qt.rgba(100 / 255, 100 / 255, 100 / 255, 1.0)
    readonly property color shield: Qt.rgba(0.2, 0.6, 1.0, 1.0)

    // In the order of Modifiers.Kind: fast ball, big paddle, shield,
    // small paddle, spin paddle, narrow field
    readonly property list<color> modifierColors: [
        Qt.rgba(1.0, 0.55, 0.0, 1.0),
        Qt.rgba(0.4, 1.0, 0.4, 1.0),
        shield,
        Qt.rgba(0.65, 0.3, 1.0, 1.0),
        Qt.rgba(1.0, 0.25, 0.5, 1.0),
        Qt.rgba(0.0, 0.9, 0.9, 1.0)
    ]
    readonly property list<string> modifierGlyphs: [">>", "+", "||", "-", "@", "="]
    readonly property list<string> modifierNames: [
        qsTr("Fast ball"), qsTr("Big paddle"), qsTr("Shield"),
        qsTr("Small paddle"), qsTr("Spin curse"), qsTr("Narrow field")
    ]

    // Closest camera distance in front of the scene it looks at, Main
    // backs off further when the window is too narrow for the scenes
    readonly property real cameraDistance: 34.0
    readonly property int cameraDuration: 250
}
