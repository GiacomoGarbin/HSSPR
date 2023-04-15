#define BACKFACE_CULLING 1
#define SHADOWS 0
#include "RayTracedCommon.hlsl"

#define FIXME 1
#include "../RenderToyD3D11/shaders/Default.hlsl"

float4 RayTracedReflectionsPS(const VertexOut pin) : SV_Target
{
#if NORMALS
    const float4 value = DepthNormalBuffer.Load(uint3(pin.position.xy, 0));
    const float depth = value.w;
    const float normal = value.xyz;
#else
    const float depth = DepthBuffer.Load(uint3(pin.position.xy, 0));
#endif

    if (depth == 1)
    {
        // do not raytrace sky pixels
        discard;
    }
    
    float3 worldPos = GetWorldPos(pin.uv, depth);

#if !NORMALS
    // fake normal
    float3 a = ddx(worldPos);
    float3 b = ddy(worldPos);
    float3 n = normalize(cross(a, b));
#endif

    // offset to avoid selfshadows
    worldPos += 0.001f * n;

    // const float3 rayDir = reflect(worldPos - gEyePosition, float3(0,1,0));
    const float3 rayDir = reflect(worldPos - gEyePosition, n);
    const float3 rayDirInv = 1 / rayDir;
    
    int materialIndex = -1;
    float3 hitWorldPos = 0;
    float3 normal = 0;
    float2 uv = 0;
    float3 tangent = 0;
    
    if (!RayTraced(worldPos, rayDir, rayDirInv, materialIndex, hitWorldPos, normal, uv, tangent))
    {
        discard;
    }

    VertexOut1 pixel;
	pixel.position = 0; // don't care
	pixel.world    = hitWorldPos;
	pixel.normal   = normal;
	pixel.uv       = uv;
	pixel.tangent  = tangent;

    const float4 result = DefaultImpl(pixel, materialIndex);

    return float4(result.rgb, result.a * 0.25f);
}