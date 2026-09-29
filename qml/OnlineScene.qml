pragma ComponentBehavior: Bound

import QtQuick
import QtQuick3D
import Phong

// Games over the internet and the shared high scores: on or off, the name
// the others see and the server, the published one unless one is set
OptionsScene {
    id: root

    title: qsTr("Online")

    rows: [
        {
            label: qsTr("Internet play"),
            values: [true, false],
            names: [qsTr("On"), qsTr("Off")],
            get: () => GameSettings.online,
            set: (value) => GameSettings.online = value
        },
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
            label: qsTr("Own server"),
            values: [],
            text: true,
            maxLength: 60,
            placeholder: qsTr("none"),
            get: () => GameSettings.serverUrl,
            set: (url) => GameSettings.serverUrl = url
        },
        {
            label: qsTr("Published server"),
            values: [],
            hint: OnlineService.busy ? qsTr("asking...") : OnlineService.publishedServer || qsTr("none"),
            set: () => {
                GameSettings.serverUrl = ""
                OnlineService.refresh()
            }
        }
    ]

    Text3D {
        y: -8.2
        scale: Qt.vector3d(0.5, 0.5, 0.5)
        horizontalAlignment: Text.AlignHCenter
        color: OnlineService.available ? Theme.title : Theme.dimmed
        text: !GameSettings.online ? qsTr("Internet play is off")
              : OnlineService.available ? qsTr("Playing online with %1").arg(OnlineService.server)
              : qsTr("No server for internet play yet")
    }

    Text3D {
        y: -9.0
        scale: Qt.vector3d(0.5, 0.5, 0.5)
        horizontalAlignment: Text.AlignHCenter
        color: Theme.dimmed
        text: qsTr("Without an own server the published one is used, Enter on it goes back to it")
    }

    Text3D {
        y: -9.8
        scale: Qt.vector3d(0.5, 0.5, 0.5)
        horizontalAlignment: Text.AlignHCenter
        color: Theme.dimmed
        text: qsTr("Without a name the others see \"Player\", the high scores need one")
    }
}
