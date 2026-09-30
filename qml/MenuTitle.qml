import QtQuick
import QtQuick3D
import Phong

Text3D {
    id: root

    readonly property real fittedScale: Math.min(Theme.menuTitleScale,
                                                  Theme.menuTitleMaxWidth / Math.max(textWidth, 0.001))

    y: Theme.menuTitleY
    scale: Qt.vector3d(fittedScale, fittedScale, fittedScale)
    horizontalAlignment: Text.AlignHCenter
    color: Theme.title
    glow: 0.8
}