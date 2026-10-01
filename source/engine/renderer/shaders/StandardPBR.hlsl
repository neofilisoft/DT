// Copyright Neofilisoft. All Rights Reserved.
// Standard PBR Shader for Lacrima Engine
// Ported from Freely Engine's Cook-Torrance BRDF

cbuffer GlobalUniforms : register(b0, space0) {
    float4x4 viewMatrix;
    float4x4 projectionMatrix;
    float4x4 viewProjectionMatrix;

    float4 lightDirection; // xyz, w = 0
    float4 lightColor;     // rgb, w = intensity
    float4 ambientColor;   // rgb, w = intensity
    float4 cameraPosition; // xyz, w = 1
};

[[vk::push_constant]]
struct PushConstants {
    float4x4 modelMatrix;
    float4 color;
} pc;

Texture2D albedoMap             : register(t0, space1);
SamplerState albedoSampler      : register(s0, space1);

Texture2D normalMap             : register(t1, space1);
SamplerState normalSampler      : register(s1, space1);

Texture2D mrMap                 : register(t2, space1);
SamplerState mrSampler          : register(s2, space1);

Texture2D aoMap                 : register(t3, space1);
SamplerState aoSampler          : register(s3, space1);

Texture2D emissiveMap           : register(t4, space1);
SamplerState emissiveSampler    : register(s4, space1);

struct VSInput {
    float3 position : POSITION;
    float3 normal   : NORMAL;
    float2 texCoord : TEXCOORD0;
    float4 tangent  : TANGENT;
};

struct PSInput {
    float4 position : SV_Position;
    float3 worldPos : POSITION0;
    float2 texCoord : TEXCOORD0;
    float4 color    : COLOR;
    float3x3 tbn    : MATRIX;
};

PSInput VSMain(VSInput input) {
    PSInput output;
    
    float4 worldPos = mul(pc.modelMatrix, float4(input.position, 1.0));
    output.worldPos = worldPos.xyz;
    output.position = mul(viewProjectionMatrix, worldPos);
    output.texCoord = input.texCoord;
    output.color = pc.color;

    // Normal mapping (TBN)
    float3x3 model3x3 = (float3x3)pc.modelMatrix;
    float3 T = normalize(mul(model3x3, input.tangent.xyz));
    float3 N = normalize(mul(model3x3, input.normal));
    
    // Re-orthogonalize T with respect to N
    T = normalize(T - dot(T, N) * N);
    
    // Compute Bitangent
    float3 B = cross(N, T) * input.tangent.w;
    
    output.tbn = float3x3(T, B, N);

    return output;
}

static const float PI = 3.14159265359;

float DistributionGGX(float3 N, float3 H, float roughness) {
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

float GeometrySmith(float3 N, float3 V, float3 L, float roughness) {
    float ggx2 = GeometrySchlickGGX(max(dot(N, V), 0.0), roughness);
    float ggx1 = GeometrySchlickGGX(max(dot(N, L), 0.0), roughness);
    return ggx1 * ggx2;
}

float3 FresnelSchlick(float cosTheta, float3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

float4 PSMain(PSInput input) : SV_Target {
    float4 albedoSamp = albedoMap.Sample(albedoSampler, input.texCoord);
    float3 albedo = pow(albedoSamp.rgb * input.color.rgb, float3(2.2, 2.2, 2.2));
    
    float2 mr = mrMap.Sample(mrSampler, input.texCoord).bg; // B = Metallic, G = Roughness
    float metallic = mr.x;
    float roughness = max(mr.y, 0.05); // prevent divide by zero
    
    float ao = aoMap.Sample(aoSampler, input.texCoord).r;
    float3 emissive = emissiveMap.Sample(emissiveSampler, input.texCoord).rgb;

    float3 normalSamp = normalMap.Sample(normalSampler, input.texCoord).rgb;
    normalSamp = normalSamp * 2.0 - 1.0;
    
    float3 N = normalize(mul(normalSamp, input.tbn));
    float3 V = normalize(cameraPosition.xyz - input.worldPos);
    
    float3 F0 = lerp(float3(0.04, 0.04, 0.04), albedo, metallic);
    float3 Lo = float3(0.0, 0.0, 0.0);
    
    // Single directional light
    if (lightColor.w > 0.0) {
        float3 L = normalize(-lightDirection.xyz);
        float3 H = normalize(V + L);
        
        float3 radiance = lightColor.rgb * lightColor.w;
        
        float NDF = DistributionGGX(N, H, roughness);
        float G   = GeometrySmith(N, V, L, roughness);
        float3 F  = FresnelSchlick(max(dot(H, V), 0.0), F0);
        
        float3 kD = (1.0 - F) * (1.0 - metallic);
        float NdotL = max(dot(N, L), 0.0);
        
        float3 numerator = NDF * G * F;
        float denominator = 4.0 * max(dot(N, V), 0.0) * NdotL + 0.0001;
        float3 specular = numerator / denominator;
        
        Lo += (kD * albedo / PI + specular) * radiance * NdotL;
    }
    
    float3 ambient = ambientColor.rgb * ambientColor.w * albedo * ao;
    float3 finalColor = ambient + Lo + emissive;
    
    // ACES Tone mapping + Gamma correction
    finalColor = finalColor / (finalColor + float3(1.0, 1.0, 1.0));
    finalColor = pow(finalColor, float3(1.0 / 2.2, 1.0 / 2.2, 1.0 / 2.2));
    
    return float4(finalColor, albedoSamp.a * input.color.a);
}
