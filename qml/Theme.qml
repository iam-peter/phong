pragma Singleton

import QtQuick
import Phong

// Shared look of all scenes, the colors come from the theme chosen in the
// graphics settings
QtObject {
    id: theme

    // The wasm platform ships DejaVu Sans Mono but knows no "monospace" alias
    readonly property string fontFamily: Qt.platform.os === "wasm" ? "DejaVu Sans Mono" : "monospace"

    // In the order of GraphicsSettings.Theme. ink maps other colors, e.g.
    // of modifiers, onto the few a classic theme has, from dark to light.
    // Light themes tone down highlights, light and glow, they would wash
    // out the colors.
    readonly property var palettes: [
        {
            // Neon on a dark blue night
            background: "#05091a", floor: "#091029", grid: "#1f3373",
            text: "#e8f0ff", dimmed: "#6b7aa8", title: "#ff40d1", accent: "#ff4f5e",
            ball: "#ffe65c", ballMark: "#ff8c1a", extraBall: "#ffb347",
            leftPlayer: "#29e6ff", rightPlayer: "#ff4fa3",
            wall: "#739eff", goal: "#0a0f2b", shield: "#40a1ff",
            bumper: "#ff6b3d", block: "#8061ff", shadow: "#000008",
            starFrom: "#7389ff", starTo: "#ffe6ff", ink: null,
            shine: 1.0, glow: 1.0, light: 1.0
        },
        {
            // Black, grey and white like the arcade original
            background: "#000000", floor: "#080808", grid: "#262626",
            text: "#f2f2f2", dimmed: "#7a7a7a", title: "#ffffff", accent: "#ffffff",
            ball: "#ffffff", ballMark: "#6e6e6e", extraBall: "#bdbdbd",
            leftPlayer: "#ffffff", rightPlayer: "#ffffff",
            wall: "#d9d9d9", goal: "#121212", shield: "#bdbdbd",
            bumper: "#a6a6a6", block: "#6e6e6e", shadow: "#000000",
            starFrom: "#4d4d4d", starTo: "#d9d9d9",
            ink: ["#6e6e6e", "#a6a6a6", "#d9d9d9", "#ffffff"],
            shine: 1.0, glow: 1.0, light: 1.0
        },
        {
            // Dark ink on paper
            background: "#f2eee3", floor: "#e6e1d3", grid: "#cfc8b4",
            text: "#1a1a1a", dimmed: "#8a8474", title: "#1a1a1a", accent: "#c23b22",
            ball: "#1a1a1a", ballMark: "#c23b22", extraBall: "#555555",
            leftPlayer: "#1f3b73", rightPlayer: "#c23b22",
            wall: "#2b2b2b", goal: "#ddd6c4", shield: "#1f3b73",
            bumper: "#444444", block: "#6b6b6b", shadow: "#8a8474",
            starFrom: "#d9d3c2", starTo: "#bdb6a3",
            ink: ["#111111", "#333333", "#555555", "#777777"],
            shine: 0.35, glow: 0.0, light: 0.8
        },
        {
            // The four greens of the original Game Boy
            background: "#9bbc0f", floor: "#8bac0f", grid: "#7a9c0f",
            text: "#0f380f", dimmed: "#306230", title: "#0f380f", accent: "#0f380f",
            ball: "#0f380f", ballMark: "#8bac0f", extraBall: "#306230",
            leftPlayer: "#0f380f", rightPlayer: "#306230",
            wall: "#306230", goal: "#8bac0f", shield: "#306230",
            bumper: "#306230", block: "#306230", shadow: "#306230",
            starFrom: "#8bac0f", starTo: "#306230",
            ink: ["#0f380f", "#306230"],
            shine: 0.0, glow: 0.0, light: 0.72
        },
        {
            // An amber monochrome monitor
            background: "#070300", floor: "#0d0600", grid: "#3d1f00",
            text: "#ffb000", dimmed: "#8a5200", title: "#ffcc33", accent: "#ffcc33",
            ball: "#ffcc33", ballMark: "#8a5200", extraBall: "#ff9500",
            leftPlayer: "#ffb000", rightPlayer: "#ffcc33",
            wall: "#cc7a00", goal: "#140900", shield: "#ff9500",
            bumper: "#ff9500", block: "#b35c00", shadow: "#000000",
            starFrom: "#3d1f00", starTo: "#b35c00",
            ink: ["#b35c00", "#ff9500", "#ffb000", "#ffcc33"],
            shine: 0.6, glow: 1.0, light: 1.0
        }
    ]
    readonly property var palette: palettes[GraphicsSettings.theme] ?? palettes[0]

    readonly property color background: palette.background
    readonly property color floor: palette.floor
    readonly property color grid: palette.grid
    readonly property color text: palette.text
    readonly property color dimmed: palette.dimmed
    // The entries of a menu that aren't selected
    readonly property color unselected: Qt.tint(text, Qt.rgba(background.r, background.g, background.b, 0.25))
    readonly property color title: palette.title
    readonly property color accent: palette.accent
    readonly property color ball: palette.ball
    readonly property color ballMark: palette.ballMark
    readonly property color extraBall: palette.extraBall
    readonly property color leftPlayer: palette.leftPlayer
    readonly property color rightPlayer: palette.rightPlayer
    readonly property color paddle: leftPlayer
    readonly property color wall: palette.wall
    readonly property color goal: palette.goal
    readonly property color shield: palette.shield
    readonly property color bumper: palette.bumper
    readonly property color block: palette.block
    readonly property color shadow: palette.shadow
    readonly property color starFrom: palette.starFrom
    readonly property color starTo: palette.starTo

    // A color of the theme for any other color, by brightness
    function tint(color) {
        const ink = palette.ink
        if (!ink)
            return color
        const c = Qt.color(color)
        const brightness = 0.299 * c.r + 0.587 * c.g + 0.114 * c.b
        return ink[Math.min(ink.length - 1, Math.floor(brightness * ink.length))]
    }

    // How much self-lit surfaces shine, a little even without the bloom
    readonly property real glowScale: palette.glow * (GraphicsSettings.glow === GraphicsSettings.NoGlow ? 0.3 : 1.0)
    readonly property bool bloom: palette.glow > 0.0
    readonly property real shine: palette.shine
    readonly property real light: palette.light
    readonly property bool phong: GraphicsSettings.shading === GraphicsSettings.Phong

    // Closest camera distance in front of the scene it looks at, Main
    // backs off further until the scene fits the window
    readonly property real minimumCameraDistance: 20.0
    readonly property int cameraDuration: 250

    // Menu layout. Lists shrink their items and this base rhythm together.
    readonly property real menuTitleY: 8.6
    readonly property real menuTitleScale: 1.8
    readonly property real menuTitleMaxWidth: 30.0
    readonly property real menuAnnotationY: 6.4
    readonly property real menuAnnotationScale: 0.55
    readonly property real menuAnnotationMaxWidth: 32.0
    readonly property real menuListTop: 4.6
    readonly property real menuListBottom: -9.4
    readonly property real menuItemSpacing: 1.6
    readonly property real menuDetailedItemSpacing: 2.4
    readonly property real menuHintY: -11.5
    readonly property real menuHintScale: 0.5
    readonly property real menuHintMaxWidth: 34.0
}
