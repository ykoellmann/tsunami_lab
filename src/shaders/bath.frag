#version 330 core

in float vHeight;
out vec4 fragColor;

void main() {
    float h = vHeight;
    vec3 color;

    if (h < 0.0) {
        // ocean: dark blue (deep) → light blue (shallow)
        float t = clamp(h / -6000.0, 0.0, 1.0);
        color = mix(vec3(0.05, 0.18, 0.45), vec3(0.18, 0.52, 0.78), t);
    } else {
        // land: green (low) → brown (high)
        float t = clamp(h / 3000.0, 0.0, 1.0);
        color = mix(vec3(0.30, 0.58, 0.22), vec3(0.58, 0.42, 0.22), t);
    }

    fragColor = vec4(color, 1.0);
}
