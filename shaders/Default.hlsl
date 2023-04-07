#include "Common.hlsl"

struct VertexIn
{
	float3 position : POSITION;
	float3 normal   : NORMAL;
	float2 uv       : TEXCOORD;
	float3 tangent  : TANGENT;
};

struct VertexOut
{
	float4 position : SV_POSITION;
	float3 world    : POSITION0;
	float3 normal   : NORMAL;
	float2 uv       : TEXCOORD;
	float3 tangent  : TANGENT;
};

VertexOut DefaultVS(VertexIn vin)
{
	VertexOut vout;

// 	const MaterialData material = gMaterialBuffer[gMaterialIndex];
	
// #ifdef SKINNED
//     float weights[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
//     weights[0] = vin.BoneWeights.x;
//     weights[1] = vin.BoneWeights.y;
//     weights[2] = vin.BoneWeights.z;
//     weights[3] = 1.0f - weights[0] - weights[1] - weights[2];

//     float3 PositionL = float3(0.0f, 0.0f, 0.0f);
//     float3 NormalL = float3(0.0f, 0.0f, 0.0f);
//     float3 TangentL = float3(0.0f, 0.0f, 0.0f);

//     for(int i = 0; i < 4; ++i)
//     {
// 		const float4x4 BoneTransform = gBoneTransforms[vin.BoneIndices[i]];

//         PositionL += weights[i] * mul(float4(vin.PositionL, 1.0f), BoneTransform).xyz;
//         NormalL += weights[i] * mul(vin.NormalL, (float3x3)(BoneTransform));
//         TangentL += weights[i] * mul(vin.TangentL.xyz, (float3x3)(BoneTransform));
//     }

//     vin.PositionL = PositionL;
//     vin.NormalL = NormalL;
//     vin.TangentL.xyz = TangentL;
// #endif // SKINNED

	vout.world = mul(gWorld, float4(vin.position, 1.0f)).xyz;
	vout.position = mul(gViewProj, float4(vout.world, 1.0f));

	vout.normal = mul((float3x3)(gWorld), vin.normal);
	vout.tangent = mul((float3x3)(gWorld), vin.tangent);

	// const float4 TexCoord = mul(float4(vin.TexCoord, 0.0f, 1.0f), gTexCoordTransform);
	// vout.TexCoord = mul(TexCoord, material.transform).xy;
	
	vout.uv = vin.uv;

	// // projective tex-coords to project shadow map onto scene
	// vout.ShadowPositionH = mul(PositionW, gShadowMapTransform);

	return vout;
}

float4 DefaultPS(const VertexOut pin) : SV_Target
{
	const MaterialData object = gMaterialBuffer[gMaterialIndex];

	float4 diffuse = object.diffuse;
	// diffuse *= gDiffuseTexture[material.DiffuseTextureIndex].Sample(gSamplerLinearWrap, pin.TexCoord);

#if ALPHA_TEST
	clip(diffuse.a - 0.1f);
#endif // ALPHA_TEST

// #if NORMAL_MAPPING
// 	const float4 NormalSample = gDiffuseTexture[material.NormalTextureIndex].Sample(gSamplerLinearWrap, pin.TexCoord);
// 	const float3 normal = NormalSampleToWorldSpace(NormalSample.rgb, normalize(pin.NormalW), pin.TangentW);
// #else // NORMAL_MAPPING
	const float3 normal = normalize(pin.normal);
// #endif // NORMAL_MAPPING

	float3 toEye = gEyePosition - pin.world;
	const float distToEye = length(toEye);
	toEye /= distToEye;

// 	// indirect lighting
// #if AMBIENT_OCCLUSION || 1
// 	const float2 TexCoord = pin.PositionH.xy * gRenderTargetSizeInverse;
// 	const float AmbientAccess = gAmbientOcclusionMap.Sample(gSamplerLinearClamp, TexCoord, 0.0f).r;
// 	const float4 ambient = gAmbientLight * DiffuseAlbedo * AmbientAccess;
// #else // AMBIENT_OCCLUSION
	const float4 ambient = gAmbientLight * diffuse;
// #endif // AMBIENT_OCCLUSION
	
// 	// direct lighting
// #if NORMAL_MAPPING
// 	const float shininess = (1.0f - material.roughness) * NormalSample.a;
// #else // NORMAL_MAPPING
	const float shininess = 1.0f - object.roughness;
// #endif // NORMAL_MAPPING
	const Material material = { diffuse, object.fresnel, shininess };
// #if SHADOW || 1
//     // only the first light casts a shadow
//     float3 ShadowFactor = float3(1.0f, 1.0f, 1.0f);
// 	ShadowFactor[0] = CalculateShadowFactor(pin.ShadowPositionH);
// #else // SHADOW
	const float3 shadow = 1.0f;
// #endif // SHADOW
	const float4 direct = ComputeLighting(gLights, material, pin.world, normal, toEye, shadow);
	
	float4 result = ambient + direct;

	// specular reflections
	{
		const float3 r = reflect(-toEye, normal);
		// const float4 reflection = gCubeMap.Sample(gSamplerLinearWrap, r);
		const float3 fresnel = SchlickFresnel(object.fresnel, normal, r);
		// result.rgb += shininess * fresnel * reflection.rgb;
		result.rgb += shininess * fresnel;
	}

// #if FOG
// 	const float FogAmount = saturate((DistToEye - gFogStart) / gFogRange);
// 	result = lerp(result, gFogColor, FogAmount);
// #endif // FOG

	result.a = diffuse.a;

	return result;
}