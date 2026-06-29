#version 330 core
layout(location = 0) in vec2 aXZ;
uniform mat4 uVP;
void main() {
    gl_Position = uVP * vec4(aXZ.x, 0.0, aXZ.y, 1.0);
}
