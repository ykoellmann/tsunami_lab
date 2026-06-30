#version 330 core
in float vElev;
in float vDisp;
in vec3 vWorld;
out vec4 fragColor;

uniform int uMode;        // 0 = bathymetry, 1 = displacement
uniform float uDispRange; // peak |displacement| for normalising the colormap

// Hypsometric tint with fixed elevation breakpoints: a given absolute height
// always maps to the same colour, and low-lying land (e.g. Britain, mostly
// < 300 m) keeps visible relief instead of collapsing to a single green.
// Breakpoints must stay in sync with the legend in main_viz.cpp.
vec3 colormap(float h) {
    if (h < 0.0) {
        float d = -h; // depth, metres
        if (d < 200.0)  return mix(vec3(0.51, 0.74, 0.86), vec3(0.33, 0.62, 0.83), d / 200.0);
        if (d < 1000.0) return mix(vec3(0.33, 0.62, 0.83), vec3(0.18, 0.45, 0.76), (d - 200.0) / 800.0);
        if (d < 3000.0) return mix(vec3(0.18, 0.45, 0.76), vec3(0.09, 0.28, 0.58), (d - 1000.0) / 2000.0);
        return mix(vec3(0.09, 0.28, 0.58), vec3(0.03, 0.13, 0.38), clamp((d - 3000.0) / 3000.0, 0.0, 1.0));
    } else {
        if (h < 100.0)  return mix(vec3(0.27, 0.55, 0.27), vec3(0.45, 0.66, 0.32), h / 100.0);
        if (h < 300.0)  return mix(vec3(0.45, 0.66, 0.32), vec3(0.72, 0.73, 0.39), (h - 100.0) / 200.0);
        if (h < 600.0)  return mix(vec3(0.72, 0.73, 0.39), vec3(0.78, 0.66, 0.45), (h - 300.0) / 300.0);
        if (h < 1200.0) return mix(vec3(0.78, 0.66, 0.45), vec3(0.62, 0.48, 0.36), (h - 600.0) / 600.0);
        if (h < 2500.0) return mix(vec3(0.62, 0.48, 0.36), vec3(0.48, 0.40, 0.36), (h - 1200.0) / 1300.0);
        return mix(vec3(0.48, 0.40, 0.36), vec3(0.95, 0.95, 0.96), clamp((h - 2500.0) / 1500.0, 0.0, 1.0));
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
