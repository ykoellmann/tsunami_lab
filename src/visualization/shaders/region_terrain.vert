#version 330 core
layout(location = 0) in vec2 aXZ;    // (x, z) world position
layout(location = 1) in float aElev; // metres
layout(location = 2) in float aDisp; // vertical seafloor displacement, metres

uniform mat4 uVP;
uniform float uScaleY;     // metres → world units for elevation (incl. exagg.)
uniform float uDispScaleY; // metres → world units for displacement
uniform int uMode;         // 0 = bathymetry, 1 = displacement

out float vElev;
out float vDisp;
out vec3 vWorld;

void main() {
    // Bathymetry mode shows the deformed seabed (relief + displacement);
    // displacement mode shows the displacement field on its own.
    float y = (uMode == 0) ? (aElev + aDisp) * uScaleY : aDisp * uDispScaleY;
    vec3 world = vec3(aXZ.x, y, aXZ.y);
    vWorld = world;
    vElev = aElev;
    vDisp = aDisp;
    gl_Position = uVP * vec4(world, 1.0);
}
