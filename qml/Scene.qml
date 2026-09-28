import QtQuick
import QtQuick3D

// A screen of the game. All scenes live side by side in one 3D world and
// the camera flies to the active one. Input is forwarded by Main.
Node {
    id: root

    property string name
    property bool active: false
    property var phong

    // Only the scene in view and the one the camera leaves are drawn
    visible: active || phong?.leavingScene === root

    // All the texts of the scene in one draw call, see Text3D
    readonly property TextBatch textBatch: sceneText.batch

    TextLayer {
        id: sceneText
    }

    // Added to the camera while the scene is active, e.g. for shaking
    property vector3d viewOffset: Qt.vector3d(0, 0, 0)
    property vector3d viewRotation: Qt.vector3d(0, 0, 0)
    // 0 to 1, for effects of the view
    property real slowMotion: 0.0

    // How far the content reaches from the middle, the camera comes as
    // close as it fits
    property real contentHalfWidth: 18.5
    property real contentHalfHeight: 13.0

    signal keyPressed(var event)
    signal keyReleased(var event)

    // Pointer positions are window coordinates
    signal pointerPressed(int id, real x, real y)
    signal pointerMoved(int id, real x, real y)
    signal pointerReleased(int id)

    signal focusLost()

    // Gamepads navigate like the arrow keys, Return and Escape, while the
    // scene wants that, e.g. not during a game
    property bool menuNavigation: true
    signal gamepadNavigated(var pad, int key)
    onGamepadNavigated: (pad, key) => {
        if (menuNavigation)
            keyPressed({ key: key, isAutoRepeat: false, accepted: false, gamepad: true })
    }
}
