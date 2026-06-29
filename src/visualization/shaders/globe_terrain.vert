#version 330 core
layout(location = 0) in vec2 aPos;   // (lon, lat) in degrees
layout(location = 1) in float aElev; // metres

uniform mat4 uVP;

out float vElev;

void main() {
    vElev = aElev;
    // Negate lat so that north (+lat) maps to -Z; with azimuth=0 this puts
    // north at the top of the screen and east to the right.
    gl_Position = uVP * vec4(aPos.x, 0.0, -aPos.y, 1.0);
}
