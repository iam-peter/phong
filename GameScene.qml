import QtQuick 2.13
import Qt3D.Core 2.13
import Qt3D.Render 2.13
import Qt3D.Input 2.13
import Qt3D.Extras 2.13
import "logic.js" as Logic

Scene {
    id: scene

    property Ball ball: Ball {
    }
    property list<Player> players: [
        Player { /* left */
            name: "Joe"
            color: "red"
            racket.position: Qt.vector3d((-0.5 * stageEntity.width) + stageEntity.baseDepth, 0, 0)
            racket.scale: Qt.vector3d(1, 4, 1)
        },
        Player { /* right */
            name: "Peter"
            color: "green"
            racket.position: Qt.vector3d((0.5 * stageEntity.width) - stageEntity.baseDepth, 0, 0)
            racket.scale: Qt.vector3d(1, 4, 1)
        }
    ]

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

    Entity {
        id: scoreEntity

        ExtrudedTextWrapper {
            id: scoreMesh
            text: players[0].score + " : " + players[1].score
            depth: 1.0
            horizontalAlignment: Text.AlignHCenter
        }
    }

    Stage {
        id: stageEntity
        width: 30
        height: 20
    }

    onPressed: function (event) {
        console.log("Game | KeyboardHandler | onPressed")
    }
}
