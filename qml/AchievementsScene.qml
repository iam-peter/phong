pragma ComponentBehavior: Bound

import QtQuick
import QtQuick3D
import Phong

// The challenges and which of them are done, in two columns
Scene {
    id: root

    readonly property int rowsPerColumn: Math.ceil(Stats.achievements.length / 2)

    onKeyPressed: (event) => {
        event.accepted = true
        switch (event.key) {
            case Qt.Key_Escape:
            case Qt.Key_Enter:
            case Qt.Key_Return:
            case Qt.Key_Space:
                SoundEffects.play(SoundEffects.MenuSelect)
                phong.previousScene()
                break
        }
    }
    onPointerPressed: (id, x, y) => phong.clickableAt(x, y)?.clicked()

    Text3D {
        y: 8.2
        scale: Qt.vector3d(1.8, 1.8, 1.8)
        horizontalAlignment: Text.AlignHCenter
        color: Theme.title
        glow: 0.8
        text: qsTr("Achievements")
    }

    Text3D {
        y: 6.3
        scale: Qt.vector3d(0.6, 0.6, 0.6)
        horizontalAlignment: Text.AlignHCenter
        color: Theme.dimmed
        text: qsTr("%1 of %2 done").arg(Stats.unlockedCount).arg(Stats.achievements.length)
    }

    Repeater3D {
        model: Stats.achievements

        delegate: Node {
            id: achievement

            required property var modelData
            required property int index

            readonly property bool done: modelData.unlocked

            x: index < root.rowsPerColumn ? -16.0 : 2.0
            y: 4.0 - (index % root.rowsPerColumn) * Math.min(2.35, 12.6 / Math.max(root.rowsPerColumn - 1, 1))

            Disc {
                position: Qt.vector3d(-0.9, 0.25, 0)
                radius: 0.3
                sphere: achievement.done
                thickness: 0.2
                color: achievement.done ? Theme.title : Theme.goal
                glow: achievement.done ? 0.6 : 0.0
            }

            Text3D {
                scale: Qt.vector3d(0.7, 0.7, 0.7)
                color: achievement.done ? Theme.text : Theme.dimmed
                glow: achievement.done ? 0.3 : 0.0
                text: achievement.modelData.name
            }

            Text3D {
                y: -0.9
                scale: Qt.vector3d(0.42, 0.42, 0.42)
                color: Theme.dimmed
                text: achievement.modelData.description
            }
        }
    }

    Text3D {
        y: -11
        scale: Qt.vector3d(0.5, 0.5, 0.5)
        horizontalAlignment: Text.AlignHCenter
        text: qsTr("[Esc] back")
        color: Theme.dimmed
        clickable: true
        onClicked: phong.previousScene()
    }
}
