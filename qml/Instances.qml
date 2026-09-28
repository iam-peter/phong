import QtQuick
import QtQuick3D

// Many copies of one mesh in a single draw call: a row of balls left, the
// segments of a power bar. Every item is { x, y, z, color, glow, scale },
// all but x and y optional, the alpha of the color fades it. Items may
// also stretch the mesh by sx, sy and sz instead and turn it by angle
// degrees around z. The mesh is scaled and turned by meshScale and
// meshRotation before it goes to the place of an item.
Model {
    id: root

    property var items: []
    property vector3d meshScale: Qt.vector3d(0.01, 0.01, 0.01)
    property vector3d meshRotation: Qt.vector3d(0, 0, 0)
    property real shininess: 0.6
    // Scales the glow of all items, e.g. to pulse
    property real glow: 1.0
    property int lighting: DefaultMaterial.FragmentLighting

    // Without instances Qt draws the mesh once as it is
    visible: items.length > 0
    scale: meshScale
    eulerRotation: meshRotation
    instancing: InstanceList {
        id: list
    }

    // The same entries while the number of items stays, e.g. for a
    // trail that moves every frame, otherwise new ones
    function rebuild() {
        const count = list.instances.length
        if (count === items.length) {
            for (let i = 0; i < count; ++i)
                apply(list.instances[i], items[i])
            return
        }

        // A copy, the list property is a live view of the entries
        const old = []
        for (let i = 0; i < count; ++i)
            old.push(list.instances[i])
        const entries = []
        for (const item of items) {
            const created = entry.createObject(list)
            apply(created, item)
            entries.push(created)
        }
        list.instances = entries
        for (const gone of old)
            gone.destroy()
    }

    function apply(target, item) {
        target.position = Qt.vector3d(item.x, item.y, item.z ?? 0)
        const scale = item.scale ?? 1
        target.scale = Qt.vector3d(item.sx ?? scale, item.sy ?? scale, item.sz ?? scale)
        target.eulerRotation = Qt.vector3d(0, 0, item.angle ?? 0)
        target.color = item.color ?? "white"
        target.customData = Qt.vector4d(item.glow ?? 0, 0, 0, 0)
    }

    onItemsChanged: rebuild()
    Component.onCompleted: rebuild()

    materials: PhongMaterial {
        instanced: true
        color: "white"
        glow: root.glow
        shininess: root.shininess
        lighting: root.lighting
    }

    Component {
        id: entry
        InstanceListEntry {}
    }
}
