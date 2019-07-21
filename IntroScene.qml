import QtQuick 2.13
import Qt3D.Core 2.13
import Qt3D.Render 2.13
import Qt3D.Input 2.13
import Qt3D.Extras 2.13
import "logic.js" as Logic

Scene {
    id: scene

    property Scene nextScene

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

    onPressed: function (event) {
        console.log("Intro | KeyboardHandler | onPressed")

        Logic.nextScene(nextScene)
        event.accepted = true;
    }
    onEscapePressed: function (event) {
        event.accepted = true;
    }
}
