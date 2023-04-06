cbuffer MainPassCB : register(b0)
{
	// float4x4 gView;
	// float4x4 gViewInverse;
	// float4x4 gProj;
	// float4x4 gProjInverse;
	float4x4 gViewProj;
// 	float4x4 gViewProjInverse;
// 	float4x4 gShadowMapTransform;
// 	float3 gEyePositionW;
// 	float padding1;
// 	float2 gRenderTargetSize;
// 	float2 gRenderTargetSizeInverse;
// 	float gNearPlane;
// 	float gFarPlane;
// 	float gDeltaTime;
// 	float gTotalTime;

// 	float4 gAmbientLight;

// #ifdef FOG
// 	float4 gFogColor;
// 	float gFogStart;
// 	float gFogRange;
// 	float2 padding2;
// #endif // FOG

// 	Light gLights[LIGHT_MAX_COUNT];
};

cbuffer ObjectCB : register(b1)
{
	float4x4 gWorld;
	// float4x4 gTexCoordTransform;
	// uint gMaterialIndex;
	// float3 padding;
};