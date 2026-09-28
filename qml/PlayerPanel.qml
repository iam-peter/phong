pragma ComponentBehavior: Bound

import QtQuick
import QtQuick3D

// Everything about a player beside the field: name, score, sets, active
// effects, the power bar and the keys
Node {
    id: root

    property string name
    property color color: Theme.text
    property string score
    property int sets: 0
    property int setsToWin: 1
    // Definitions of the active effects, see Modifiers.activeEffects()
    property var effects: []
    property real power: 0.0
    property bool showPower: true
    // Balls to lose in solo play, 0 for none
    property int lives: 0
    property int livesLeft: 0
    property var hints: []

    // Long names, e.g. of machines on the network, shrink to the panel
    Text3D {
        id: nameText
        readonly property real fit: Math.min(0.8, 4.6 / Math.max(textWidth, 0.1))
        y: 8.4
        scale: Qt.vector3d(fit, fit, fit)
        horizontalAlignment: Text.AlignHCenter
        color: root.color
        glow: 0.5
        text: root.name
    }

    Text3D {
        y: 5.0
        scale: Qt.vector3d(2.2, 2.2, 2.2)
        horizontalAlignment: Text.AlignHCenter
        // Seen from the side, a deep score would look like a block
        depth: 0.15
        text: root.score
    }

    // Won sets, or the balls left
    Repeater3D {
        model: root.lives > 0 ? root.lives : root.setsToWin > 1 ? root.setsToWin : 0

        delegate: Disc {
            required property int index
            readonly property int count: root.lives > 0 ? root.lives : root.setsToWin
            x: (index - 0.5 * (count - 1)) * 0.8
            y: 4.1
            radius: root.lives > 0 ? 0.3 : 0.22
            sphere: root.lives > 0
            thickness: 0.2
            color: root.lives > 0 ? (index < root.livesLeft ? root.color : Theme.goal)
                                  : (index < root.sets ? Theme.title : Theme.goal)
            glow: 0.4
        }
    }

    Repeater3D {
        model: root.effects

        delegate: ModifierItem {
            required property var modelData
            required property int index
            x: (index - 0.5 * (root.effects.length - 1)) * 1.15
            y: 2.8
            radius: 0.5
            wobbling: false
            color: modelData.color
            glyph: modelData.glyph
        }
    }

    PowerBar {
        visible: root.showPower
        y: -2.6
        vertical: true
        power: root.power
        color: root.color
    }

    Repeater3D {
        model: root.hints

        delegate: Text3D {
            required property string modelData
            required property int index
            y: -3.6 - index * 0.85
            scale: Qt.vector3d(0.4, 0.4, 0.4)
            horizontalAlignment: Text.AlignHCenter
            color: Theme.dimmed
            text: modelData
        }
    }
}
