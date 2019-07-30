.pragma library // Shared game state
.import QtQuick 2.0 as QQ

// Game Stuff
var m_root = null;
var m_gameState = {};
var m_camera = {};
function getGameState() { return m_gameState; }

function initialise(root, camera)
{
    m_root = root;
    m_camera = camera;
    m_gameState.sceneStack = [];
    return m_gameState;
}

function getLastSceneOnStack() {
    return (m_gameState.sceneStack.length) ? m_gameState.sceneStack[m_gameState.sceneStack.length - 1] : null;
}

function nextScene(scene) {
    var nextScene = scene;
    var currScene = getLastSceneOnStack();

    console.log(">>> Next scene " + nextScene + " " + nextScene.title)

    if (currScene !== null) {
        currScene.active = false;
    }

    // Transform camera
    transformCamera(nextScene.position);

    // Push next scene to stack
    m_gameState.sceneStack.push(nextScene);
    nextScene.active = true;
}

function previousScene() {
    // Pop current scene of stack
    var currScene = m_gameState.sceneStack.pop();
    currScene.active = false;
    var prevScene = getLastSceneOnStack();

    // Set remaining scene as current
    if (prevScene !== null) {
        prevScene.active = true;

        console.log(">>> Previous scene " + prevScene + " " + prevScene.title)

        m_root.cameraAnimation.started.connect(function() {
            prevScene.onTransitionStarted()
        })
        m_root.cameraAnimation.finished.connect(function() {
            prevScene.onTransitionFinished()
        })

        // Transform camera
        transformCamera(prevScene.position)
    }
}

function transformCamera(position) {

    if (m_root.cameraAnimation.running)
        m_root.cameraAnimation.complete()

    m_root.cameraViewCenterAnimation.to = position;
    m_root.cameraPositionAnimation.to = position.plus(Qt.vector3d(0, 0, 34));
    m_root.cameraAnimation.start();
}


function transitionStarted() {
    console.log("transition started")
}

function transitionFinished() {
    console.log("transition finished")
}
