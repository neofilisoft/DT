#version 450
// static_mesh.vert - Lacrima Engine 3D Static Mesh Vertex Shader (Step 4: CSM)

layout(set = 0, binding = 0) uniform GlobalUniforms {
    mat4 view;
    mat4 projection;
    mat4 viewProjection;
    vec4 lightDirection;
    vec4 lightColor;
    vec4 ambientColor;
    vec4 cameraPosition;
    // CSM fields follow in UBO but are only read in fragment shader
    mat4 lightSpaceMatrices[4];
    vec4 cascadeSplitDepths;
} ubo;

layout(push_constant) uniform PushConstants {
    mat4 model;
    vec4 color;
} pc;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec4 inTangent;

layout(location = 0) out vec3 fragWorldPos;
layout(location = 1) out vec2 fragTexCoord;
layout(location = 2) out vec4 fragColor;
layout(location = 3) out vec3 fragT;
layout(location = 4) out vec3 fragB;
layout(location = 5) out vec3 fragN;
layout(location = 6) out vec4 fragViewPos; // w = view-space depth for cascade selection

void main() {
    vec4 worldPos = pc.model * vec4(inPosition, 1.0);
    fragWorldPos  = worldPos.xyz;
    vec4 viewPos  = ubo.view * worldPos;
    gl_Position   = ubo.projection * viewPos;
    fragTexCoord  = inTexCoord;
    fragColor     = pc.color;
    fragViewPos   = vec4(viewPos.xyz, viewPos.z); // w = view-space z depth

    mat3 m3 = mat3(pc.model);
    vec3 T = normalize(m3 * inTangent.xyz);
    vec3 N = normalize(m3 * inNormal);
    T = normalize(T - dot(T, N) * N);
    vec3 B = cross(N, T) * inTangent.w;
    fragT = T;
    fragB = B;
    fragN = N;
}
