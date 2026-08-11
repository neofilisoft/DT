#version 450

layout(set = 0, binding = 0) uniform GlobalUniforms {
    mat4 viewMatrix;
    mat4 projectionMatrix;
    mat4 viewProjectionMatrix;
    vec4 lightDirection;
    vec4 lightColor;
    vec4 ambientColor;
    vec4 cameraPosition;
} ubo;

layout(push_constant) uniform PushConstants {
    mat4 modelMatrix;
    vec4 color;
} pc;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;

layout(location = 0) out vec3 fragNormal;
layout(location = 1) out vec2 fragTexCoord;
layout(location = 2) out vec3 fragPosWorld;

void main() {
    vec4 worldPos = pc.modelMatrix * vec4(inPosition, 1.0);
    gl_Position = ubo.viewProjectionMatrix * worldPos;
    
    // Transform normal to world space (assuming uniform scaling for now)
    fragNormal = mat3(pc.modelMatrix) * inNormal;
    fragTexCoord = inTexCoord;
    fragPosWorld = worldPos.xyz;
}
