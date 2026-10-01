#version 450
// shadow_depth.vert - Lacrima Engine CSM Shadow Pass
// Renders static mesh geometry from the directional light perspective.
// One draw per cascade; cascade index selects the correct light-space matrix.

layout(set = 0, binding = 0) uniform ShadowUBO {
    mat4 lightSpaceMatrices[4]; // one per cascade
} shadowUbo;

layout(push_constant) uniform ShadowPC {
    mat4  modelMatrix;
    uint  cascadeIndex;
    float _pad0;
    float _pad1;
    float _pad2;
} pc;

layout(location = 0) in vec3 inPosition;
// Normal / uv / tangent bound from same vertex buffer but unused in depth-only pass
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec4 inTangent;

void main()
{
    gl_Position = shadowUbo.lightSpaceMatrices[pc.cascadeIndex] * pc.modelMatrix * vec4(inPosition, 1.0);
}
