import QtQuick
import QtQuick3D
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
    // The scene the camera is flying away from, shown until it arrived.
    // The others are hidden, instanced models aren't culled.
    property Scene leavingScene: null

    // Scene layout follows the window: the camera backs off until the widest
    // scene fits, and the scenes are spread so that only the current one is
    // in view. The game scene stays at the origin, the physics world with it.
    readonly property real contentHalfWidth: 18.5
    readonly property real contentHalfHeight: 13.0
    readonly property real tanHalfFieldOfView: Math.tan(0.5 * camera.fieldOfView * Math.PI / 180)
    readonly property real aspectRatio: Math.max(width, 1) / Math.max(height, 1)
    // The camera distance for content reaching halfWidth and halfHeight
    // from the middle of a scene
    function distanceFor(halfWidth, halfHeight) {
        return Math.max(Theme.minimumCameraDistance, halfWidth / (tanHalfFieldOfView * aspectRatio),
                        halfHeight / tanHalfFieldOfView)
    }
    function sceneDistance(scene) {
        return distanceFor(scene?.contentHalfWidth ?? contentHalfWidth, scene?.contentHalfHeight ?? contentHalfHeight)
    }
    // The widest content any scene has, its distance keeps the others out
    // of view
    readonly property real widestHalfWidth: 23.0
    readonly property real cameraDistance: distanceFor(widestHalfWidth, contentHalfHeight)
    readonly property real sceneSpacingX: Math.max(40, cameraDistance * tanHalfFieldOfView * aspectRatio
                                                       + widestHalfWidth + 1.0)
    readonly property real sceneSpacingY: Math.max(40, cameraDistance * tanHalfFieldOfView
                                                       + contentHalfHeight + 1.0)

    function nextScene(scene) {
        leavingScene = currentScene
        if (currentScene)
            currentScene.active = false

        sceneStack = sceneStack.concat([scene])
        transformCamera(scene)
        scene.active = true
    }

    function previousScene() {
        if (sceneStack.length < 2)
            return

        leavingScene = currentScene
        currentScene.active = false
        sceneStack = sceneStack.slice(0, -1)
        transformCamera(currentScene)
        currentScene.active = true
    }

    // Pops scenes until scene is the current one
    function returnTo(scene) {
        const index = sceneStack.indexOf(scene)
        if (index < 0 || scene === currentScene)
            return

        leavingScene = currentScene
        currentScene.active = false
        sceneStack = sceneStack.slice(0, index + 1)
        transformCamera(currentScene)
        currentScene.active = true
    }

    function cameraPosition(scene) {
        return scene.position.plus(Qt.vector3d(0, 0, sceneDistance(scene)))
    }

    function transformCamera(scene) {
        cameraAnimation.stop()
        cameraAnimation.from = cameraRig.position
        cameraAnimation.to = cameraPosition(scene)
        cameraAnimation.start()
    }

    // A scene whose content changes size, e.g. with the layout
    Connections {
        target: phong.currentScene
        function onContentHalfWidthChanged() { Qt.callLater(phong.snapCamera) }
        function onContentHalfHeightChanged() { Qt.callLater(phong.snapCamera) }
    }

    // The scenes moved with the window size, catch up with the current one
    function snapCamera() {
        if (!currentScene)
            return

        cameraAnimation.stop()
        cameraRig.position = cameraPosition(currentScene)
        leavingScene = null
    }

    onWidthChanged: Qt.callLater(snapCamera)
    onHeightChanged: Qt.callLater(snapCamera)

    function startGame(mode) {
        if (mode !== GameScene.TwoPlayers)
            gameScene.remotes = [null, null]
        gameScene.mode = mode
        gameScene.ladderStage = 0
        if (mode === GameScene.Tournament) {
            Tournament.start()
            nextScene(bracketScene)
            return
        }
        nextScene(gameScene)
        gameScene.startMatch()
    }

    // Who plays which side before a game of several players
    function openLobby(party) {
        lobbyScene.open(party)
        nextScene(lobbyScene)
    }

    // Three to six players on a polygon, controllers for every side
    // watching: ids of players on the network who watch
    function startParty(players, controllers, watching) {
        partyScene.players = players
        partyScene.controllers = controllers ?? [{ kind: "keyboard" }]
        partyScene.spectators = watching ?? []
        nextScene(partyScene)
        partyScene.start()
    }

    // The classic field, slots from the lobby: keyboard, gamepads, or a
    // player on the network
    function startTwoPlayers(left, right, watching) {
        gameScene.spectators = watching ?? []
        gameScene.leftPadAssigned = left?.kind === "pad" ? left.pad : null
        gameScene.rightPadAssigned = right?.kind === "pad" ? right.pad : null
        gameScene.remotes = [left, right].map((slot) => slot?.kind === "remote"
                                              ? { id: slot.id, name: slot.name, token: slot.token ?? "" } : null)
        startGame(GameScene.TwoPlayers)
    }

    function openJoin() {
        nextScene(joinScene)
    }

    // The machine joined a host: its lobby, its games and their states
    Connections {
        target: Lan
        function onJoined() {
            lobbyScene.remote = true
            lobbyScene.remoteSlots = []
            phong.nextScene(lobbyScene)
        }
        function onReceivedFromHost(message) {
            switch (message.t) {
                case "lobby":
                    lobbyScene.showRemote(message)
                    break
                case "practice":
                    lobbyScene.applyPractice(message)
                    break
                case "full":
                    joinScene.error = qsTr("The game is full")
                    break
                case "start":
                    if (message.party) {
                        partyScene.remote = true
                        partyScene.startRemote(message)
                        if (phong.currentScene !== partyScene)
                            phong.nextScene(partyScene)
                    }
                    else {
                        // A rematch comes from the results
                        if (phong.sceneStack.includes(gameScene))
                            phong.returnTo(gameScene)
                        else
                            phong.nextScene(gameScene)
                        gameScene.startRemote(message)
                    }
                    break
                case "state":
                    if (partyScene.remote)
                        partyScene.applyRemote(message)
                    else if (gameScene.remote)
                        gameScene.applyRemote(message)
                    break
            }
        }
        function onLeft(reason) {
            joinScene.error = reason
            lobbyScene.remote = false
            partyScene.remote = false
            gameScene.remote = false
            phong.returnTo(phong.sceneStack.includes(joinScene) ? joinScene : menuScene)
        }
    }

    // The player's next match of the tournament, from the bracket
    function playTournamentMatch() {
        nextScene(gameScene)
        gameScene.startMatch()
    }

    function showBracket() {
        returnTo(bracketScene)
    }

    // A new draw after being knocked out
    function restartTournament() {
        Tournament.start()
        returnTo(bracketScene)
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
        environment: SceneEnvironment {
            readonly property int glow: GraphicsSettings.glow
            readonly property int antialiasing: GraphicsSettings.antialiasing

            clearColor: Theme.background
            backgroundMode: SceneEnvironment.Color

            antialiasingMode: antialiasing >= GraphicsSettings.Multisample2x ? SceneEnvironment.MSAA
                                                                             : SceneEnvironment.NoAA
            antialiasingQuality: antialiasing === GraphicsSettings.Multisample4x ? SceneEnvironment.High
                                                                                : SceneEnvironment.Medium
            // The glow tonemaps, it always runs
            tonemapMode: SceneEnvironment.TonemapModeNone
            effects: [glowEffect]
        }

        // Higher glow spreads wider
        Glow {
            id: glowEffect
            readonly property bool on: GraphicsSettings.glow !== GraphicsSettings.NoGlow && Theme.bloom
            intensity: !on ? 0.0 : GraphicsSettings.glow === GraphicsSettings.HighGlow ? 1.0
                       : GraphicsSettings.glow === GraphicsSettings.MediumGlow ? 0.9 : 0.8
            wide: GraphicsSettings.glow === GraphicsSettings.LowGlow ? 0.0 : 1.0
            fxaa: GraphicsSettings.antialiasing === GraphicsSettings.FastAntialiasing ? 1.0 : 0.0
        }

        // The letters of all texts, see Text3D
        Texture {
            id: fontAtlasTexture
            textureData: FontAtlasTexture {}
            minFilter: Texture.Linear
            magFilter: Texture.Linear
            mipFilter: Texture.None
            tilingModeHorizontal: Texture.ClampToEdge
            tilingModeVertical: Texture.ClampToEdge
            Component.onCompleted: FontAtlas.texture = fontAtlasTexture
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
                // All scenes are in the world, only the one in view is drawn
                frustumCullingEnabled: true

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
            onFinished: phong.leavingScene = null
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
            menuScene: menuScene
            position: Qt.vector3d(0, 0, 0)
        }

        SettingsScene {
            id: settingsScene
            phong: phong
            position: Qt.vector3d(-phong.sceneSpacingX, phong.sceneSpacingY, 0)
            graphicsScene: graphicsScene
            controlsScene: controlsScene
            onlineScene: onlineScene
        }

        OnlineScene {
            id: onlineScene
            phong: phong
            position: Qt.vector3d(-phong.sceneSpacingX, 2 * phong.sceneSpacingY, 0)
        }

        ControlsScene {
            id: controlsScene
            phong: phong
            position: Qt.vector3d(-2 * phong.sceneSpacingX, phong.sceneSpacingY, 0)
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
            achievementsScene: achievementsScene
        }

        AchievementsScene {
            id: achievementsScene
            phong: phong
            position: Qt.vector3d(-2 * phong.sceneSpacingX, -phong.sceneSpacingY, 0)
        }

        JoinScene {
            id: joinScene
            phong: phong
            position: Qt.vector3d(2 * phong.sceneSpacingX, -phong.sceneSpacingY, 0)
            menuScene: menuScene
        }

        LobbyScene {
            id: lobbyScene
            phong: phong
            position: Qt.vector3d(2 * phong.sceneSpacingX, 0, 0)
            menuScene: menuScene
        }

        PartyScene {
            id: partyScene
            phong: phong
            position: Qt.vector3d(phong.sceneSpacingX, -phong.sceneSpacingY, 0)
            menuScene: menuScene
        }

        BracketScene {
            id: bracketScene
            phong: phong
            position: Qt.vector3d(phong.sceneSpacingX, 0, 0)
            menuScene: menuScene
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
            achievements: gameScene.newAchievements
            remote: gameScene.remote
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

    Connections {
        target: Gamepads
        function onNavigated(pad, key) {
            phong.currentScene?.gamepadNavigated(pad, key)
        }
    }

    Binding {
        target: SoundEffects
        property: "enabled"
        value: GameSettings.sound
    }

    Binding {
        target: SoundEffects
        property: "musicEnabled"
        value: GameSettings.music
    }

    Binding {
        target: SoundEffects
        property: "musicVolume"
        value: GameSettings.musicVolume / 100
    }

    Binding {
        target: Gamepads
        property: "rumbleEnabled"
        value: GameSettings.rumble
    }

    Binding {
        target: Lan
        property: "playerName"
        value: GameSettings.playerName
    }

    Binding {
        target: FontAtlas
        property: "family"
        value: Theme.fontFamily
    }

    Binding {
        target: OnlineService
        property: "enabled"
        value: GameSettings.online
    }

    Binding {
        target: OnlineService
        property: "customServer"
        value: GameSettings.serverUrl
    }

    Binding {
        target: HighScores
        property: "serverUrl"
        value: OnlineService.server
    }

    Component.onCompleted: {
        cameraRig.position = cameraPosition(menuScene)
        nextScene(menuScene)
    }
}
