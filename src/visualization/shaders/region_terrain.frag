#version 330 core
in float vElev;
in float vDisp;
in vec3 vWorld;
out vec4 fragColor;

uniform int uMode;        // 0 = bathymetry, 1 = displacement
uniform float uDispRange; // peak |displacement| for normalising the colormap

vec3 colormap(float h) {
    if (h < 0.0) {
        float t = clamp(h / -6000.0, 0.0, 1.0);
        return mix(vec3(0.05, 0.18, 0.45), vec3(0.22, 0.55, 0.80), 1.0 - t);
    } else {
        float t = clamp(h / 3000.0, 0.0, 1.0);
        return mix(vec3(0.30, 0.58, 0.22), vec3(0.62, 0.45, 0.25), t);
    }
}

// Diverging colormap: blue for subsidence, red for uplift, pale at zero.
vec3 displColormap(float d) {
    float t = (uDispRange > 0.0) ? clamp(d / uDispRange, -1.0, 1.0) : 0.0;
    vec3 zero = vec3(0.93, 0.93, 0.88);
    return (t >= 0.0) ? mix(zero, vec3(0.80, 0.18, 0.12), t)
                      : mix(zero, vec3(0.12, 0.30, 0.80), -t);
}

void main() {
    vec3 n = normalize(cross(dFdx(vWorld), dFdy(vWorld)));
    if (n.y < 0.0) n = -n;
    vec3 lightDir = normalize(vec3(0.35, 1.0, 0.25));
    float diff = max(dot(n, lightDir), 0.0) * 0.65 + 0.35;

    vec3 base;
    if (uMode == 0) {
        base = colormap(vElev) * diff;
        // Tint the deformed seafloor: warm for uplift, cool for subsidence.
        float m = clamp(abs(vDisp) / 5.0, 0.0, 1.0);
        vec3 tint = (vDisp >= 0.0) ? vec3(0.95, 0.35, 0.15)
                                   : vec3(0.55, 0.20, 0.85);
        base = mix(base, tint, 0.55 * m);
    } else {
        base = displColormap(vDisp) * diff;
    }
    fragColor = vec4(base, 1.0);
}
