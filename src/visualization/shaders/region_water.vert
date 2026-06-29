#version 330 core
layout(location = 0) in vec2 aXZ;
layout(location = 1) in float aBath; // metres (negative under water)
layout(location = 2) in float aH;    // water column, metres

uniform mat4 uVP;
uniform float uScaleY;    // metres → world units (incl. terrain exaggeration)
uniform float uWaveExagg; // additional multiplier for wave anomaly

out float vEta; // surface elevation rel. to sea level, metres
out float vH;
out vec3 vWorld;

void main() {
    float eta = aBath + aH;
    // Land vertices have eta = b > 0; interpolating across ocean–land edges
    // would colour the boundary white. Snap to 0 before interpolation.
    float posEta = (aH >= 0.01) ? eta : 0.0;
    vEta = posEta;
    vH = aH;
    // Reduce exaggeration toward 1x in shallow water: shoaling already
    // amplifies the wave there, so full uWaveExagg creates extreme spikes.
    float depth = max(0.0, -aBath);
    float depthScale = clamp(depth / 500.0, 0.0, 1.0);
    float effExagg = 1.0 + (uWaveExagg - 1.0) * depthScale;
    vec3 world = vec3(aXZ.x, posEta * uScaleY * effExagg, aXZ.y);
    vWorld = world;
    gl_Position = uVP * vec4(world, 1.0);
}
