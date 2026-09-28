pragma Singleton

import QtQuick
import Phong

// Shared look of all scenes
QtObject {
    // The wasm platform ships DejaVu Sans Mono but knows no "monospace" alias
    readonly property string fontFamily: Qt.platform.os === "wasm" ? "DejaVu Sans Mono" : "monospace"

    // Neon on a dark blue night
    readonly property color background: Qt.rgba(0.02, 0.035, 0.1, 1.0)
    readonly property color floor: Qt.rgba(0.035, 0.06, 0.16, 1.0)
    readonly property color grid: Qt.rgba(0.12, 0.2, 0.45, 1.0)
    readonly property color text: Qt.rgba(0.91, 0.94, 1.0, 1.0)
    readonly property color dimmed: Qt.rgba(0.42, 0.48, 0.66, 1.0)
    readonly property color title: Qt.rgba(1.0, 0.25, 0.82, 1.0)
    readonly property color accent: Qt.rgba(1.0, 0.31, 0.37, 1.0)
    readonly property color ball: Qt.rgba(1.0, 0.9, 0.36, 1.0)
    readonly property color ballMark: Qt.rgba(1.0, 0.55, 0.1, 1.0)
    readonly property color extraBall: Qt.rgba(1.0, 0.7, 0.28, 1.0)
    readonly property color leftPlayer: Qt.rgba(0.16, 0.9, 1.0, 1.0)
    readonly property color rightPlayer: Qt.rgba(1.0, 0.31, 0.64, 1.0)
    readonly property color paddle: leftPlayer
    readonly property color wall: Qt.rgba(0.45, 0.62, 1.0, 1.0)
    readonly property color goal: Qt.rgba(0.04, 0.06, 0.17, 1.0)
    readonly property color shield: Qt.rgba(0.25, 0.63, 1.0, 1.0)
    readonly property color bumper: Qt.rgba(1.0, 0.42, 0.24, 1.0)
    readonly property color block: Qt.rgba(0.5, 0.38, 1.0, 1.0)
    readonly property color shadow: Qt.rgba(0.0, 0.0, 0.03, 1.0)

    // How much self-lit surfaces shine, a little even without the bloom
    readonly property real glowScale: GraphicsSettings.glow === GraphicsSettings.NoGlow ? 0.3 : 1.0
    readonly property bool phong: GraphicsSettings.shading === GraphicsSettings.Phong

    // Closest camera distance in front of the scene it looks at, Main
    // backs off further when the window is too narrow for the scenes
    readonly property real cameraDistance: 34.0
    readonly property int cameraDuration: 250
}
