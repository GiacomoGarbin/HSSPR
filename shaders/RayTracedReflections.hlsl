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
    
    // float t = 0;
    // float2 barycentricCoords = 0;
    int materialIndex = -1;
    float3 hitWorldPos = 0;
    float3 normal = 0;
    float2 uv = 0;
    float3 tangent = 0;

    // if (!RayTraced(worldPos, rayDir, rayDirInv, t, barycentricCoords))
    if (!RayTraced(worldPos, rayDir, rayDirInv, materialIndex, hitWorldPos, normal, uv, tangent))
    {
        discard;
    }

//     MaterialData material = gMaterialBuffer[materialIndex];
    
// 	float3 toEye = gEyePosition - worldPos; // should use world pos of reflected triangle?
// 	const float distToEye = length(toEye);
// 	toEye /= distToEye;
    
// 	float4 diffuse = material.diffuse;
    
// #if ALPHA_TEST
// 	clip(diffuse.a - 0.1f);
// #endif // ALPHA_TEST

// 	const float4 ambient = gAmbientLight * diffuse;

// 	const float shininess = 1.0f - material.roughness;

// 	const Material mat = { diffuse, material.fresnel, shininess };
    
// 	const float3 shadow = 1.0f;
    
// 	const float4 direct = ComputeLighting(gLights, mat, worldPos /* dontcare */, normal, toEye, shadow);
	
// 	float4 result = ambient + direct;

//     // specular reflections
// 	{
// 		const float3 r = reflect(-toEye, normal);
// 		// const float4 reflection = gCubeMap.Sample(gSamplerLinearWrap, r);
// 		const float3 fresnel = SchlickFresnel(material.fresnel, normal, r);
// 		// result.rgb += shininess * fresnel * reflection.rgb;
// 		result.rgb += shininess * fresnel;
// 	}
    
// 	result.a = diffuse.a;

    VertexOut1 pixel;
	pixel.position = 0; // don't care
	pixel.world    = hitWorldPos;
	pixel.normal   = normal;
	pixel.uv       = uv;
	pixel.tangent  = tangent;

    const float4 result = DefaultImpl(pixel, materialIndex);

    return float4(result.rgb, result.a * 0.25f);
}