import QtQuick
import QtQuick3D
import QtQuick3D.Helpers

// Extruded text with alignment around its origin
Node {
    id: root

    property alias text: geometry.text
    property alias font: geometry.font
    property alias depth: geometry.depth
    property color color: Theme.text

    property int horizontalAlignment: Text.AlignLeft
    property int verticalAlignment: Text.AlignBottom

    // Makes the text react to taps and clicks, see Main.clickableAt()
    property bool clickable: false
    signal clicked()

    // Size in scene units, from the font metrics: Model.bounds only updates
    // while the scene is synced for rendering, and moving the model then is
    // lost on wasm. The geometry is normalized by the point size, and like
    // its bounds the width reaches from the origin to the right ink edge.
    readonly property real textWidth: (metrics.tightBoundingRect.x + metrics.tightBoundingRect.width)
                                      / geometry.font.pointSize
    readonly property real textHeight: metrics.tightBoundingRect.height / geometry.font.pointSize

    TextMetrics {
        id: metrics
        font: geometry.font
        text: root.text
    }

    Model {
        id: model

        x: root.horizontalAlignment === Text.AlignRight ? -root.textWidth
           : root.horizontalAlignment === Text.AlignHCenter ? -0.5 * root.textWidth
           : 0.0
        // Vertical alignment follows the ink, the baseline is the default
        y: root.verticalAlignment === Text.AlignTop
           ? metrics.tightBoundingRect.y / geometry.font.pointSize
           : root.verticalAlignment === Text.AlignVCenter
           ? (metrics.tightBoundingRect.y + 0.5 * metrics.tightBoundingRect.height) / geometry.font.pointSize
           : 0.0

        geometry: ExtrudedTextGeometry {
            id: geometry
            // The default QtConcurrent generation never finishes on
            // single-threaded wasm, and short labels are cheap anyway
            asynchronous: false
            depth: 0.5
            font.family: Theme.fontFamily
            font.pointSize: 20
            font.capitalization: Font.AllUppercase
        }
        materials: DefaultMaterial {
            diffuseColor: root.color
            specularAmount: 0.0
        }

        // Invisible hit area, picking the glyphs alone misses between letters
        Model {
            visible: root.clickable
            pickable: root.clickable
            source: "#Rectangle"
            // Metrics grow downwards from the baseline, the geometry upwards
            position: Qt.vector3d((metrics.tightBoundingRect.x + 0.5 * metrics.tightBoundingRect.width)
                                      / geometry.font.pointSize,
                                  -(metrics.tightBoundingRect.y + 0.5 * metrics.tightBoundingRect.height)
                                      / geometry.font.pointSize,
                                  root.depth)
            scale: Qt.vector3d((metrics.tightBoundingRect.width / geometry.font.pointSize + 0.6) / 100,
                               (root.textHeight + 0.6) / 100, 1.0)
            materials: DefaultMaterial {
                opacity: 0.0
                depthDrawMode: Material.NeverDepthDraw
            }
        }
    }
}
