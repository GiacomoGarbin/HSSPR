#define BACKFACE_CULLING 0 // enable for reflections
#include "RayTracedCommon.hlsl"

cbuffer RayTracedShadowsCB : register(b1)
{
    float3   lightDir;
    float    padding0;
    float3   lightDirInv;
    float    padding1;
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

	bool collision = false;
	int offsetToNextNode = 1;
    

    float4 clipPos = float4(2 * pin.uv - 1, depth, 1);
    clipPos.y = -clipPos.y;

    float4 worldPos = mul(viewProjInv, clipPos);
    worldPos.xyz /= worldPos.w;

    float3 rayDir = lightDir;
    float3 rayDirInv = lightDirInv;

#if NORMALS
    // offset to avoid selfshadows
    worldPos.xyz += 5 * normal;
#endif

    // fake normal
    {
        float3 a = ddx(worldPos.xyz);
        float3 b = ddy(worldPos.xyz);
        float3 n = normalize(cross(a, b));

        const float NdotL = dot(n, lightDir);
        if (NdotL <= 0)
        {
            // do not raytrace for surfaces that point away from the light
            discard;
        }

        worldPos.xyz += 0.005f * n;
    }

    int dataOffset = 0;

    [loop]
    while (offsetToNextNode != 0)
    {
        float4 element0 = BVH[dataOffset++];
        float4 element1 = BVH[dataOffset++];

        offsetToNextNode = int(element0.w);

        collision = false;

        if (offsetToNextNode < 0) // node
        {

            // check for intersection with node AABB
            collision = RayBoxIntersect(worldPos.xyz, rayDirInv, element0.xyz, element1.xyz);

            // if there is collision, go to the next node (left) 
            if (!collision)
            {
                // or else skip over the whole branch (right)
                dataOffset += abs(offsetToNextNode);
            }
        }
        else if (offsetToNextNode > 0) // leaf
        {

            float4 element2 = BVH[dataOffset++];

            float t = 0;
            float2 barycentricCoords = 0;

            // check for intersection with leaf triangle
            collision = RayTriIntersect(worldPos.xyz, rayDir, element0.xyz, element1.xyz, element2.xyz, t, barycentricCoords);

            if (collision)
            {
                // break;
                return float4(0,0,0,0.25f);
            }
        }
    }
    
    discard;
    return 0;
}