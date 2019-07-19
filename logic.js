.pragma library // Shared game state
.import QtQuick 2.0 as QQ

// Game Stuff
var gameState = {};
var camera = {};
function getGameState() { return gameState; }

function nextScene(scene) {
    var nextScene = scene;

    console.log(">>> Next scene " + nextScene + " " + nextScene.menuItemName)

    if (gameState.sceneStack.length) {
        var currScene = gameState.sceneStack[gameState.sceneStack.length - 1];
        currScene.active = false;
    }

    // Transform camera
    var pos = nextScene.position;
    camera.viewCenter = pos;
    camera.position = pos.plus(Qt.vector3d(0, 0, 34));

    // Push next scene to stack
    gameState.sceneStack.push(nextScene);
    nextScene.active = true;
}

function previousScene() {
    // Pop current scene of stack
    var currScene = gameState.sceneStack.pop();
    currScene.active = false;

    // Set remaining scene as current
    if (gameState.sceneStack.length) {
        var prevScene = gameState.sceneStack[gameState.sceneStack.length - 1];
        prevScene.active = true;

        console.log(">>> Previous scene " + prevScene + " " + prevScene.menuItemName)

        // Transform camera
        var pos = prevScene.position;
        camera.viewCenter = pos;
        camera.position = pos.plus(Qt.vector3d(0, 0, 34));
    }
}

function initialise(cam)
{
    camera = cam;

    gameState.sceneStack = [];
    return gameState;
}


function transitionStarted() {
    console.log("transition started")
}

function transitionFinished() {
    console.log("transition finished")
}
