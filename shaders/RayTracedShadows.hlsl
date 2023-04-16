#define SHADOWS 1
#include "RayTracedCommon.hlsl"

cbuffer RayTracedShadowsCB : register(b1)
{
    float3   lightDir;
    float    padding0;
    float3   lightDirInv;
    float    padding1x;
};

float4 RayTracedShadowsPS(const VertexOut pin) : SV_Target
{
    const float depth = DepthBuffer.Load(uint3(pin.position.xy, 0));
    const float4 gbuffer = GBuffer.Load(uint3(pin.position.xy, 0));

    const float3 normal = gbuffer.xyz;
    const int materialIndex = gbuffer.w;

	const float NdotL = dot(normal, lightDir);

    if ((depth == 1) || (NdotL <= 0))
    {
        // do not raytrace sky pixels
        // do not raytrace surfaces that point away from the light
        discard;
    }
    
    float3 worldPos = GetWorldPos(pin.uv, depth);

    // offset to avoid self shadows
    worldPos += kSelfShadowOffset * normal;

    if (!RayTraced(worldPos, lightDir, lightDirInv))
    {
        discard;
    }

    return float4(0,0,0,0.25f);
}