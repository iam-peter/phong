// Glow: a seven-tap blur along direction, as in ExtendedSceneEnvironment.
// The first pass, on the scene, only keeps what is brighter than white,
// fading in from threshold on.
void MAIN()
{
    vec2 s = direction / INPUT_SIZE;
    vec3 c = texture(INPUT, INPUT_UV).rgb * 0.174938;
    c += (texture(INPUT, INPUT_UV + s).rgb + texture(INPUT, INPUT_UV - s).rgb) * 0.165569;
    c += (texture(INPUT, INPUT_UV + 2.0 * s).rgb + texture(INPUT, INPUT_UV - 2.0 * s).rgb) * 0.140367;
    c += (texture(INPUT, INPUT_UV + 3.0 * s).rgb + texture(INPUT, INPUT_UV - 3.0 * s).rgb) * 0.106595;
    if (first > 0.5) {
        float brightest = max(c.r, max(c.g, c.b));
        c = min(c * smoothstep(threshold, threshold + 2.0, brightest), vec3(12.0));
    }
    FRAGCOLOR = vec4(c, 1.0);
}
