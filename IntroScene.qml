import QtQuick 2.13
import Qt3D.Core 2.13
import Qt3D.Render 2.13
import Qt3D.Input 2.13
import Qt3D.Extras 2.13
import "logic.js" as Logic

Scene {
    id: sceneRoot

    onActiveChanged: console.log("IntroScene active: " + active)

    Entity {
        ExtrudedTextWrapper {
            id: textMesh
            text: "YOOO! Intro here"
            depth: 1.0
            horizontalAlignment: Text.AlignHCenter
            ambientColor: "#FF00FF"
        }
        Transform {
            id: textTransform
            scale: 2
        }
        components: [textMesh, textTransform]
    }

    Entity {
        ExtrudedTextWrapper {
            id: anotherTextMesh
            text: "[Press any key]"
            depth: 1.0
            horizontalAlignment: Text.AlignHCenter
            ambientColor: "#FF00FF"
        }
        Transform {
            id: anotherTextTransform
            translation: Qt.vector3d(0, -4, 0)
        }
        components: [anotherTextMesh, anotherTextTransform]
    }

    KeyboardDevice {
        id: keyboardDevice
    }

    KeyboardHandler {
        id: keyboardHandler
        sourceDevice: keyboardDevice
        focus: sceneRoot.active
        enabled: sceneRoot.active
        onPressed: {


            console.log("INTRO onPressed")
            console.log("focus " + focus)
            console.log("enabled " + enabled)

            Logic.nextScene(root.menuScene)
            event.accepted = true;
        }
        onFocusChanged: console.log("KeyboardHandler focus: " + focus)
        onEnabledChanged: console.log("KeyboardHandler enabled: " + focus)
    }
}
