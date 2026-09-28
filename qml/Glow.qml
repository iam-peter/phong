import QtQuick
import QtQuick3D

// The glow of what is brighter than white and the fast anti-aliasing,
// the way ExtendedSceneEnvironment does them, in a few passes of a few
// uniforms each. There every pass uploads its whole set of settings, one
// WebGL call per value in the browser. The bright parts are blurred at
// half, a quarter and, for a wide glow, an eighth of the size, first
// across then down, and the last pass tonemaps the scene and adds them.
// The environment's own tonemapping has to be off.
Effect {
    id: root

    // Brightness from which a surface starts to glow
    property real threshold: 0.9
    // Strength of the glow, 0 for none
    property real intensity: 1.0
    // The third, widest level
    property real wide: 1.0
    property real fxaa: 0.0

    // Per pass
    property vector2d direction: Qt.vector2d(1, 0)
    property real first: 0.0

    // The samplers of the last pass, bound to the glow levels there
    readonly property TextureInput glow1: TextureInput {
        texture: Texture {}
    }
    readonly property TextureInput glow2: TextureInput {
        texture: Texture {}
    }
    readonly property TextureInput glow3: TextureInput {
        texture: Texture {}
    }

    component Level: Buffer {
        format: Buffer.RGBA16F
        textureFilterOperation: Buffer.Linear
        textureCoordOperation: Buffer.ClampToEdge
    }

    Level { id: halfA; name: "halfA"; sizeMultiplier: 0.5 }
    Level { id: halfB; name: "halfB"; sizeMultiplier: 0.5 }
    Level { id: quarterA; name: "quarterA"; sizeMultiplier: 0.25 }
    Level { id: quarterB; name: "quarterB"; sizeMultiplier: 0.25 }
    Level { id: eighthA; name: "eighthA"; sizeMultiplier: 0.125 }
    Level { id: eighthB; name: "eighthB"; sizeMultiplier: 0.125 }

    Shader {
        id: blur
        stage: Shader.Fragment
        shader: "glow_blur.frag"
    }

    Shader {
        id: combine
        stage: Shader.Fragment
        shader: "glow_combine.frag"
    }

    component BlurPass: Pass {
        property Buffer from
        property vector2d along
        property real isFirst: 0.0
        shaders: [blur]
        commands: [
            BufferInput { buffer: from },
            SetUniformValue { target: "direction"; value: along },
            SetUniformValue { target: "first"; value: isFirst }
        ]
    }

    // The scene across into half the size, keeping the bright parts
    Pass {
        id: firstPass
        shaders: [blur]
        commands: [
            SetUniformValue { target: "direction"; value: Qt.vector2d(1, 0) },
            SetUniformValue { target: "first"; value: 1.0 }
        ]
        output: halfA
    }
    BlurPass { id: halfDown; from: halfA; along: Qt.vector2d(0, 1); output: halfB }
    BlurPass { id: quarterAcross; from: halfB; along: Qt.vector2d(1, 0); output: quarterA }
    BlurPass { id: quarterDown; from: quarterA; along: Qt.vector2d(0, 1); output: quarterB }
    BlurPass { id: eighthAcross; from: quarterB; along: Qt.vector2d(1, 0); output: eighthA }
    BlurPass { id: eighthDown; from: eighthA; along: Qt.vector2d(0, 1); output: eighthB }

    Pass {
        id: combinePass
        shaders: [combine]
        commands: [
            BufferInput { buffer: halfB; sampler: "glow1" },
            BufferInput { buffer: quarterB; sampler: "glow2" },
            BufferInput { buffer: eighthB; sampler: "glow3" }
        ]
    }

    // Without glow only the last pass runs, for the tonemapping and the
    // anti-aliasing
    passes: intensity <= 0.0 ? [combinePass]
            : wide > 0.5 ? [firstPass, halfDown, quarterAcross, quarterDown, eighthAcross, eighthDown, combinePass]
            : [firstPass, halfDown, quarterAcross, quarterDown, combinePass]
}
