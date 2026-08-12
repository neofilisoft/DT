#version 450

// ---------------------------------------------------------------------------
// agent_quad.vert
//
// Renders a single colored quad for one simulation agent. No vertex buffer
// is used - the unit quad is reconstructed from gl_VertexIndex (0..5),
// which avoids a vertex buffer allocation per frame entirely.
//
// Push constants carry per-agent data: world-space 2D position (from
// RenderProxy::positionX/Z), half-extent for quad size, and RGBA color.
// A UBO carries the orthographic projection (updated once per frame, not
// per agent).
//
// Why push constants for per-agent data, not an instance buffer:
//   Push constants are the zero-overhead path: one
//   vkCmdPushConstants + one vkCmdDraw per agent. An instance buffer
//   (vkCmdDrawInstanced, single draw call) is the correct optimization for
//   large agent counts, but the current job is to prove the pipeline works - the
//   buffer is the next logical step when instancing is actually needed.
// ---------------------------------------------------------------------------

layout(push_constant) uniform AgentPushConstants
{
    vec2  worldPos;    // agent center in world-space XZ plane
    vec2  halfExtent;  // half-width/half-height of the quad in world units
    vec4  color;       // linear RGBA
    vec2  uvOffset;
    vec2  uvScale;
} pc;

layout(binding = 0) uniform FrameUBO
{
    vec2 viewOrigin;   // world-space center visible on screen
    vec2 viewExtent;   // half-extent of the view frustum in world units
    vec2 screenSize;   // viewport dimensions in pixels (for aspect correction)
} ubo;

layout(location = 0) out vec4 fragColor;
layout(location = 1) out vec2 fragUV;

// Six vertices forming two triangles (CCW winding) covering [-1,1]x[-1,1]
// in local quad space. No index buffer needed.
const vec2 kLocalPositions[6] = vec2[](
    vec2(-1.0, -1.0),
    vec2( 1.0, -1.0),
    vec2(-1.0,  1.0),
    vec2( 1.0, -1.0),
    vec2( 1.0,  1.0),
    vec2(-1.0,  1.0)
);

const vec2 kLocalUVs[6] = vec2[](
    vec2(0.0, 0.0),
    vec2(1.0, 0.0),
    vec2(0.0, 1.0),
    vec2(1.0, 0.0),
    vec2(1.0, 1.0),
    vec2(0.0, 1.0)
);

void main()
{
    vec2 local = kLocalPositions[gl_VertexIndex];

    // Transform from local [-1,1] space to world space
    vec2 worldXZ = pc.worldPos + local * pc.halfExtent;

    // Orthographic projection: world -> NDC [-1,1]
    // viewOrigin + viewExtent define the visible world rectangle.
    // Y axis in Vulkan NDC points down; world Z is "screen up" in our
    // top-down 2D view, so we negate it.
    vec2 ndc;
    ndc.x =  (worldXZ.x - ubo.viewOrigin.x) / ubo.viewExtent.x;
    ndc.y = -(worldXZ.y - ubo.viewOrigin.y) / ubo.viewExtent.y;

    gl_Position = vec4(ndc, 0.0, 1.0);
    fragColor   = pc.color;
    
    vec2 baseUV = kLocalUVs[gl_VertexIndex];
    fragUV = pc.uvOffset + baseUV * pc.uvScale;
}
