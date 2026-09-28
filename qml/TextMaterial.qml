import QtQuick
import QtQuick3D
import Phong

// The letters of a TextBatch, from the FontAtlas, lit like PhongMaterial
CustomMaterial {
    // The one Texture of the FontAtlas, made in Main
    readonly property TextureInput atlas: TextureInput {
        texture: FontAtlas.texture
    }
    readonly property real cellUV: FontAtlas.cellUV
    readonly property real lit: Theme.phong ? 1.0 : 0.0
    readonly property real glowScale: Theme.glowScale
    readonly property vector3d keyDirection: Qt.vector3d(-0.2120, 0.5736, 0.7913)
    readonly property real keyStrength: 0.9 * Theme.light
    readonly property real fillStrength: 0.45 * Theme.light
    readonly property vector3d ambient: Qt.vector3d(0.1, 0.12, 0.2)

    shadingMode: CustomMaterial.Unshaded
    vertexShader: "text.vert"
    fragmentShader: "text.frag"
    sourceBlend: CustomMaterial.SrcAlpha
    destinationBlend: CustomMaterial.OneMinusSrcAlpha
    depthDrawMode: Material.AlwaysDepthDraw
    cullMode: Material.NoCulling
}
