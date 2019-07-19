import QtQuick 2.13
import Qt3D.Core 2.13
import Qt3D.Render 2.13
import Qt3D.Input 2.13
import Qt3D.Extras 2.13
import "logic.js" as Logic

Scene {
    id: scene

    onActiveChanged: console.log("Intro | onActiveChanged: " + active)

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
        focus: scene.active

        onPressed: {
            if (!scene.active) return;

            console.log("Intro | onPressed")

            Logic.nextScene(root.menuScene)
            event.accepted = true;
        }

        onFocusChanged: console.log("Intro | onFocusChanged: " + focus)
        onEnabledChanged: console.log("Intro | onEnabledChanged: " + enabled)
    }
}
