.pragma library // Shared game state
.import QtQuick 2.0 as QQ

// Game Stuff
var gameState = {};
function getGameState() { return gameState; }

function nextScene(scene) {
    console.log("next scene " + scene + " " + scene.menuItemName)

    var nextScene = scene;

    if (gameState.sceneStack.length) {
        var currScene = gameState.sceneStack[gameState.sceneStack.length - 1];
        currScene.active = false;
    }

    var pos = nextScene.position;

    nextScene.camera.viewCenter = pos;
    nextScene.camera.position = pos.plus(Qt.vector3d(0, 0, 34));

    gameState.sceneStack.push(nextScene);
    nextScene.active = true;
}


function previousScene() {
    var currScene = gameState.sceneStack.pop();
    currScene.active = false;

    if (gameState.sceneStack.length) {
        var prevScene = gameState.sceneStack[gameState.sceneStack.length - 1];
        prevScene.active = true;
    }
}

function newGameState()
{
    gameState.sceneStack = [];
    return gameState;
}


function transitionStarted() {
    console.log("transition started")
}

function transitionFinished() {
    console.log("transition finished")
}
