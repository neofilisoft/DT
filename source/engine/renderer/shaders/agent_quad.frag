#version 450

// ---------------------------------------------------------------------------
// agent_quad.frag
//
// Outputs the interpolated per-agent color passed through from the vertex
// shader. No texturing, no lighting - M5's purpose is proving the
// simulation->render pipeline end-to-end, not visual fidelity.
// ---------------------------------------------------------------------------

layout(location = 0) in  vec4 fragColor;
layout(location = 0) out vec4 outColor;

void main()
{
    outColor = fragColor;
}
