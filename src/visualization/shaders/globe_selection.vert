#version 330 core
layout(location = 0) in vec2 aPos; // (lon, lat)
uniform mat4 uVP;
void main() {
    // Slightly above y=0 to avoid z-fighting with terrain; same lat negation as terrain.
    gl_Position = uVP * vec4(aPos.x, 0.05, -aPos.y, 1.0);
}
