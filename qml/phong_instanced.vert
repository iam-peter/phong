// The Phong material on instances: their own transform, color and glow,
// the glow in the x of their custom data
VARYING vec3 vNormal;
VARYING vec3 vToCamera;
VARYING vec4 vTint;
VARYING float vGlow;

void MAIN()
{
    vec4 world = INSTANCE_MODEL_MATRIX * vec4(VERTEX, 1.0);
    vNormal = mat3(INSTANCE_MODEL_MATRIX) * NORMAL;
    vToCamera = CAMERA_POSITION - world.xyz;
    vTint = INSTANCE_COLOR;
    vGlow = INSTANCE_DATA.x;
    POSITION = INSTANCE_MODELVIEWPROJECTION_MATRIX * vec4(VERTEX, 1.0);
}
