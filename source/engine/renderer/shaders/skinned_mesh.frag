#version 450
// skinned_mesh.frag - Lacrima Engine Skinned Mesh PBR Fragment Shader (Step 3 + 4 CSM)
// Full PBR lighting with Cascaded Shadow Map (CSM) PCF soft shadows.

// ---------------------------------------------------------------------------
// Set 0: Global Uniforms
// ---------------------------------------------------------------------------
layout(set = 0, binding = 0) uniform GlobalUniforms {
    mat4 view;
    mat4 projection;
    mat4 viewProjection;
    vec4 lightDirection;   // direction TO light (normalised), w=0
    vec4 lightColor;       // RGB + Intensity (w)
    vec4 ambientColor;     // RGB + Intensity (w)
    vec4 cameraPosition;   // xyz, w=1

    // CSM
    mat4 lightSpaceMatrices[4]; // one per cascade
    vec4 cascadeSplitDepths;    // x,y,z,w = far-plane of cascades 0,1,2,3 (view space depth)
} ubo;

// ---------------------------------------------------------------------------
// Set 1: PBR Material textures
// ---------------------------------------------------------------------------
layout(set = 1, binding = 0) uniform sampler2D albedoMap;
layout(set = 1, binding = 1) uniform sampler2D normalMap;
layout(set = 1, binding = 2) uniform sampler2D mrMap;
layout(set = 1, binding = 3) uniform sampler2D aoMap;
layout(set = 1, binding = 4) uniform sampler2D emissiveMap;

// ---------------------------------------------------------------------------
// Set 3: CSM shadow maps (2D array, depth comparison sampler)
// ---------------------------------------------------------------------------
layout(set = 3, binding = 0) uniform sampler2DArrayShadow shadowMap;

// ---------------------------------------------------------------------------
// Varyings (matches skinned_mesh.vert)
// ---------------------------------------------------------------------------
layout(location = 0) in vec3 fragWorldPos;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) in vec4 fragColor;
layout(location = 3) in vec3 fragT;
layout(location = 4) in vec3 fragB;
layout(location = 5) in vec3 fragN;
layout(location = 6) in vec4 fragViewPos; // clip-space (or view-pos.z in w)

layout(location = 0) out vec4 outColor;

const float PI = 3.14159265359;

// --- PBR helper functions ---------------------------------------------------
float DistributionGGX(vec3 N, vec3 H, float r) {
    float a2 = r*r*r*r;
    float d  = max(dot(N,H),0.0);
    float q  = d*d*(a2-1.0)+1.0;
    return a2/(PI*q*q);
}
float GeoSchlick(float x, float r) {
    float k=(r+1.0)*(r+1.0)/8.0;
    return x/(x*(1.0-k)+k);
}
float GeometrySmith(vec3 N, vec3 V, vec3 L, float r) {
    return GeoSchlick(max(dot(N,V),0.0),r)*GeoSchlick(max(dot(N,L),0.0),r);
}
vec3 FresnelSchlick(float c, vec3 F0) {
    return F0+(1.0-F0)*pow(clamp(1.0-c,0.0,1.0),5.0);
}

// --- PCF 3x3 shadow sampling ------------------------------------------------
float SampleShadowPCF(int cascade, vec3 worldPos, float bias)
{
    vec4 lightSpacePos = ubo.lightSpaceMatrices[cascade] * vec4(worldPos, 1.0);

    // Perspective divide
    vec3 projCoords = lightSpacePos.xyz / lightSpacePos.w;

    // Remap XY from [-1,1] to [0,1] for texture sampling
    projCoords.x = projCoords.x * 0.5 + 0.5;
    projCoords.y = projCoords.y * 0.5 + 0.5;

    // Outside shadow map = fully lit
    if (projCoords.x < 0.0 || projCoords.x > 1.0 ||
        projCoords.y < 0.0 || projCoords.y > 1.0)
        return 1.0;

    float shadow  = 0.0;
    float depth   = projCoords.z - bias;
    vec2  texelSize = vec2(1.0 / 2048.0); // kCSMResolution

    // 3x3 PCF kernel
    for (int x = -1; x <= 1; ++x)
    {
        for (int y = -1; y <= 1; ++y)
        {
            vec2 offset = vec2(x, y) * texelSize;
            shadow += texture(shadowMap, vec4(projCoords.xy + offset, float(cascade), depth));
        }
    }
    return shadow / 9.0;
}

// Select the correct CSM cascade from view-space depth
float ComputeShadow(vec3 worldPos, float viewDepth)
{
    float bias = 0.005;

    float absDepth = abs(viewDepth);
    int cascade = 3;
    if      (absDepth < ubo.cascadeSplitDepths.x) cascade = 0;
    else if (absDepth < ubo.cascadeSplitDepths.y) cascade = 1;
    else if (absDepth < ubo.cascadeSplitDepths.z) cascade = 2;

    return SampleShadowPCF(cascade, worldPos, bias);
}

void main() {
    vec4 albedoSamp = texture(albedoMap, fragTexCoord);
    vec3 albedo     = pow(albedoSamp.rgb * fragColor.rgb, vec3(2.2));
    vec2 mr         = texture(mrMap, fragTexCoord).bg;
    float metallic  = mr.x;
    float roughness = max(mr.y, 0.05);
    float ao        = texture(aoMap, fragTexCoord).r;
    vec3 emissive   = texture(emissiveMap, fragTexCoord).rgb;

    vec3 ns  = texture(normalMap, fragTexCoord).rgb * 2.0 - 1.0;
    mat3 TBN = mat3(fragT, fragB, fragN);
    vec3 N   = normalize(TBN * ns);
    vec3 V   = normalize(ubo.cameraPosition.xyz - fragWorldPos);

    vec3 F0 = mix(vec3(0.04), albedo, metallic);
    vec3 Lo = vec3(0.0);

    float shadow = 1.0;

    if (ubo.lightColor.w > 0.0) {
        vec3 L = normalize(-ubo.lightDirection.xyz);
        vec3 H = normalize(V+L);
        vec3 rad = ubo.lightColor.rgb * ubo.lightColor.w;

        float NDF = DistributionGGX(N, H, roughness);
        float G   = GeometrySmith(N, V, L, roughness);
        vec3  F   = FresnelSchlick(max(dot(H,V),0.0), F0);

        vec3 kD  = (1.0-F)*(1.0-metallic);
        float NdL = max(dot(N,L), 0.0);
        vec3 spec = (NDF*G*F)/(4.0*max(dot(N,V),0.0)*NdL+0.0001);

        shadow = ComputeShadow(fragWorldPos, fragViewPos.w);

        Lo += (kD*albedo/PI + spec)*rad*NdL * shadow;
    }

    vec3 ambient = ubo.ambientColor.rgb * ubo.ambientColor.w * albedo * ao;
    vec3 col     = ambient + Lo + emissive;
    col = col/(col+vec3(1.0));
    col = pow(col, vec3(1.0/2.2));
    outColor = vec4(col, albedoSamp.a * fragColor.a);
}
