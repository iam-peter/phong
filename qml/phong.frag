// The Phong material: a key light from above in front, a fill light at
// the camera, ambient light, their highlights and the glow. Lit by hand, the
// built-in lighting uploads every light slot for every draw.
VARYING vec3 vNormal;
VARYING vec3 vToCamera;
// Color and glow of an instance, white and 1 otherwise
VARYING vec4 vTint;
VARYING float vGlow;

void MAIN()
{
    vec3 base = color.rgb * vTint.rgb;
    vec3 result = base;
    if (lit > 0.5) {
        vec3 n = normalize(vNormal);
        vec3 v = normalize(vToCamera);
        float key = max(dot(n, keyDirection), 0.0);
        float fill = max(dot(n, v), 0.0);
        result = base * (ambient + keyStrength * key + fillStrength * fill);
        // A narrow highlight of the key light and a soft one of the fill
        // light, strong on shiny surfaces like the balls, none on the floor
        float highlight = 3.0 * pow(max(dot(n, normalize(keyDirection + v)), 0.0), highlightPower) * keyStrength
                          + pow(fill, 0.1 * highlightPower) * fillStrength;
        result += vec3(highlight * specular);
    }
    // Unshaded materials have no OBJECT_OPACITY, Qt keeps the opacity of
    // the node in the alpha of its material properties
    FRAGCOLOR = vec4(result + base * emission * vGlow, color.a * vTint.a * qt_material_properties.a);
}
