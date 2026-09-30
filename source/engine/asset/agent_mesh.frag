// Copyright Neofilisoft. All Rights Reserved.
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

layout(set = 1, binding = 0) uniform sampler2D albedoMap;
layout(set = 1, binding = 1) uniform sampler2D normalMap;
layout(set = 1, binding = 2) uniform sampler2D mrMap;
layout(set = 1, binding = 3) uniform sampler2D aoMap;
layout(set = 1, binding = 4) uniform sampler2D emissiveMap;

layout(location = 0) in vec3 fragWorldPos;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) in vec4 fragColor;
layout(location = 3) in mat3 fragTBN;

layout(location = 0) out vec4 outColor;

const float PI = 3.14159265359;

float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    return a2 / (PI * denom * denom);
}

float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    return NdotV / (NdotV * (1.0 - k) + k);
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float ggx2 = GeometrySchlickGGX(max(dot(N, V), 0.0), roughness);
    float ggx1 = GeometrySchlickGGX(max(dot(N, L), 0.0), roughness);
    return ggx1 * ggx2;
}

vec3 FresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

void main() {
    vec4 albedoSamp = texture(albedoMap, fragTexCoord);
    vec3 albedo = pow(albedoSamp.rgb * fragColor.rgb, vec3(2.2));
    
    vec2 mr = texture(mrMap, fragTexCoord).bg;
    float metallic = mr.x;
    float roughness = max(mr.y, 0.05);
    
    float ao = texture(aoMap, fragTexCoord).r;
    vec3 emissive = texture(emissiveMap, fragTexCoord).rgb;
    
    vec3 normalSamp = texture(normalMap, fragTexCoord).rgb;
    normalSamp = normalSamp * 2.0 - 1.0;
    vec3 N = normalize(fragTBN * normalSamp);
    
    vec3 V = normalize(ubo.cameraPosition.xyz - fragWorldPos);
    
    vec3 F0 = mix(vec3(0.04), albedo, metallic);
    vec3 Lo = vec3(0.0);
    
    if (ubo.lightColor.w > 0.0) {
        vec3 L = normalize(-ubo.lightDirection.xyz);
        vec3 H = normalize(V + L);
        
        vec3 radiance = ubo.lightColor.rgb * ubo.lightColor.w;
        
        float NDF = DistributionGGX(N, H, roughness);
        float G   = GeometrySmith(N, V, L, roughness);
        vec3 F    = FresnelSchlick(max(dot(H, V), 0.0), F0);
        
        vec3 kD = (1.0 - F) * (1.0 - metallic);
        float NdotL = max(dot(N, L), 0.0);
        
        vec3 numerator = NDF * G * F;
        float denominator = 4.0 * max(dot(N, V), 0.0) * NdotL + 0.0001;
        vec3 specular = numerator / denominator;
        
        Lo += (kD * albedo / PI + specular) * radiance * NdotL;
    }
    
    vec3 ambient = ubo.ambientColor.rgb * ubo.ambientColor.w * albedo * ao;
    vec3 color = ambient + Lo + emissive;
    
    color = color / (color + vec3(1.0));
    color = pow(color, vec3(1.0 / 2.2));
    
    outColor = vec4(color, albedoSamp.a * fragColor.a);
}
