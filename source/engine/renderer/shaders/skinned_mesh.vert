#version 450
// skinned_mesh.vert - Lacrima Engine GPU Vertex Skinning Shader (Step 3 + 4 CSM)
// Supports up to 128 bones via SSBO. Outputs view-space depth for CSM cascade selection.

layout(set = 0, binding = 0) uniform GlobalUniforms {
    mat4 view;
    mat4 projection;
    mat4 viewProjection;
    vec4 lightDirection;
    vec4 lightColor;
    vec4 ambientColor;
    vec4 cameraPosition;
    mat4 lightSpaceMatrices[4];
    vec4 cascadeSplitDepths;
} ubo;

// Bone matrix palette - up to 128 bones via storage buffer (set=2, binding=0)
layout(set = 2, binding = 0) readonly buffer BonePalette {
    mat4 bones[128];
} bonePalette;

layout(push_constant) uniform PushConstants {
    mat4 model;
    vec4 color;
} pc;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec4 inTangent;
layout(location = 4) in uvec4 inJointIndices;
layout(location = 5) in vec4  inJointWeights;

layout(location = 0) out vec3 fragWorldPos;
layout(location = 1) out vec2 fragTexCoord;
layout(location = 2) out vec4 fragColor;
layout(location = 3) out vec3 fragT;
layout(location = 4) out vec3 fragB;
layout(location = 5) out vec3 fragN;
layout(location = 6) out vec4 fragViewPos; // w = view-space z for cascade selection

void main() {
    mat4 skinMatrix =
        inJointWeights.x * bonePalette.bones[inJointIndices.x] +
        inJointWeights.y * bonePalette.bones[inJointIndices.y] +
        inJointWeights.z * bonePalette.bones[inJointIndices.z] +
        inJointWeights.w * bonePalette.bones[inJointIndices.w];

    vec4 localPos  = skinMatrix * vec4(inPosition, 1.0);
    vec4 worldPos  = pc.model * localPos;
    fragWorldPos   = worldPos.xyz;
    vec4 viewPos   = ubo.view * worldPos;
    gl_Position    = ubo.projection * viewPos;
    fragViewPos    = vec4(viewPos.xyz, viewPos.z);

    fragTexCoord   = inTexCoord;
    fragColor      = pc.color;

    mat3 skinNormal = mat3(skinMatrix);
    mat3 m3         = mat3(pc.model) * skinNormal;
    vec3 T = normalize(m3 * inTangent.xyz);
    vec3 N = normalize(m3 * inNormal);
    T = normalize(T - dot(T, N) * N);
    vec3 B = cross(N, T) * inTangent.w;
    fragT = T;
    fragB = B;
    fragN = N;
}
