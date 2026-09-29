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

    // What Esc does, for the button of the fingers
    function pressEscape() {
        view.forceActiveFocus()
        currentScene?.keyPressed({ key: Qt.Key_Escape, text: "", modifiers: Qt.NoModifier,
                                   isAutoRepeat: false, accepted: false })
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

    // Who plays which side before a game of several players. kind is
    // "local" for the players at this machine, "online" to host for others.
    function openLobby(kind) {
        lobbyScene.open(kind)
        nextScene(lobbyScene)
    }

    // Back from a game to where it was chosen: the local lobby with its
    // players, the solo list, or hosting and joining, the network is closed
    // by then
    function returnToMenu() {
        if (sceneStack.includes(lobbyScene) && lobbyScene.network === "local" && !lobbyScene.remote) {
            returnTo(lobbyScene)
            return
        }
        for (const scene of [soloScene, playOnlineScene]) {
            if (sceneStack.includes(scene)) {
                returnTo(scene)
                return
            }
        }
        returnTo(menuScene)
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

    // With a room code over the internet or on the LAN, the way last
    // chosen if there is a server for it
    function openJoin() {
        joinScene.kind = GameSettings.network === "internet" && OnlineService.available ? "internet" : "lan"
        // Back here from a lobby the reason stays
        joinScene.error = ""
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
            soloScene: soloScene
            playOnlineScene: playOnlineScene
        }

        // Alone against the computer, or a wall. The last one played is
        // remembered.
        ChoiceScene {
            id: soloScene
            phong: phong
            position: Qt.vector3d(-2 * phong.sceneSpacingX, 0, 0)
            title: qsTr("Solo")
            currentItem: Math.min(Math.max(GameSettings.mode, 0), 5)

            function play(index, mode) {
                GameSettings.mode = index
                phong.startGame(mode)
            }

            entries: [
                { text: qsTr("Versus Computer"), detail: qsTr("One match, the computer's level is in the settings"),
                  activate: () => soloScene.play(0, GameScene.OnePlayer) },
                { text: qsTr("Ladder"), detail: qsTr("Easy, Normal and Hard in a row, a loss can be retried"),
                  activate: () => soloScene.play(1, GameScene.Ladder) },
                { text: qsTr("Tournament"), detail: qsTr("A knockout bracket of eight with their own ways to play"),
                  activate: () => soloScene.play(2, GameScene.Tournament) },
                { text: qsTr("Endless"), detail: qsTr("The computer gets faster the longer you last, three balls"),
                  activate: () => soloScene.play(3, GameScene.Endless) },
                { text: qsTr("Bricks"), detail: qsTr("A wall of bricks between you and the computer"),
                  activate: () => soloScene.play(4, GameScene.Bricks) },
                { text: qsTr("Squash"), detail: qsTr("Alone against a wall, the longest rally counts"),
                  activate: () => soloScene.play(5, GameScene.Squash) }
            ]
        }

        // Over the network: host a lobby for the others, or join one. How,
        // over the internet or on the LAN, is chosen there.
        ChoiceScene {
            id: playOnlineScene
            phong: phong
            position: Qt.vector3d(2 * phong.sceneSpacingX, phong.sceneSpacingY, 0)
            title: qsTr("Online")

            onActiveChanged: {
                // The server may have moved, and wakes up meanwhile
                if (active) {
                    OnlineService.refresh()
                    OnlineService.check()
                }
            }

            status: !OnlineService.available ? (GameSettings.online ? qsTr("No server for internet play, only the LAN, see Settings, Online")
                                                                    : qsTr("Internet play is off, only the LAN, see Settings, Online"))
                    : OnlineService.status
            statusColor: OnlineService.state === OnlineService.Unreachable ? Theme.accent : Theme.dimmed

            entries: [
                // Browsers can't take connections on the LAN
                { text: qsTr("Host a game"), enabled: OnlineService.available || Lan.canHost,
                  detail: qsTr("Choose the players, others join over the internet or the LAN"),
                  activate: () => phong.openLobby("online") },
                { text: qsTr("Join a game"),
                  detail: qsTr("With the code of a room, or a game on the LAN"),
                  activate: () => phong.openJoin() }
            ]
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
            phong.wakeButtons()
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

        // Only fingers bring up the buttons for them, the mouse has keys
        PointHandler {
            acceptedDevices: PointerDevice.TouchScreen
            onActiveChanged: {
                if (active)
                    phong.touched = true
            }
        }
    }

    // Fingers have no Esc: on a phone, or once the screen was touched,
    // buttons at the top do what Esc does, pause the game or go back, and
    // switch to full screen. In a game they sit in the middle, the sides
    // steer the paddles, in the menus in the corner, clear of the titles.
    property bool touched: false

    // During a game the buttons only show for a moment after a touch,
    // they would be in the way of the play otherwise
    property bool buttonsAwake: false
    function wakeButtons() {
        buttonsAwake = true
        buttonsAsleep.restart()
    }
    Timer {
        id: buttonsAsleep
        interval: 3000
        onTriggered: phong.buttonsAwake = false
    }

    component CornerButton: Rectangle {
        id: button

        signal tapped()

        // The smallest a finger hits well
        width: 44
        height: 44
        radius: 9
        color: Qt.rgba(Theme.background.r, Theme.background.g, Theme.background.b, 0.6)
        border.color: Theme.dimmed
        border.width: 2

        TapHandler {
            // Takes the finger at once, the scene below doesn't get it.
            // Tapped fires on release, when browsers allow full screen.
            gesturePolicy: TapHandler.WithinBounds
            onTapped: button.tapped()
        }
    }

    // Phones play sideways, upright a note covers the game and takes the
    // fingers. The game pauses when the phone turns upright.
    readonly property bool upright: GraphicsSettings.touchScreen && height > width
    onUprightChanged: {
        if (upright && currentScene?.running === true)
            pressEscape()
    }

    Rectangle {
        anchors.fill: parent
        visible: phong.upright
        color: Qt.rgba(Theme.background.r, Theme.background.g, Theme.background.b, 0.92)

        MultiPointTouchArea {
            anchors.fill: parent
            mouseEnabled: true
        }

        Column {
            anchors.centerIn: parent
            width: parent.width - 40
            spacing: 24

            // A phone turning on its side
            Rectangle {
                anchors.horizontalCenter: parent.horizontalCenter
                width: 36
                height: 60
                radius: 6
                color: "transparent"
                border.color: Theme.title
                border.width: 3

                SequentialAnimation on rotation {
                    running: phong.upright
                    loops: Animation.Infinite
                    PauseAnimation { duration: 500 }
                    NumberAnimation { from: 0; to: -90; duration: 700; easing.type: Easing.InOutQuad }
                    PauseAnimation { duration: 900 }
                    NumberAnimation { from: -90; to: 0; duration: 300 }
                }
            }

            Text {
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                color: Theme.text
                font.family: Theme.fontFamily
                font.capitalization: Font.AllUppercase
                font.pixelSize: 20
                text: qsTr("Turn your phone sideways")
            }
        }
    }

    Row {
        // The game scenes are the ones that run
        readonly property bool inGame: phong.currentScene?.running !== undefined

        // Paused and after the game they stay
        readonly property bool shown: !inGame || phong.currentScene.running !== true || phong.buttonsAwake

        x: inGame ? 0.5 * (parent.width - width) : parent.width - width - 8
        y: 8
        spacing: 10
        visible: phong.touched || GraphicsSettings.touchScreen
        // Faded out they let the touches through to the game
        opacity: shown ? 1.0 : 0.0
        enabled: shown

        Behavior on opacity {
            NumberAnimation { duration: 300 }
        }

        Behavior on x {
            NumberAnimation { duration: 250; easing.type: Easing.OutCubic }
        }

        CornerButton {
            visible: GraphicsSettings.fullScreenAvailable
            onTapped: GraphicsSettings.fullScreen = !GraphicsSettings.fullScreen

            // Four corners, pointing out to go full screen, in to leave it
            Item {
                id: frame
                anchors.centerIn: parent
                width: 20
                height: 20

                Repeater {
                    model: 4

                    delegate: Item {
                        required property int index
                        readonly property bool atRight: index % 2 === 1
                        readonly property bool atBottom: index >= 2
                        readonly property bool outwards: !GraphicsSettings.fullScreen

                        x: atRight ? frame.width - width : 0
                        y: atBottom ? frame.height - height : 0
                        width: 8
                        height: 8

                        Rectangle {
                            y: parent.atBottom === parent.outwards ? 5 : 0
                            width: 8
                            height: 3
                            color: Theme.text
                        }
                        Rectangle {
                            x: parent.atRight === parent.outwards ? 5 : 0
                            width: 3
                            height: 8
                            color: Theme.text
                        }
                    }
                }
            }
        }

        CornerButton {
            // The menu at the bottom of the stack has nothing to go back to
            visible: phong.sceneStack.length > 1
            onTapped: phong.pressEscape()

            // Pause while a game runs, back everywhere else
            readonly property bool pauses: phong.currentScene?.running === true

            Row {
                visible: parent.pauses
                anchors.centerIn: parent
                spacing: 5

                Repeater {
                    model: 2
                    delegate: Rectangle {
                        width: 5
                        height: 18
                        color: Theme.text
                    }
                }
            }

            Text {
                visible: !parent.pauses
                anchors.centerIn: parent
                color: Theme.text
                font.family: Theme.fontFamily
                font.pixelSize: 24
                font.bold: true
                text: "<"
            }
        }
    }

    // On the desktop the window follows the setting, and the setting the
    // window when the window manager leaves full screen. The browser page
    // goes full screen itself, see GraphicsSettings.
    Binding {
        when: Qt.platform.os !== "wasm"
        target: phong
        property: "visibility"
        value: GraphicsSettings.fullScreen ? Window.FullScreen : Window.Windowed
    }
    onVisibilityChanged: {
        if (Qt.platform.os !== "wasm" && visibility !== Window.Hidden && visibility !== Window.Minimized)
            GraphicsSettings.fullScreen = visibility === Window.FullScreen
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
