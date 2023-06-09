#define RAYTRACED_SHADOWS 0
#define RAYTRACED_REFLECTIONS 1
#include "RayTracedCommon.hlsl"

#define FIXME 1
#define SHADOW_MAPPING 0
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

#if FAKE_NORMAL
	const float3 e0 = ddx(pixel.world);
	const float3 e1 = ddy(pixel.world);
	pixel.normal = normalize(cross(e0, e1));
#endif // FAKE_NORMAL

    const float4 result = DefaultImpl(pixel, hitPoint.materialIndex);

    return float4(result.rgb, result.a /** material.roughness*/); // TODO: alpha based on roughness
}