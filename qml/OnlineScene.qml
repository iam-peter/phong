pragma ComponentBehavior: Bound

import QtQuick
import QtQuick3D
import Phong

// Games over the internet and the shared high scores: the name the
// others see and the server
OptionsScene {
    id: root

    title: qsTr("Online")

    rows: [
        {
            label: qsTr("Name"),
            values: [],
            text: true,
            maxLength: 12,
            placeholder: qsTr("none"),
            get: () => GameSettings.playerName,
            set: (name) => GameSettings.playerName = name
        },
        {
            label: qsTr("Server"),
            values: [],
            text: true,
            maxLength: 60,
            placeholder: qsTr("none"),
            get: () => GameSettings.serverUrl,
            set: (url) => GameSettings.serverUrl = url
        },
        {
            label: qsTr("Default server"),
            values: [],
            set: () => GameSettings.serverUrl = GameSettings.defaultServer
        }
    ]

    Text3D {
        y: -8.6
        scale: Qt.vector3d(0.5, 0.5, 0.5)
        horizontalAlignment: Text.AlignHCenter
        color: Theme.dimmed
        text: qsTr("The server runs phong-server, e.g. wss://phong.example.com or 192.168.1.5:45460")
    }

    Text3D {
        y: -9.4
        scale: Qt.vector3d(0.5, 0.5, 0.5)
        horizontalAlignment: Text.AlignHCenter
        color: Theme.dimmed
        text: qsTr("Without a name the others see \"Player\", the high scores need one")
    }

    Text3D {
        y: -10.2
        scale: Qt.vector3d(0.5, 0.5, 0.5)
        horizontalAlignment: Text.AlignHCenter
        color: Theme.dimmed
        text: qsTr("[Enter] edit and confirm   [Backspace] delete   [Esc] cancel")
    }
}
