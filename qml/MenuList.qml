import QtQuick
import QtQuick3D
import Phong

Node {
    id: root

    property int count: 0
    property real contentHeight: count * Theme.menuItemSpacing
    property real top: Theme.menuListTop
    property real bottom: Theme.menuListBottom
    property real baseSpacing: Theme.menuItemSpacing

    readonly property real availableHeight: top - bottom
    readonly property real scaleFactor: Math.min(1.0, availableHeight / Math.max(contentHeight, 0.001))
    readonly property real spacing: baseSpacing * scaleFactor
    readonly property vector3d itemScale: Qt.vector3d(scaleFactor, scaleFactor, scaleFactor)

    function yFor(index: int): real {
        return top - index * spacing
    }

    function yForOffset(offset: real): real {
        return top - offset * scaleFactor
    }
}