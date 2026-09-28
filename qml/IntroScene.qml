import QtQuick

Scene {
    id: root

    property Scene followingScene

    function next() {
        phong.nextScene(followingScene)
    }

    onKeyPressed: (event) => {
        event.accepted = true
        if (event.key !== Qt.Key_Escape && !event.isAutoRepeat)
            next()
    }
    onPointerPressed: next()

    Text3D {
        scale: Qt.vector3d(2, 2, 2)
        horizontalAlignment: Text.AlignHCenter
        text: qsTr("Qt Challenge #1")
        color: Theme.title
    }

    Text3D {
        y: -4
        horizontalAlignment: Text.AlignHCenter
        text: qsTr("[Press any key]")
        font.capitalization: Font.MixedCase
    }
}
