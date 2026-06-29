#version 330 core
in float vEta;
in float vH;
in vec3 vWorld;
out vec4 fragColor;

uniform float uAnom; // peak |anomaly| for colour normalisation

void main() {
    if (vH < 0.01) discard; // dry cell: let the seabed show

    vec3 n = normalize(cross(dFdx(vWorld), dFdy(vWorld)));
    if (n.y < 0.0) n = -n;
    float diff = max(dot(n, normalize(vec3(0.35, 1.0, 0.25))), 0.0) * 0.4 + 0.6;

    float t = (uAnom > 0.0) ? clamp(vEta / uAnom, -1.0, 1.0) : 0.0;
    vec3 calm = vec3(0.10, 0.42, 0.72);
    vec3 c = (t >= 0.0) ? mix(calm, vec3(0.95, 0.90, 0.95), t)
                        : mix(calm, vec3(0.02, 0.10, 0.32), -t);
    fragColor = vec4(c * diff, 0.78);
}
