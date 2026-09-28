import QtQuick
import QtQuick3D

Window {
    id: phong

    width: 800
    height: 600
    minimumWidth: 200
    minimumHeight: 100
    visible: true
    color: "black"
    title: qsTr("Phong")

    property var sceneStack: []
    readonly property Scene currentScene: sceneStack.length ? sceneStack[sceneStack.length - 1] : null

    // Scene layout follows the window: the camera backs off until the widest
    // scene fits, and the scenes are spread so that only the current one is
    // in view. The game scene stays at the origin, the physics world with it.
    readonly property real contentHalfWidth: 18.5
    readonly property real contentHalfHeight: 13.0
    readonly property real tanHalfFieldOfView: Math.tan(0.5 * camera.fieldOfView * Math.PI / 180)
    readonly property real aspectRatio: Math.max(width, 1) / Math.max(height, 1)
    readonly property real cameraDistance: Math.max(Theme.cameraDistance,
                                                    contentHalfWidth / (tanHalfFieldOfView * aspectRatio))
    readonly property real sceneSpacingX: Math.max(40, cameraDistance * tanHalfFieldOfView * aspectRatio
                                                       + contentHalfWidth + 1.0)
    readonly property real sceneSpacingY: Math.max(40, cameraDistance * tanHalfFieldOfView
                                                       + contentHalfHeight + 1.0)

    function nextScene(scene) {
        if (currentScene)
            currentScene.active = false

        sceneStack = sceneStack.concat([scene])
        transformCamera(scene.position)
        scene.active = true
    }

    function previousScene() {
        if (sceneStack.length < 2)
            return

        currentScene.active = false
        sceneStack = sceneStack.slice(0, -1)
        transformCamera(currentScene.position)
        currentScene.active = true
    }

    // Pops scenes until scene is the current one
    function returnTo(scene) {
        const index = sceneStack.indexOf(scene)
        if (index < 0 || scene === currentScene)
            return

        currentScene.active = false
        sceneStack = sceneStack.slice(0, index + 1)
        transformCamera(currentScene.position)
        currentScene.active = true
    }

    function cameraPosition(scene) {
        return scene.position.plus(Qt.vector3d(0, 0, cameraDistance))
    }

    function transformCamera(position) {
        cameraAnimation.stop()
        cameraAnimation.from = camera.position
        cameraAnimation.to = position.plus(Qt.vector3d(0, 0, cameraDistance))
        cameraAnimation.start()
    }

    // The scenes moved with the window size, catch up with the current one
    function snapCamera() {
        if (!currentScene)
            return

        cameraAnimation.stop()
        camera.position = cameraPosition(currentScene)
    }

    onWidthChanged: Qt.callLater(snapCamera)
    onHeightChanged: Qt.callLater(snapCamera)

    function startGame(mode) {
        gameScene.mode = mode
        nextScene(gameScene)
        gameScene.startMatch()
    }

    function showResults() {
        nextScene(resultScene)
    }

    function rematch() {
        returnTo(gameScene)
        gameScene.startMatch()
    }

    // Intersection of the pointer ray with the plane z = 0 of scene
    function toScene(x, y, scene) {
        const near = view.mapTo3DScene(Qt.vector3d(x, y, 0))
        const far = view.mapTo3DScene(Qt.vector3d(x, y, 10))
        const t = (scene.scenePosition.z - near.z) / (far.z - near.z)
        return scene.mapPositionFromScene(near.plus(far.minus(near).times(t)))
    }

    // Clickable Text3D of the current scene under the pointer, or null
    function clickableAt(x, y) {
        let node = view.pick(x, y).objectHit
        while (node && node.clickable !== true)
            node = node.parent

        let scene = node
        while (scene && scene !== currentScene)
            scene = scene.parent

        return scene ? node : null
    }

    onActiveChanged: {
        if (!active && currentScene)
            currentScene.focusLost()
    }

    View3D {
        id: view

        anchors.fill: parent
        focus: true
        camera: camera

        environment: SceneEnvironment {
            clearColor: "black"
            backgroundMode: SceneEnvironment.Color
            antialiasingMode: SceneEnvironment.MSAA
            antialiasingQuality: SceneEnvironment.High
        }

        PerspectiveCamera {
            id: camera

            fieldOfView: 45
            clipNear: 0.1
            clipFar: 1000

            // The light travels with the camera
            PointLight {
                brightness: 1.0
                constantFade: 1.0
                linearFade: 0.0
                quadraticFade: 0.0
            }
        }

        Vector3dAnimation {
            id: cameraAnimation
            target: camera
            property: "position"
            duration: Theme.cameraDuration
            easing.type: Easing.InOutQuad
        }

        MenuScene {
            id: menuScene
            phong: phong
            position: Qt.vector3d(-phong.sceneSpacingX, 0, 0)
            settingsScene: settingsScene
        }

        GameScene {
            id: gameScene
            phong: phong
            position: Qt.vector3d(0, 0, 0)
        }

        SettingsScene {
            id: settingsScene
            phong: phong
            position: Qt.vector3d(-phong.sceneSpacingX, phong.sceneSpacingY, 0)
        }

        ResultScene {
            id: resultScene
            phong: phong
            position: Qt.vector3d(0, -phong.sceneSpacingY, 0)
            match: gameScene.match
            menuScene: menuScene
            mode: gameScene.mode
        }

        Keys.onPressed: (event) => {
            if (phong.currentScene)
                phong.currentScene.keyPressed(event)
        }
        Keys.onReleased: (event) => {
            if (phong.currentScene)
                phong.currentScene.keyReleased(event)
        }
    }

    // Mouse and touch, forwarded to the current scene like the keys
    MultiPointTouchArea {
        anchors.fill: parent
        mouseEnabled: true
        maximumTouchPoints: 4

        onPressed: (points) => {
            view.forceActiveFocus()
            for (const point of points)
                phong.currentScene?.pointerPressed(point.pointId, point.x, point.y)
        }
        onUpdated: (points) => {
            for (const point of points)
                phong.currentScene?.pointerMoved(point.pointId, point.x, point.y)
        }
        onReleased: (points) => {
            for (const point of points)
                phong.currentScene?.pointerReleased(point.pointId)
        }
        onCanceled: (points) => {
            for (const point of points)
                phong.currentScene?.pointerReleased(point.pointId)
        }
    }

    Component.onCompleted: {
        camera.position = cameraPosition(menuScene)
        nextScene(menuScene)
    }
}
