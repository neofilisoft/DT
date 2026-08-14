// Copyright Neofilisoft. All Rights Reserved.
#version 450

// ---------------------------------------------------------------------------
// agent_quad.frag
//
// Outputs the interpolated per-agent color passed through from the vertex
// shader. No texturing, no lighting - purpose is proving the
// simulation->render pipeline end-to-end, not visual fidelity.
// ---------------------------------------------------------------------------

layout(location = 0) in  vec4 fragColor;
layout(location = 1) in  vec2 fragUV;

layout(binding = 0, set = 1) uniform sampler2D texSampler;

layout(location = 0) out vec4 outColor;

void main()
{
    vec4 texColor = texture(texSampler, fragUV);
    outColor = fragColor * texColor;
    
    // Alpha discard for transparency
    if (outColor.a < 0.1)
        discard;
}
