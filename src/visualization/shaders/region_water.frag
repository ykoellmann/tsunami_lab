#version 330 core
in float vEta;
in vec3 vWorld;
out vec4 fragColor;

uniform float uAnom; // peak |anomaly| for colour normalisation

// Diverging colormap: cool violet ramp for troughs, warm jet ramp for crests.
// t in [0,1] for crests, t in [-1,0] for troughs.
vec3 wavemap(float t) {
    // Crest: calm blue → cyan → green → yellow → orange → red
    const vec3 p0 = vec3(0.13, 0.34, 0.80);
    const vec3 p1 = vec3(0.10, 0.62, 0.88);
    const vec3 p2 = vec3(0.20, 0.80, 0.42);
    const vec3 p3 = vec3(0.95, 0.88, 0.20);
    const vec3 p4 = vec3(0.97, 0.52, 0.12);
    const vec3 p5 = vec3(0.82, 0.10, 0.09);
    // Trough: calm blue → indigo → deep violet → dark purple
    const vec3 n1 = vec3(0.22, 0.28, 0.82);
    const vec3 n2 = vec3(0.42, 0.12, 0.78);
    const vec3 n3 = vec3(0.55, 0.05, 0.55);
    const vec3 n4 = vec3(0.32, 0.02, 0.30);

    if (t >= 0.0) {
        if (t < 0.2) return mix(p0, p1, t / 0.2);
        if (t < 0.4) return mix(p1, p2, (t - 0.2) / 0.2);
        if (t < 0.6) return mix(p2, p3, (t - 0.4) / 0.2);
        if (t < 0.8) return mix(p3, p4, (t - 0.6) / 0.2);
        return mix(p4, p5, (t - 0.8) / 0.2);
    } else {
        float s = -t;
        if (s < 0.25) return mix(p0, n1, s / 0.25);
        if (s < 0.50) return mix(n1, n2, (s - 0.25) / 0.25);
        if (s < 0.75) return mix(n2, n3, (s - 0.50) / 0.25);
        return mix(n3, n4, (s - 0.75) / 0.25);
    }
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
    fragColor = vec4(wavemap(t) * diff, 0.82);
}
