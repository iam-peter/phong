import QtQuick
import QtQuick3D
import Phong

OptionsScene {
    id: root

    title: qsTr("Graphics")

    rows: [
        {
            label: qsTr("Shading"),
            values: [GraphicsSettings.Phong, GraphicsSettings.Flat],
            names: [qsTr("Phong"), qsTr("Flat")],
            get: () => GraphicsSettings.shading,
            set: (value) => GraphicsSettings.shading = value
        },
        {
            label: qsTr("Glow"),
            values: [GraphicsSettings.NoGlow, GraphicsSettings.LowGlow, GraphicsSettings.MediumGlow,
                     GraphicsSettings.HighGlow],
            names: [qsTr("Off"), qsTr("Low"), qsTr("Medium"), qsTr("High")],
            get: () => GraphicsSettings.glow,
            set: (value) => GraphicsSettings.glow = value
        },
        {
            label: qsTr("Anti-aliasing"),
            values: [GraphicsSettings.NoAntialiasing, GraphicsSettings.FastAntialiasing,
                     GraphicsSettings.Multisample2x, GraphicsSettings.Multisample4x],
            names: [qsTr("Off"), qsTr("Fast"), qsTr("2x"), qsTr("4x")],
            get: () => GraphicsSettings.antialiasing,
            set: (value) => GraphicsSettings.antialiasing = value
        },
        {
            label: qsTr("Stars"),
            values: [true, false],
            names: [qsTr("On"), qsTr("Off")],
            get: () => GraphicsSettings.stars,
            set: (value) => GraphicsSettings.stars = value
        },
        {
            label: qsTr("Floor"),
            values: [true, false],
            names: [qsTr("On"), qsTr("Off")],
            get: () => GraphicsSettings.floor,
            set: (value) => GraphicsSettings.floor = value
        },
        {
            label: qsTr("Shadows"),
            values: [true, false],
            names: [qsTr("On"), qsTr("Off")],
            get: () => GraphicsSettings.shadows,
            set: (value) => GraphicsSettings.shadows = value
        },
        {
            label: qsTr("Show FPS"),
            values: [false, true],
            names: [qsTr("Off"), qsTr("On")],
            get: () => GraphicsSettings.showFps,
            set: (value) => GraphicsSettings.showFps = value
        },
        {
            label: qsTr("Restore defaults"),
            values: [],
            names: [],
            set: () => GraphicsSettings.restoreDefaults()
        }
    ]

    // A Phong shaded sphere to judge the settings by
    Node {
        position: Qt.vector3d(0, -9.1, 0)

        Disc {
            sphere: true
            radius: 0.9
            color: Theme.ball
            glow: 0.35
            shininess: 1.0

            NumberAnimation on eulerRotation.y {
                from: 0
                to: 360
                duration: 8000
                loops: Animation.Infinite
            }
        }
    }
}
