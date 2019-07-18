import QtQuick 2.13
import QtQuick.Scene3D 2.13
import Qt3D.Core 2.13
import Qt3D.Render 2.13
import Qt3D.Input 2.13
import Qt3D.Extras 2.13

Item {
    id: root
    visible: true
    width: 640
    height: 480

    Rectangle {
        anchors.fill: parent
        color: "black"
    }

    property var gameState

    Scene3D {
        id: scene3d
        anchors.fill: parent
        anchors.margins: 10
        focus: true
        aspects: ["input", "logic"]
        cameraAspectRatioMode: Scene3D.AutomaticAspectRatio

        Entity {
            id: sceneRoot

            Camera {
                id: camera
                projectionType: CameraLens.PerspectiveProjection
                fieldOfView: 45
                nearPlane: 0.1
                farPlane: 1000.0
                position: Qt.vector3d(0.0, 0.0, 34.0)
                upVector: Qt.vector3d(0.0, 1.0, 0.0)
                viewCenter: Qt.vector3d(0.0, 0.0, 0.0)
            }

            //FirstPersonCameraController { camera: camera }

            components: [
                RenderSettings {
                    activeFrameGraph: ForwardRenderer {
                        camera: camera
                        clearColor: "transparent"
                    }
                },
                InputSettings {}
            ]


            MenuScreen {
                id: menu
                root: root
                camera: camera
            }

            GameCanvas {
                id: game
                root: root
                camera: camera
            }
        }
    }


    Timer {
        id: gameStarter
        interval: 4000
        running: false
        repeat: false
        onTriggered: Logic.startGame(canvas);
    }

    states: [
        State {
            name: "gameOn"
            when: gameState.gameOver === false && passedSplash
            //PropertyChanges { target: view; y: -(height - 960) }
            //StateChangeScript { script: root.countdown = 0; }
            //PropertyChanges { target: gameStarter; running: true }
        },
        State {
            name: "gameOver"
            when: gameState.gameOver === true
            //PropertyChanges { target: view; y: 0 }
        }
    ]

    function changeScene(name) {
        switch (name) {
            case "play":
                camera.viewCenter = Qt.vector3d(game.x, game.y, 0.0)
                camera.position = Qt.vector3d(game.x, game.y, 34.0)
                break;
            default:
                break;
        }

    }

    //Component.onCompleted: gameState = Logic.newGameState(canvas);
}
