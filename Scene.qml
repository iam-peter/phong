import QtQuick 2.13
import Qt3D.Core 2.13
import Qt3D.Render 2.13
import Qt3D.Input 2.13
import "logic.js" as Logic

Entity {
    id: scene

    property bool active: false
    property var position: Qt.vector3d(0, 0, 0)

    property Item root
    property Camera camera
    property string menuItemName: "unknown"

    property var onPressed: function (event) {
        console.log("Scene | onPressed")
    }

    property var onEscapePressed: function (event) {
        console.log("Scene | onEscapePressed")

        Logic.previousScene()
        event.accepted = true;
    }

    Transform {
        id: sceneTransform
        translation: scene.position
    }

    components: [sceneTransform]
}
