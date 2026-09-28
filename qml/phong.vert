// The Phong material: the normal and the way to the camera for the light
// in the fragment shader
VARYING vec3 vNormal;
VARYING vec3 vToCamera;
VARYING vec4 vTint;
VARYING float vGlow;

void MAIN()
{
    vec4 world = MODEL_MATRIX * vec4(VERTEX, 1.0);
    vNormal = NORMAL_MATRIX * NORMAL;
    vToCamera = CAMERA_POSITION - world.xyz;
    vTint = vec4(1.0);
    vGlow = 1.0;
    POSITION = MODELVIEWPROJECTION_MATRIX * vec4(VERTEX, 1.0);
}
