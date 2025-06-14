#version 450

// Global UBO - set 0
layout(set = 0, binding = 0, std140) uniform GlobalUBO {
    mat4 view;
    mat4 proj;
    vec3 cameraPos;
} globalUBO;

// Instance UBO - set 1
layout(set = 1, binding = 0, std140) uniform InstanceUBO {
    mat4 model;
    mat4 normalMatrix;
} instanceUBO;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec3 inTangent;

layout(location = 0) out vec3 fragPos;
layout(location = 1) out vec2 fragTexCoord;
layout(location = 2) out mat3 fragTBN;

mat3 CalculateTBN()
{
    vec3 N = normalize(inNormal);
    vec3 T = normalize(inTangent);
    vec3 B = normalize(cross(N, T)); // Ensure B is orthogonal

    return mat3(T, B, N);
}

void main() {
    gl_Position = globalUBO.proj * globalUBO.view * instanceUBO.model * vec4(inPosition, 1.0);

    fragPos = vec3(instanceUBO.model * vec4(inPosition, 1.0));
    fragTexCoord = inTexCoord;
    fragTBN = CalculateTBN();
}