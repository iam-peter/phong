import QtQuick 2.13
import QtQuick.Scene3D 2.13
import Qt3D.Core 2.13
import Qt3D.Render 2.13
import Qt3D.Input 2.13
import Qt3D.Extras 2.13
import Qt3D.Logic 2.13
import "logic.js" as Logic

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

            // TODO: Debug camera
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

            IntroScene {
                id: introScene
                root: root
                camera: camera
                title: "intro"
                position: Qt.vector3d(-30, 0, 0)
                nextScene: menuScene
            }

            MenuScene {
                id: menuScene
                root: root
                camera: camera
                title: "menu"
                position: Qt.vector3d(0, 0, 0)
                menuScenes: [
                    gameScene,
                    settingsScene
                ]
            }

            GameScene {
                id: gameScene
                root: root
                camera: camera
                title: "play"
                position: Qt.vector3d(30, 0, 0)
            }

            Scene {
                id: settingsScene
                root: root
                camera: camera
                title: "settings"
                position: Qt.vector3d(60, 0, 0)
            }

            KeyboardDevice {
                id: keyboardDevice
            }

            KeyboardHandler {
                id: keyboardHandler
                sourceDevice: keyboardDevice
                focus: true
                onPressed: Logic.currentScene().onPressed(event)
                onEscapePressed: Logic.currentScene().onEscapePressed(event)
            }

            FrameAction {
                id: gameLoop
                //onTriggered: console.log("FrameAction.triggered() " + dt)
            }
        }
    }

    /*
    transitions: [
      Transition {
          SequentialAnimation {
              ScriptAction { script: Logic.transitionStarted() }
              Vector3dAnimation {
                  target: camera
                  properties: "position,viewCenter"
                  duration: 400
              }
              ScriptAction { script: Logic.transitionFinished() }
          }
      } ]
*/
    Component.onCompleted: {
        console.log(">>> onCompleted")
        gameState = Logic.initialise(camera)
        Logic.nextScene(introScene)
    }
}
