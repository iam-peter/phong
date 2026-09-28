import QtQuick
import QtQuick3D
import Phong

// The texts under its parent in one draw call, see Text3D and TextBatch
Model {
    id: layer

    readonly property TextBatch batch: batch

    source: "#Rectangle"
    // After the other transparent objects of the scene, the edges of the
    // letters blend over them
    depthBias: -100
    instancing: TextBatch {
        id: batch
        origin: layer.parent
    }
    materials: TextMaterial {}
}
