import QtQuick 2.13
import Qt3D.Core 2.13

Entity {
    id: playerEntity

    property string name: "PlayerUnknown #n"
    property int score: 0
    property color color: "purple"


    property Racket racket: Racket{
        ambientColor: playerEntity.color
    }

    // Keyboard layout
    property int up: Qt.Key_Up
    property int down: Qt.Key_Down
    property int back: Qt.Key_B
    property int accept: Qt.Key_A
    property int start: Qt.Key_S
}
