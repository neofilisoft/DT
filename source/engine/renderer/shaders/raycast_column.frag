#version 450
layout(set = 1, binding = 0) uniform sampler2D texSampler;
layout(location = 0) in vec2 fragUV;
layout(location = 1) in vec4 fragColor;
layout(location = 0) out vec4 outColor;
void main()
{
    outColor = texture(texSampler, fragUV) * fragColor;
    if (outColor.a < 0.1)
        discard;
}