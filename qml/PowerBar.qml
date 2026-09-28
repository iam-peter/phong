import QtQuick
import QtQuick3D

// A power bar of eight segments, pulsing when full
Node {
    id: bar

    property real power: 0.0
    property color color: Theme.text
    // Grows upwards instead of sideways, away from the middle
    property bool vertical: false
    property int direction: 1
    readonly property bool full: power >= 1.0
    property real pulse: 0.0

    SequentialAnimation on pulse {
        running: bar.full
        loops: Animation.Infinite
        NumberAnimation { from: 0.0; to: 1.0; duration: 300 }
        NumberAnimation { from: 1.0; to: 0.0; duration: 300 }
    }

    // The segments in one draw call, the full ones pulse together
    Instances {
        source: "#Cube"
        meshScale: bar.vertical ? Qt.vector3d(1.2 / 100, 0.4 / 100, 0.004) : Qt.vector3d(0.7 / 100, 0.35 / 100, 0.004)
        lighting: DefaultMaterial.NoLighting
        glow: bar.full ? (0.8 + 1.2 * bar.pulse) / 0.5 : 1.0
        items: {
            const items = []
            for (let index = 0; index < 8; ++index) {
                const filled = bar.power * 8 >= index + 1 - 1e-6
                items.push({
                    x: bar.vertical ? 0.0 : bar.direction * (0.35 + index * 0.78),
                    y: bar.vertical ? 0.22 + index * 0.52 : 0.0,
                    color: filled ? bar.color : Theme.goal,
                    glow: filled ? 0.5 : 0.0
                })
            }
            return items
        }
    }
}
