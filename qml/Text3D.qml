import QtQuick
import QtQuick3D
import Phong

// Text with alignment around its origin. The letters come from the
// FontAtlas and are drawn with all the other texts of the scene at once,
// in the TextLayer of the nearest ancestor with a textBatch, or one of
// its own. A few darker copies behind give it its depth.
Node {
    id: root

    property string text
    // From the back at 0 to the front
    property real depth: 0.5
    property color color: Theme.text
    // Titles glow more than labels
    property real glow: 0.05

    property int horizontalAlignment: Text.AlignLeft
    property int verticalAlignment: Text.AlignBottom

    // Makes the text react to taps and clicks, see Main.clickableAt()
    property bool clickable: false
    signal clicked()
    // Room around the letters that reacts too, in scene units. Arrows
    // reach far to the outside, a single letter is small for a finger.
    property real hitLeft: 0.3
    property real hitRight: 0.3
    property real hitVertical: 0.3

    // Size in scene units, from the font metrics, the letters are placed
    // in the same units: a point of the font is a unit. The width reaches
    // from the origin to the right ink edge.
    readonly property real textWidth: (metrics.tightBoundingRect.x + metrics.tightBoundingRect.width)
                                      / metrics.font.pointSize
    readonly property real textHeight: metrics.tightBoundingRect.height / metrics.font.pointSize

    // Where the letters start from the origin, for the TextBatch.
    // Vertical alignment follows the ink, the baseline is the default.
    readonly property real alignX: horizontalAlignment === Text.AlignRight ? -textWidth
                                   : horizontalAlignment === Text.AlignHCenter ? -0.5 * textWidth
                                   : 0.0
    readonly property real alignY: verticalAlignment === Text.AlignTop
                                   ? metrics.tightBoundingRect.y / metrics.font.pointSize
                                   : verticalAlignment === Text.AlignVCenter
                                   ? (metrics.tightBoundingRect.y + 0.5 * metrics.tightBoundingRect.height)
                                     / metrics.font.pointSize
                                   : 0.0

    property TextBatch batch: null

    TextMetrics {
        id: metrics
        font.family: Theme.fontFamily
        font.pointSize: 20
        font.capitalization: Font.AllUppercase
        text: root.text
    }

    // Invisible hit area, picking the glyphs alone misses between letters
    Model {
        visible: root.clickable
        pickable: root.clickable
        source: "#Rectangle"
        // Metrics grow downwards from the baseline, the letters upwards
        position: Qt.vector3d(root.alignX + (metrics.tightBoundingRect.x + 0.5 * metrics.tightBoundingRect.width)
                                  / metrics.font.pointSize + 0.5 * (root.hitRight - root.hitLeft),
                              root.alignY - (metrics.tightBoundingRect.y + 0.5 * metrics.tightBoundingRect.height)
                                  / metrics.font.pointSize,
                              root.depth)
        scale: Qt.vector3d((metrics.tightBoundingRect.width / metrics.font.pointSize + root.hitLeft + root.hitRight) / 100,
                           (root.textHeight + 2 * root.hitVertical) / 100, 1.0)
        materials: DefaultMaterial {
            opacity: 0.0
            depthDrawMode: Material.NeverDepthDraw
            lighting: DefaultMaterial.NoLighting
        }
    }

    Component {
        id: ownLayer
        TextLayer {}
    }

    Component.onCompleted: {
        let node = root.parent
        while (node && node.textBatch === undefined)
            node = node.parent
        batch = node ? node.textBatch : ownLayer.createObject(root).batch
        batch.add(root)
    }
    Component.onDestruction: batch?.remove(root)
}
