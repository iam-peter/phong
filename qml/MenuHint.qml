import QtQuick
import QtQuick3D
import Phong

Text3D {
    id: root

    readonly property real fittedScale: Math.min(Theme.menuHintScale,
                                                  Theme.menuHintMaxWidth / Math.max(textWidth, 0.001))

    y: Theme.menuHintY
    scale: Qt.vector3d(fittedScale, fittedScale, fittedScale)
    horizontalAlignment: Text.AlignHCenter
    color: Theme.dimmed
}