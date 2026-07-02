#version 330 core
in float vEta;
in vec3 vWorld;
out vec4 fragColor;

uniform float uAnom; // peak |anomaly| for colour normalisation

// Jet-style ramp for the wave crest: calm blue runs up through cyan, green and
// yellow into orange and red at the peak anomaly.
vec3 jet(float t) {
    const vec3 c0 = vec3(0.13, 0.34, 0.80); // calm ocean blue
    const vec3 c1 = vec3(0.10, 0.62, 0.88); // cyan
    const vec3 c2 = vec3(0.20, 0.80, 0.42); // green
    const vec3 c3 = vec3(0.95, 0.88, 0.20); // yellow
    const vec3 c4 = vec3(0.97, 0.52, 0.12); // orange
    const vec3 c5 = vec3(0.82, 0.10, 0.09); // red
    if (t < 0.2) return mix(c0, c1, t / 0.2);
    if (t < 0.4) return mix(c1, c2, (t - 0.2) / 0.2);
    if (t < 0.6) return mix(c2, c3, (t - 0.4) / 0.2);
    if (t < 0.8) return mix(c3, c4, (t - 0.6) / 0.2);
    return mix(c4, c5, (t - 0.8) / 0.2);
}

void main() {
    // No dry-cell discard: dry vertices are snapped to sea level (vEta = 0) in
    // the vertex shader, so those cells render as calm water. Discarding them
    // left a pale strip of bare shallow seabed along the coarse sim shoreline
    // wherever it disagreed with the fine terrain's coast. Land above sea
    // level occludes this sheet via the depth test.
    vec3 n = normalize(cross(dFdx(vWorld), dFdy(vWorld)));
    if (n.y < 0.0) n = -n;
    float diff = max(dot(n, normalize(vec3(0.35, 1.0, 0.25))), 0.0) * 0.4 + 0.6;

    float t = (uAnom > 0.0) ? clamp(vEta / uAnom, -1.0, 1.0) : 0.0;
    // Crests sweep the jet ramp; troughs deepen the calm blue.
    vec3 c = (t >= 0.0) ? jet(t)
                        : mix(vec3(0.13, 0.34, 0.80), vec3(0.02, 0.10, 0.32), -t);
    fragColor = vec4(c * diff, 0.82);
}
