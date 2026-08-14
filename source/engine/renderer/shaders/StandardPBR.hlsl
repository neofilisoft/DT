// Copyright Neofilisoft. All Rights Reserved.
// Standard PBR Shader - Engine Core

struct DirectionalLight {
    float3 direction;
    float pad0;
    float3 color;
    float intensity;
};

struct PointLight {
    float3 position;
    float radius;
    float3 color;
    float intensity;
    float4 attenuation; // x=const, y=linear, z=quad
};

struct SpotLight {
    float3 position;
    float radius;
    float3 direction;
    float intensity;
    float3 color;
    float innerConeCos;
    float4 params; // x=outerConeCos, y=const, z=linear, w=quad
};

cbuffer LightDataUBO : register(b1) {
    DirectionalLight dirLight;
    
    uint pointLightCount;
    uint spotLightCount;
    uint2 padding;

    PointLight pointLights[16];
    SpotLight spotLights[16];
};

cbuffer CameraUBO : register(b0) {
    float4x4 mvp;
    float4x4 model;
    float4 cameraPos;
};

Texture2D baseColorTex : register(t2);
SamplerState baseColorSampler : register(s3);

struct VSInput {
    float3 position : POSITION;
    float3 color : COLOR;
    float3 normal : NORMAL;
    float2 texCoord : TEXCOORD0;
};

struct PSInput {
    float4 position : SV_POSITION;
    float3 worldPos : POSITION0;
    float3 color : COLOR;
    float3 normal : NORMAL;
    float2 texCoord : TEXCOORD0;
};

PSInput VSMain(VSInput input) {
    PSInput output;
    output.position = mul(mvp, float4(input.position, 1.0));
    output.worldPos = mul(model, float4(input.position, 1.0)).xyz;
    
    // Normal transform (assuming uniform scale for now)
    output.normal = normalize(mul((float3x3)model, input.normal));
    
    output.color = input.color;
    output.texCoord = input.texCoord;
    return output;
}

float4 PSMain(PSInput input) : SV_TARGET {
    float4 baseColor = baseColorTex.Sample(baseColorSampler, input.texCoord);
    
    float3 normal = normalize(input.normal);
    float3 viewDir = normalize(cameraPos.xyz - input.worldPos);
    
    // Simple Lambertian for Directional Light
    float3 lightDir = normalize(-dirLight.direction);
    float NdotL = max(dot(normal, lightDir), 0.0);
    float3 diffuse = dirLight.color * dirLight.intensity * NdotL;

    // TODO: Loop through Point Lights
    for (uint i = 0; i < pointLightCount; ++i) {
        float3 lightVec = pointLights[i].position - input.worldPos;
        float distance = length(lightVec);
        if (distance < pointLights[i].radius) {
            float3 lDir = lightVec / distance;
            float atten = 1.0 / (pointLights[i].attenuation.x + 
                                 pointLights[i].attenuation.y * distance + 
                                 pointLights[i].attenuation.z * distance * distance);
            float ndl = max(dot(normal, lDir), 0.0);
            diffuse += pointLights[i].color * pointLights[i].intensity * ndl * atten;
        }
    }

    // Ambient Baseline
    float3 ambient = float3(0.05, 0.05, 0.08) * baseColor.rgb;
    
    float3 finalColor = ambient + diffuse * baseColor.rgb;

    return float4(finalColor, baseColor.a);
}
