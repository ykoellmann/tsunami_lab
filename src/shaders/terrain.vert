#version 330 core

layout(location = 0) in vec2 aXZ;   // x, z position (metres)
layout(location = 1) in float aY;   // height (metres)

uniform mat4 uMVP;
uniform float uYScale;              // vertical exaggeration

out float vHeight;

void main() {
    vHeight = aY;
    gl_Position = uMVP * vec4(aXZ.x, aY * uYScale, aXZ.y, 1.0);
}
