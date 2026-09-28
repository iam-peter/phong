import QtQuick
import QtQuick3D

// The shading the game is named after: diffuse light and a specular
// highlight. Flat shading in the graphics settings just shows the color.
DefaultMaterial {
    id: root

    property color color: Theme.text
    // How much the surface lights itself, bright ones bloom
    property real glow: 0.0
    // Strength of the highlight
    property real shininess: 0.0

    lighting: Theme.phong ? DefaultMaterial.FragmentLighting : DefaultMaterial.NoLighting
    diffuseColor: color
    specularAmount: Theme.phong ? shininess : 0.0
    specularRoughness: 0.15
    emissiveFactor: Qt.vector3d(color.r, color.g, color.b).times(glow * Theme.glowScale)
}
