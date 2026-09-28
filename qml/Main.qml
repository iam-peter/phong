import QtQuick
import QtQuick3D
import QtQuick3D.Helpers
import Phong

Window {
    id: phong

    width: 800
    height: 600
    minimumWidth: 200
    minimumHeight: 100
    visible: true
    color: Theme.background
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
        cameraAnimation.from = cameraRig.position
        cameraAnimation.to = position.plus(Qt.vector3d(0, 0, cameraDistance))
        cameraAnimation.start()
    }

    // The scenes moved with the window size, catch up with the current one
    function snapCamera() {
        if (!currentScene)
            return

        cameraAnimation.stop()
        cameraRig.position = cameraPosition(currentScene)
    }

    onWidthChanged: Qt.callLater(snapCamera)
    onHeightChanged: Qt.callLater(snapCamera)

    function startGame(mode) {
        gameScene.mode = mode
        gameScene.ladderStage = 0
        nextScene(gameScene)
        gameScene.startMatch()
    }

    // The next computer level of the ladder
    function nextLadderLevel() {
        gameScene.ladderStage = Math.min(gameScene.ladderStage + 1, 2)
        rematch()
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

        // Glow and anti-aliasing follow the graphics settings, the glow
        // picks up what is brighter than white, the self-lit surfaces
        environment: ExtendedSceneEnvironment {
            readonly property int glow: GraphicsSettings.glow
            readonly property int antialiasing: GraphicsSettings.antialiasing

            clearColor: Theme.background
            backgroundMode: SceneEnvironment.Color

            antialiasingMode: antialiasing >= GraphicsSettings.Multisample2x ? SceneEnvironment.MSAA
                                                                             : SceneEnvironment.NoAA
            antialiasingQuality: antialiasing === GraphicsSettings.Multisample4x ? SceneEnvironment.High
                                                                                : SceneEnvironment.Medium
            fxaaEnabled: antialiasing === GraphicsSettings.FastAntialiasing

            // Higher glow spreads wider, only high blurs at full quality
            glowEnabled: glow !== GraphicsSettings.NoGlow && Theme.bloom
            glowQualityHigh: glow === GraphicsSettings.HighGlow
            glowBlendMode: ExtendedSceneEnvironment.Additive
            glowStrength: 1.0
            glowIntensity: glow === GraphicsSettings.HighGlow ? 1.0
                           : glow === GraphicsSettings.MediumGlow ? 0.9 : 0.8
            glowBloom: 0.0

            glowHDRMinimumValue: 0.9
            glowLevel: glow === GraphicsSettings.LowGlow
                       ? ExtendedSceneEnvironment.One | ExtendedSceneEnvironment.Two
                       : ExtendedSceneEnvironment.One | ExtendedSceneEnvironment.Two | ExtendedSceneEnvironment.Three
        }

        // Key light from above in front, gives the Phong highlights and
        // makes the extruded edges read
        DirectionalLight {
            eulerRotation: Qt.vector3d(-35, -15, 0)
            brightness: 0.9 * Theme.light
            ambientColor: Qt.rgba(0.1, 0.12, 0.2, 1.0)
        }

        Starfield {
            visible: GraphicsSettings.stars
        }

        // Flies from scene to scene, the camera on it shakes and leans
        Node {
            id: cameraRig

            PerspectiveCamera {
                id: camera

                position: phong.currentScene?.viewOffset ?? Qt.vector3d(0, 0, 0)
                eulerRotation: phong.currentScene?.viewRotation ?? Qt.vector3d(0, 0, 0)
                fieldOfView: 45
                clipNear: 0.1
                clipFar: 1000

                // A fill light travelling with the camera
                PointLight {
                    brightness: 0.45 * Theme.light
                    constantFade: 1.0
                    linearFade: 0.0
                    quadraticFade: 0.0
                }
            }
        }

        Vector3dAnimation {
            id: cameraAnimation
            target: cameraRig
            property: "position"
            duration: Theme.cameraDuration
            easing.type: Easing.InOutQuad
        }

        MenuScene {
            id: menuScene
            phong: phong
            position: Qt.vector3d(-phong.sceneSpacingX, 0, 0)
            settingsScene: settingsScene
            statsScene: statsScene
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
            graphicsScene: graphicsScene
        }

        GraphicsScene {
            id: graphicsScene
            phong: phong
            position: Qt.vector3d(0, phong.sceneSpacingY, 0)
        }

        StatsScene {
            id: statsScene
            phong: phong
            position: Qt.vector3d(-phong.sceneSpacingX, -phong.sceneSpacingY, 0)
        }

        ResultScene {
            id: resultScene
            phong: phong
            position: Qt.vector3d(0, -phong.sceneSpacingY, 0)
            match: gameScene.match
            menuScene: menuScene
            mode: gameScene.mode
            ladderStage: gameScene.ladderStage
            endlessScore: gameScene.endlessScore
            newHighScore: gameScene.newHighScore
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

    // Frame rate to keep an eye on the cost of the looks
    Text {
        visible: GraphicsSettings.showFps
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.margins: 8
        color: Theme.dimmed
        font.family: Theme.fontFamily
        font.pixelSize: 14
        text: qsTr("%1 fps  %2 ms").arg(view.renderStats.fps).arg(view.renderStats.frameTime.toFixed(1))
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

    Binding {
        target: SoundEffects
        property: "enabled"
        value: GameSettings.sound
    }

    Component.onCompleted: {
        cameraRig.position = cameraPosition(menuScene)
        nextScene(menuScene)
    }
}
