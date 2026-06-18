#version 330 core

in float vHeight;
out vec4 fragColor;

uniform float uRestHeight;   // calm water level (= -bathymetry where bath < 0)

void main() {
    float anomaly = vHeight - uRestHeight;

    // blue (calm) → red (wave crest), with slight transparency
    float t = clamp((anomaly + 2.0) / 4.0, 0.0, 1.0);
    vec3 color = mix(vec3(0.05, 0.28, 0.68), vec3(0.92, 0.22, 0.10), t);

    fragColor = vec4(color, 0.82);
}
