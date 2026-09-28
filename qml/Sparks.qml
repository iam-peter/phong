import QtQuick
import QtQuick3D
import QtQuick3D.Particles3D

// Bursts of small particles, for hits, goals and pickups
Node {
    id: root

    function burst(position, color, count) {
        particle.color = color
        emitter.burst(count, 0, position)
    }

    ParticleSystem3D {
        id: system

        ModelParticle3D {
            id: particle
            maxAmount: 300
            color: Theme.text
            colorVariation: Qt.vector4d(0.15, 0.15, 0.15, 0.0)
            fadeOutEffect: Particle3D.FadeScale
            fadeOutDuration: 250
            delegate: Model {
                source: "#Cube"
                scale: Qt.vector3d(0.0025, 0.0025, 0.0025)
                materials: DefaultMaterial {
                    diffuseColor: "white"
                    lighting: DefaultMaterial.NoLighting
                }
            }
        }

        ParticleEmitter3D {
            id: emitter
            particle: particle
            emitRate: 0
            lifeSpan: 450
            lifeSpanVariation: 200
            particleScaleVariation: 0.4
            particleRotationVariation: Qt.vector3d(180, 180, 180)
            particleRotationVelocityVariation: Qt.vector3d(300, 300, 300)
            velocity: VectorDirection3D {
                direction: Qt.vector3d(0, 0, 2)
                directionVariation: Qt.vector3d(10, 10, 2)
            }
        }
    }
}
