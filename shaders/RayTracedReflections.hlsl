#define SHADOWS 0
#include "RayTracedCommon.hlsl"

#define FIXME 1
#include "../RenderToyD3D11/shaders/Default.hlsl"

float4 RayTracedReflectionsPS(const VertexOut pin) : SV_Target
{
    const float depth = DepthBuffer.Load(uint3(pin.position.xy, 0));
    const float4 gbuffer = GBuffer.Load(uint3(pin.position.xy, 0));

    const float3 normal = gbuffer.xyz;
    const int materialIndex = gbuffer.w;

    const MaterialData material = gMaterialBuffer[materialIndex];

    if ((depth == 1) || (material.roughness == 1))
    {
        // do not raytrace sky pixels
        // do not raytrace rough surfaces
        discard;
    }
    
    float3 worldPos = GetWorldPos(pin.uv, depth);

    // offset to avoid self shadows
    worldPos += kSelfShadowOffset * normal;

    const float3 rayDir = reflect(worldPos - gEyePosition, normal);
    const float3 rayDirInv = 1 / rayDir;

    HitPoint hitPoint;
    
    if (!RayTraced(worldPos, rayDir, rayDirInv, hitPoint))
    {
        discard;
    }

    DefaultVSOut pixel;
	pixel.position = 0; // don't care
	pixel.world    = hitPoint.worldPos;
	pixel.normal   = hitPoint.normal;
	pixel.uv       = hitPoint.uv;
	pixel.tangent  = hitPoint.tangent;

    const float4 result = DefaultImpl(pixel, hitPoint.materialIndex);

    return float4(result.rgb, result.a * 0.25f); // TODO: alpha based on roughness
}