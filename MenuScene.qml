import QtQuick 2.13
import Qt3D.Core 2.13
import Qt3D.Render 2.13
import Qt3D.Input 2.13
import Qt3D.Extras 2.13
import "logic.js" as Logic

Scene {
    id: scene

    onActiveChanged: console.log("Menu | onActiveChanged: " + active)

    Entity {
        id: titleEntity
        property real ballTextGap: 0.2

        Entity {
            id: title1Entity

            ExtrudedTextWrapper {
                id: title1Mesh
                text: "p(h)"
                depth: 1.0
                font.family: "monospace"
                font.capitalization: Font.AllUppercase
            }

            Transform {
                id: title1Transform
                translation: Qt.vector3d(0, 0, 0)
            }

            components: [title1Mesh, title1Transform]
        }

        Entity {
            id: titleBallEntity

            PhongMaterial {
                id: ballMaterial
                ambient: "#FF0000"
            }

            CylinderMesh {
                id: titleBallMesh
                length: 1.0
                radius: 0.8
            }

            Transform {
                id: titleBallTransform
                translation: Qt.vector3d(title1Mesh.width + titleBallMesh.radius + titleEntity.ballTextGap, titleBallMesh.length * 0.5, 0)
                rotationX: 90.0
            }

            components: [titleBallMesh, ballMaterial, titleBallTransform]
        }

        Entity {
            id: title2Entity

            ExtrudedTextWrapper {
                id: title2Mesh
                text: "ng"
                depth: 1.0
                font.family: "monospace"
                font.capitalization: Font.AllUppercase
            }

            Transform {
                id: title2Transform
                translation: Qt.vector3d(titleBallTransform.translation.x + titleBallMesh.radius + titleEntity.ballTextGap, 0, 0)
            }

            components: [title2Mesh, title2Transform]
        }

        components: Transform {
            property var tmpTranslation: Qt.vector3d((title1Mesh.width + title2Mesh.width + (titleBallMesh.radius + titleEntity.ballTextGap) * 2) * -0.5 , 2, 0)

            translation: tmpTranslation.times(scale)
            scale: 2
        }
    }

    property int currentIdx: 0

    PhongMaterial {
        id: underlineMaterial
        ambient: "#00FF00"
    }

    NodeInstantiator {
        id: nodeInstantiator
        model: root.menuScenes

        Entity {
            id: menuItemEntity

            Entity {
                id: textEntity

                ExtrudedTextWrapper {
                    id: textMesh
                    text: modelData.menuItemName
                    depth: 1.0
                    font.family: "monospace"
                    //font.bold: true
                    font.capitalization: Font.AllUppercase
                    horizontalAlignment: Text.AlignHCenter
                }

                Transform {
                    id: textTransform
                    translation: Qt.vector3d(0, index * -2, 0)
                }

                components: [textMesh, textTransform]
            }

            Entity {
                id: underlineEntity
                enabled: (index === currentIdx)

                CuboidMesh {
                    id: underlineMesh
                }

                Transform {
                    id: underlineTransform
                    translation: Qt.vector3d(0, (index * -2) - 0.3, 0)
                    scale3D: Qt.vector3d((textMesh.maxExt.x - textMesh.minExt.x), 0.2, 1)
                }

                components: [underlineMesh, underlineMaterial, underlineTransform]
            }
        }
    }

    onPressed: function (event) {
        console.log("MenuScene | onPressed")

        if (event.key === Qt.Key_Up || event.key === Qt.Key_Down) {
            currentIdx = Math.max(0, Math.min(currentIdx + ((event.key === Qt.Key_Up) ? -1 : 1), nodeInstantiator.model.length - 1))
            camera.viewCenter = scene.position.plus(Qt.vector3d(0.0, currentIdx * -2, 0.0))
            camera.position = scene.position.plus(Qt.vector3d(0.0, currentIdx * -2, 34.0))
            event.accepted = true;
        }

        if (event.key === Qt.Key_Enter || event.key === Qt.Key_Return) {
            console.log("Selected: " + nodeInstantiator.model[currentIdx].menuItemName)
            Logic.nextScene(nodeInstantiator.model[currentIdx])
            event.accepted = true;
        }
    }
}
