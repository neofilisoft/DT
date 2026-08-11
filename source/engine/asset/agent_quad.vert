#version 450

layout(push_constant) uniform PushConstants {
    vec2 position;
    vec2 size;
    vec4 color;
} pc;

layout(location = 0) out vec4 fragColor;

// Quad vertices centered around origin, scaling by size and translating by position
vec2 positions[6] = vec2[](
    vec2(-0.5, -0.5),
    vec2( 0.5, -0.5),
    vec2(-0.5,  0.5),
    vec2( 0.5, -0.5),
    vec2( 0.5,  0.5),
    vec2(-0.5,  0.5)
);

void main() {
    gl_Position = vec4(positions[gl_VertexIndex] * pc.size + pc.position, 0.0, 1.0);
    fragColor = pc.color;
}
