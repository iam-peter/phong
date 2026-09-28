import QtQuick
import QtQuick3D

// The shading the game is named after: diffuse light and a specular
// highlight. Flat shading in the graphics settings just shows the color.
//
// Lit in its own shader rather than by the built-in lighting: that one
// uploads all fifteen light slots for every draw, one call per value on
// WebGL, most of the time of a frame in the browser. The lights are the
// ones of Main: the key light from above in front and the fill light at
// the camera.
CustomMaterial {
    id: root

    property color color: Theme.text
    // How much the surface lights itself, bright ones bloom
    property real glow: 0.0
    // Strength of the highlight
    property real shininess: 0.0
    // DefaultMaterial.NoLighting shows the plain color
    property int lighting: DefaultMaterial.FragmentLighting
    // On an instanced model: the color of each instance tints the color,
    // the x of its custom data scales the glow
    property bool instanced: false

    // The uniforms of the shaders
    readonly property real lit: Theme.phong && lighting !== DefaultMaterial.NoLighting ? 1.0 : 0.0
    readonly property real emission: glow * Theme.glowScale
    // Squared, dull surfaces lose their highlight, like with the built-in one
    readonly property real specular: Theme.phong ? 2.0 * shininess * shininess * Theme.shine : 0.0
    // Towards the key light of Main, turned by (-35, -15, 0)
    readonly property vector3d keyDirection: Qt.vector3d(-0.2120, 0.5736, 0.7913)
    readonly property real keyStrength: 0.9 * Theme.light
    readonly property real fillStrength: 0.45 * Theme.light
    readonly property vector3d ambient: Qt.vector3d(0.1, 0.12, 0.2)
    readonly property real highlightPower: 400.0

    shadingMode: CustomMaterial.Unshaded
    vertexShader: instanced ? "phong_instanced.vert" : "phong.vert"
    fragmentShader: "phong.frag"
    // Fading nodes need the blending, opaque ones look the same with it
    sourceBlend: CustomMaterial.SrcAlpha
    destinationBlend: CustomMaterial.OneMinusSrcAlpha
    depthDrawMode: Material.AlwaysDepthDraw
}
