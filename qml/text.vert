// The letters of a TextBatch: every instance is a cell of the FontAtlas,
// its corner in the x and y of the custom data, its glow in z
VARYING vec2 vUV;
VARYING vec3 vNormal;
VARYING vec3 vToCamera;
VARYING vec4 vTint;
VARYING float vGlow;

void MAIN()
{
    vec4 world = INSTANCE_MODEL_MATRIX * vec4(VERTEX, 1.0);
    vNormal = mat3(INSTANCE_MODEL_MATRIX) * vec3(0.0, 0.0, 1.0);
    vToCamera = CAMERA_POSITION - world.xyz;
    vUV = INSTANCE_DATA.xy + vec2(UV0.x, 1.0 - UV0.y) * cellUV;
    vTint = INSTANCE_COLOR;
    vGlow = INSTANCE_DATA.z;
    POSITION = INSTANCE_MODELVIEWPROJECTION_MATRIX * vec4(VERTEX, 1.0);
}
