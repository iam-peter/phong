import QtQuick
import QtQuick3D

// A screen of the game. All scenes live side by side in one 3D world and
// the camera flies to the active one. Input is forwarded by Main.
Node {
    id: root

    property string name
    property bool active: false
    property var phong

    signal keyPressed(var event)
    signal keyReleased(var event)

    // Pointer positions are window coordinates
    signal pointerPressed(int id, real x, real y)
    signal pointerMoved(int id, real x, real y)
    signal pointerReleased(int id)

    signal focusLost()
}
