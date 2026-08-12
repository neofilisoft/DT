#version 450

layout(push_constant) uniform PushConstants {
    vec2 position;
    vec2 size;
    vec4 color;
    vec2 uvOffset;
    vec2 uvScale;
} pc;

layout(location = 0) out vec4 fragColor;
layout(location = 1) out vec2 fragUV;

// Quad vertices centered around origin, scaling by size and translating by position
vec2 positions[6] = vec2[](
    vec2(-0.5, -0.5),
    vec2( 0.5, -0.5),
    vec2(-0.5,  0.5),
    vec2( 0.5, -0.5),
    vec2( 0.5,  0.5),
    vec2(-0.5,  0.5)
);

vec2 uvs[6] = vec2[](
    vec2(0.0, 0.0),
    vec2(1.0, 0.0),
    vec2(0.0, 1.0),
    vec2(1.0, 0.0),
    vec2(1.0, 1.0),
    vec2(0.0, 1.0)
);

void main() {
    gl_Position = vec4(positions[gl_VertexIndex] * pc.size + pc.position, 0.0, 1.0);
    fragColor = pc.color;
    fragUV = pc.uvOffset + uvs[gl_VertexIndex] * pc.uvScale;
}
