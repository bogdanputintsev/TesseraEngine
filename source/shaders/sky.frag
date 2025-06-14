#version 450

layout(location = 0) in vec3 fragDirection;
layout(location = 0) out vec4 outColor;

void main() {
    // Normalize the direction — it points from the camera into the world
    vec3 dir = normalize(fragDirection);

    // Use the y-component for blending — the higher it is, the closer to the zenith
    float t = clamp(dir.y * 0.5 + 0.5, 0.0, 1.0);

    // Horizon and zenith colors (you can change them)
    vec3 horizonColor = vec3(0.8, 0.9, 1.0); // light blue
    vec3 zenithColor = vec3(0.0, 0.05, 0.3); // dark blue

    // Linear interpolation from horizon to zenith
    vec3 color = mix(horizonColor, zenithColor, t);

    outColor = vec4(color, 1.0);
}

