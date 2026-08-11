#version 450

layout(set = 0, binding = 0) uniform GlobalUniforms {
    mat4 viewMatrix;
    mat4 projectionMatrix;
    mat4 viewProjectionMatrix;
    vec4 lightDirection;
    vec4 lightColor;
    vec4 ambientColor;
    vec4 cameraPosition;
} ubo;

layout(set = 1, binding = 0) uniform sampler2D texSampler;

layout(push_constant) uniform PushConstants {
    mat4 modelMatrix;
    vec4 color;
} pc;

layout(location = 0) in vec3 fragNormal;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) in vec3 fragPosWorld;

layout(location = 0) out vec4 outColor;

void main() {
    vec4 texColor = texture(texSampler, fragTexCoord);
    vec3 baseColor = texColor.rgb * pc.color.rgb;
    
    // Ambient
    vec3 ambient = ubo.ambientColor.rgb * ubo.ambientColor.a * baseColor;
    
    // Diffuse
    vec3 norm = normalize(fragNormal);
    vec3 lightDir = normalize(-ubo.lightDirection.xyz);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * ubo.lightColor.rgb * ubo.lightColor.a * baseColor;
    
    // Result
    vec3 result = ambient + diffuse;
    
    // Alpha blending
    float alpha = texColor.a * pc.color.a;
    if (alpha < 0.1) discard; // Simple alpha testing
    
    outColor = vec4(result, alpha);
}
