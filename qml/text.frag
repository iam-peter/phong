// The letters of a TextBatch: the edge where the distance field is 0.5,
// smoothed over about a pixel on screen, lit like PhongMaterial
VARYING vec2 vUV;
VARYING vec3 vNormal;
VARYING vec3 vToCamera;
VARYING vec4 vTint;
VARYING float vGlow;

void MAIN()
{
    float d = texture(atlas, vUV).r;
    float w = max(0.75 * fwidth(d), 0.001);
    float alpha = smoothstep(0.5 - w, 0.5 + w, d) * vTint.a * qt_material_properties.a;
    if (alpha < 0.02)
        discard;

    vec3 base = vTint.rgb;
    vec3 result = base;
    if (lit > 0.5) {
        vec3 n = normalize(vNormal);
        vec3 v = normalize(vToCamera);
        result = base * (ambient + keyStrength * max(dot(n, keyDirection), 0.0) + fillStrength * max(dot(n, v), 0.0));
    }
    FRAGCOLOR = vec4(result + base * vGlow * glowScale, alpha);
}
