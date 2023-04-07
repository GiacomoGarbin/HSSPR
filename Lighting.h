#pragma once

// d3d
#include <directxmath.h>
using namespace DirectX;

//
#include "Timer.h"

#define LIGHT_MAX_COUNT 16

class Lighting
{
public:

    Lighting() {};

    struct Light
    {
        XMFLOAT3 strength = XMFLOAT3(0.5f, 0.5f, 0.5f);
        float    falloffStart = 1.0f;
        XMFLOAT3 direction = XMFLOAT3(0.0f, -1.0f, 0.0f);
        float    falloffEnd = 10.0f;
        XMFLOAT3 position = XMFLOAT3(0.0f, 0.0f, 0.0f);
        float    spotPower = 64.0f;
    };

    void UpdateLights(const Timer& timer)
    {
        mLightRotationAngle += 0.1f * timer.GetDeltaTime();

        XMMATRIX R = XMMatrixRotationY(mLightRotationAngle);
        for (int i = 0; i < 3; ++i)
        {
            XMVECTOR dir = XMLoadFloat3(&mBeginLightDirections[i]);
            dir = XMVector3TransformNormal(dir, R);
            XMStoreFloat3(&mCurrentLightDirections[i], dir);
        }
    }

    const XMFLOAT3& GetLightDirection(const size_t i) const { return mCurrentLightDirections[i]; }

private:

    float mLightRotationAngle = 0.0f;
    XMFLOAT3 mBeginLightDirections[3] =
    {
        XMFLOAT3(+0.57735f, -0.57735f, 0.57735f),
        XMFLOAT3(-0.57735f, -0.57735f, 0.57735f),
        XMFLOAT3( 0.0f,     -0.707f,  -0.707f)
    };
    XMFLOAT3 mCurrentLightDirections[3];
};