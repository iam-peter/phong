import QtQuick 2.13
import Qt3D.Core 2.13
import Qt3D.Render 2.13
import Qt3D.Input 2.13
import Qt3D.Extras 2.13
import "logic.js" as Logic

Entity {
    id: stageEntity

    property real width: 40
    property real height: 30

    property real playerBaseDepth: 4

    property list<Wall> walls: [ /* Order: top, right, bottom, left */
        Wall {
            id: topWall
            position: Qt.vector3d(0, 0.5 * height, 0)
            scale: Qt.vector3d(width, 1, 1)
        },
        Wall {
            id: rightWall
            position: Qt.vector3d(0.5 * width, 0, 0)
            scale: Qt.vector3d(1, height, 1)
        },
        Wall {
            id: bottomWall
            position: Qt.vector3d(0, -0.5 * height, 0)
            scale: Qt.vector3d(width, 1, 1)
        },
        Wall {
            id: leftWall
            position: Qt.vector3d(-0.5 * width, 0, 0)
            scale: Qt.vector3d(1, height, 1)
        }
    ]


    PhongMaterial {
        id: baseMaterial
        ambient: "lightGrey"
    }
    PlaneMesh {
        id: baseMesh
    }

    Entity {
        id: leftBaseEntity

        Transform {
            id: leftBaseTransform
            scale3D: Qt.vector3d(playerBaseDepth, 1, height)
            rotation: fromAxisAndAngle(Qt.vector3d(1, 0, 0), 90)
            translation: Qt.vector3d(leftWall.position.x + playerBaseDepth * 0.5, 0, 0)
        }

        components: [baseMesh, baseMaterial, leftBaseTransform]
    }

    Entity {
        id: rightBaseEntity

        Transform {
            id: rightBaseTransform
            scale3D: Qt.vector3d(playerBaseDepth, 1, height)
            rotation: fromAxisAndAngle(Qt.vector3d(1, 0, 0), 90)
            translation: Qt.vector3d(rightWall.position.x - playerBaseDepth * 0.5, 0, 0)
        }

        components: [baseMesh, baseMaterial, rightBaseTransform]
    }

    components: [walls]
}
