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
    readonly property color extraBall: Qt.rgba(1.0, 0.8, 0.3, 1.0)
    readonly property color ballMark: Qt.rgba(0.85, 0.5, 0.0, 1.0)
    readonly property color bumper: Qt.rgba(1.0, 0.35, 0.2, 1.0)
    readonly property color block: Qt.rgba(0.7, 0.7, 0.75, 1.0)


    // Closest camera distance in front of the scene it looks at, Main
    // backs off further when the window is too narrow for the scenes
    readonly property real cameraDistance: 34.0
    readonly property int cameraDuration: 250
}
