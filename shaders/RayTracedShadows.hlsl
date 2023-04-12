#define BACKFACE_CULLING 0
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
#if NORMALS
    const float4 value = DepthNormalBuffer.Load(uint3(pin.position.xy, 0));
    const float depth = value.w;
    const float normal = value.xyz;
#else
    const float depth = DepthBuffer.Load(uint3(pin.position.xy, 0));
#endif

#if NORMALS
	const float NdotL = dot(normal, lightDir);

    if (NdotL <= 0)
    {
        // do not raytrace for surfaces that point away from the light
        discard;
    }
#endif

    if (depth == 1)
    {
        // do not raytrace for sky pixels
        discard;
    }
    
    float3 worldPos = GetWorldPos(pin.uv, depth);

#if NORMALS
    // offset to avoid selfshadows
    worldPos += 5 * normal;
#endif

    // fake normal
    {
        float3 a = ddx(worldPos);
        float3 b = ddy(worldPos);
        float3 n = normalize(cross(a, b));

        const float NdotL = dot(n, lightDir);
        if (NdotL <= 0)
        {
            // do not raytrace for surfaces that point away from the light
            discard;
        }

        // offset to avoid selfshadows
        worldPos += 0.005f * n;
    }

    float3 dontCare = 0;

    if (!RayTraced(worldPos, lightDir, lightDirInv, dontCare.x, dontCare.yz))
    {
        discard;
    }

    return float4(0,0,0,0.25f);
}