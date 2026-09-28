// Glow, last pass: the scene, smoothed at its edges if wanted and
// tonemapped, the glow levels tonemapped on top, like
// ExtendedSceneEnvironment does it. Qt's own tonemapping is off.
vec3 scene(vec2 uv)
{
    return texture(INPUT, uv).rgb;
}

// FXAA in its simple form: along the edge through the pixel
vec3 smoothed(vec2 uv)
{
    vec2 px = 1.0 / INPUT_SIZE;
    vec3 luma = vec3(0.299, 0.587, 0.114);
    vec3 m = scene(uv);
    float lumaNW = dot(min(scene(uv + vec2(-px.x, -px.y)), vec3(1.0)), luma);
    float lumaNE = dot(min(scene(uv + vec2(px.x, -px.y)), vec3(1.0)), luma);
    float lumaSW = dot(min(scene(uv + vec2(-px.x, px.y)), vec3(1.0)), luma);
    float lumaSE = dot(min(scene(uv + vec2(px.x, px.y)), vec3(1.0)), luma);
    float lumaM = dot(min(m, vec3(1.0)), luma);
    float lumaMin = min(lumaM, min(min(lumaNW, lumaNE), min(lumaSW, lumaSE)));
    float lumaMax = max(lumaM, max(max(lumaNW, lumaNE), max(lumaSW, lumaSE)));
    if (lumaMax - lumaMin < max(0.0312, lumaMax * 0.125))
        return m;

    vec2 dir = vec2(-((lumaNW + lumaNE) - (lumaSW + lumaSE)), (lumaNW + lumaSW) - (lumaNE + lumaSE));
    float reduce = max((lumaNW + lumaNE + lumaSW + lumaSE) * 0.03125, 1.0 / 128.0);
    float scale = 1.0 / (min(abs(dir.x), abs(dir.y)) + reduce);
    dir = clamp(dir * scale, vec2(-8.0), vec2(8.0)) * px;
    vec3 a = 0.5 * (scene(uv + dir * (1.0 / 3.0 - 0.5)) + scene(uv + dir * (2.0 / 3.0 - 0.5)));
    vec3 b = 0.5 * a + 0.25 * (scene(uv - dir * 0.5) + scene(uv + dir * 0.5));
    float lumaB = dot(min(b, vec3(1.0)), luma);
    return (lumaB < lumaMin || lumaB > lumaMax) ? a : b;
}

// The linear tonemapping of Qt: clamped and to sRGB
vec3 tonemap(vec3 c)
{
    c = clamp(c, vec3(0.0), vec3(1.0));
    return mix(1.055 * pow(c, vec3(1.0 / 2.4)) - 0.055, 12.92 * c, lessThan(c, vec3(0.0031308)));
}

void MAIN()
{
    vec3 c = tonemap(fxaa > 0.5 ? smoothed(INPUT_UV) : scene(INPUT_UV));
    // Without glow its buffers are left as they are, not read
    if (intensity > 0.0) {
        vec3 glow = texture(glow1, INPUT_UV).rgb + texture(glow2, INPUT_UV).rgb;
        if (wide > 0.5)
            glow += texture(glow3, INPUT_UV).rgb;
        c += tonemap(glow * intensity);
    }
    FRAGCOLOR = vec4(c, 1.0);
}
