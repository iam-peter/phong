import QtQuick 2.13
import Qt3D.Core 2.13
import Qt3D.Render 2.13
import Qt3D.Input 2.13
import Qt3D.Extras 2.13
import "logic.js" as Logic

Scene {
    id: scene

    property list<Player> players
    property Ball ball

    onActiveChanged: console.log("GameScene active: " + active)
    /*
    property int countdown: 10

    Timer {
        id: countdownTimer
        interval: 1000
        running: root.countdown < 5
        repeat: true
        onTriggered: root.countdown++
    }
*/

    /* Score */
    Entity {
        id: scoreEntity

        ExtrudedTextWrapper {
            id: scoreMesh
            text: players[0].score + " : " + players[1].score
            depth: 1.0
            horizontalAlignment: Text.AlignHCenter
        }
    }
    /* Score */

    /* Stage */
    Stage {
        id: stageEntity
        width: 30
        height: 20
    }
    /* Stage */

    onPressed: function (event) {
        console.log("Game | KeyboardHandler | onPressed")
    }
}
