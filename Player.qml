import QtQuick 2.0

Item {
    property string name: "PlayerUnknown #n"
    property int score: 0

    property Racket racket

    // Keyboard layout
    property int up: Qt.Key_Up
    property int down: Qt.Key_Down
    property int back: Qt.Key_B
    property int accept: Qt.Key_A
    property int start: Qt.Key_S
}
