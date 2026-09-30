import QtQuick
import QtQuick3D
import Phong

Text3D {
    id: root

    property real maximumWidth: Theme.menuAnnotationMaxWidth
    readonly property real fittedScale: Math.min(Theme.menuAnnotationScale,
                                                  maximumWidth / Math.max(textWidth, 0.001))

    y: Theme.menuAnnotationY
    scale: Qt.vector3d(fittedScale, fittedScale, fittedScale)
    horizontalAlignment: Text.AlignHCenter
    color: Theme.dimmed
}