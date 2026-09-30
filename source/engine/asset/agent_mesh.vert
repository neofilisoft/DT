// Copyright Neofilisoft. All Rights Reserved.
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
layout(location = 3) in vec4 inTangent;

layout(location = 0) out vec3 fragWorldPos;
layout(location = 1) out vec2 fragTexCoord;
layout(location = 2) out vec4 fragColor;
layout(location = 3) out mat3 fragTBN;

void main() {
    vec4 worldPos = pc.modelMatrix * vec4(inPosition, 1.0);
    fragWorldPos = worldPos.xyz;
    gl_Position = ubo.viewProjectionMatrix * worldPos;
    fragTexCoord = inTexCoord;
    fragColor = pc.color;

    mat3 model3x3 = mat3(pc.modelMatrix);
    vec3 T = normalize(model3x3 * inTangent.xyz);
    vec3 N = normalize(model3x3 * inNormal);
    
    // Gram-Schmidt
    T = normalize(T - dot(T, N) * N);
    
    vec3 B = cross(N, T) * inTangent.w;
    
    fragTBN = mat3(T, B, N);
}
