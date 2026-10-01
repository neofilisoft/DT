#version 450
layout(push_constant) uniform RaycastPushConstants
{
    vec4 geometry; // ndc center x, half width, half height, center y
    vec4 uv;       // wall U, atlas row, shade, unused
    vec4 color;
} pc;
layout(location = 0) out vec2 fragUV;
layout(location = 1) out vec4 fragColor;
const vec2 positions[6] = vec2[](
    vec2(-1.0, -1.0), vec2(1.0, -1.0), vec2(-1.0, 1.0),
    vec2(1.0, -1.0), vec2(1.0, 1.0), vec2(-1.0, 1.0));
const vec2 uvs[6] = vec2[](
    vec2(0.0, 0.0), vec2(1.0, 0.0), vec2(0.0, 1.0),
    vec2(1.0, 0.0), vec2(1.0, 1.0), vec2(0.0, 1.0));
void main()
{
    vec2 local = positions[gl_VertexIndex];
    gl_Position = vec4(pc.geometry.x + local.x * pc.geometry.y,
                       pc.geometry.w + local.y * pc.geometry.z, 0.0, 1.0);
    vec2 localUV = uvs[gl_VertexIndex];
    fragUV = vec2(pc.uv.x, (pc.uv.y + localUV.y) / max(pc.uv.w, 1.0));
    fragColor = pc.color * pc.uv.z;
}