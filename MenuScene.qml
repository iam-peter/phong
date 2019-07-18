import QtQuick 2.13
import Qt3D.Core 2.13
import Qt3D.Render 2.13
import Qt3D.Input 2.13
import Qt3D.Extras 2.13

Scene {
    id: menu

    property int currentIdx: 0

    PhongMaterial {
        id: textMaterial
        ambient: "#FFFFFF"
    }

    PhongMaterial {
        id: underlineMaterial
        ambient: "#00FF00"
    }

    Entity {
        id: titleEntity

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

            components: [title1Mesh, textMaterial, title1Transform]
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
                translation: Qt.vector3d(title1Mesh.maxExt.x + 1.0, (title1Mesh.maxExt.y - title1Mesh.minExt.y) * 0.5, 0)
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
                translation: Qt.vector3d(title1Mesh.maxExt.x + 2.0, 0, 0)
            }

            components: [title2Mesh, textMaterial, title2Transform]
        }

        Transform {
            id: titleTransform
            translation: Qt.vector3d(((title1Mesh.maxExt.x - title1Mesh.minExt.x) + 2.0 +(title2Mesh.maxExt.x - title2Mesh.minExt.x)) * -1 , 4, 0)
            scale: 2
        }

         components: [titleTransform]
    }


    ListModel {
        id: menuModel

        ListElement { name: "play" }
        ListElement { name: "settings" }
        ListElement { name: "help" }
        ListElement { name: "credits" }
        ListElement { name: "quit" }
    }

    NodeInstantiator {
        model: menuModel

        Entity {
            id: menuItemEntity

            Entity {
                id: textEntity

                ExtrudedTextWrapper {
                    id: textMesh
                    text: name
                    depth: 1.0
                    font.family: "monospace"
                    //font.bold: true
                    font.capitalization: Font.AllUppercase
                }

                Transform {
                    id: textTransform
                    translation: Qt.vector3d((textMesh.maxExt.x - textMesh.minExt.x) * -0.5, index * -2, 0)
                    //scale: 2
                }

                components: [textMesh, textMaterial, textTransform]
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

    KeyboardDevice {
        id: keyboardDevice
    }

    KeyboardHandler {
        id: keyboardHandler
        sourceDevice: keyboardDevice
        focus: true
        onPressed: {
            if (event.key === Qt.Key_Up || event.key === Qt.Key_Down) {
                currentIdx = Math.max(0, Math.min(currentIdx + ((event.key === Qt.Key_Up) ? -1 : 1), menuModel.count - 1))
                camera.viewCenter = Qt.vector3d(0.0, currentIdx * -2, 0.0)
                camera.position = Qt.vector3d(0.0, currentIdx * -2, 34.0)

                event.accepted = true;
            }

            if (event.key === Qt.Key_Enter || event.key === Qt.Key_Return) {
                console.log("Selected: " + menuModel.get(currentIdx).name)

                root.state = menuModel.get(currentIdx).name

                event.accepted = true;
            }
        }
    }
}
