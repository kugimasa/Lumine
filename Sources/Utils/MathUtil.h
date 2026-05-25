#pragma once

#include <DirectXMath.h>

using namespace DirectX;
using Float2 = DirectX::XMFLOAT2;
using Float3 = DirectX::XMFLOAT3;
using Float4 = DirectX::XMFLOAT4;
using Matrix = DirectX::XMMATRIX;
using Mtx3x4 = DirectX::XMFLOAT3X4;
using Mtx4x3 = DirectX::XMFLOAT4X3;
using Mtx4x4 = DirectX::XMFLOAT4X4;
using Vector = DirectX::XMVECTOR;

static Vector ZeroVector()
{
    return XMVectorZero();
}

static Vector IdentityQuat()
{
    return XMQuaternionIdentity();
}

static Vector Vector4(float x, float y, float z, float w)
{
    return XMVectorSet(x, y, z, w);
}

static Vector DoubleToVector3(const double* v)
{
    Float3 vec3;
    vec3.x = static_cast<float>(v[0]);
    vec3.y = static_cast<float>(v[1]);
    vec3.z = static_cast<float>(v[2]);
    return XMLoadFloat3(&vec3);
}

static Vector DoubleToVector4(const double* v)
{
    Float4 vec4;
    vec4.x = static_cast<float>(v[0]);
    vec4.y = static_cast<float>(v[1]);
    vec4.z = static_cast<float>(v[2]);
    vec4.w = static_cast<float>(v[3]);
    return XMLoadFloat4(&vec4);
}

static Matrix IdentityMtx()
{
    return XMMatrixIdentity();
}

static float Lerp(float a, float b, float t)
{
    return a + (b - a) * t;
}

// https://easings.net/#easeInOutQuad
static float EaseInOutQuad(float t)
{
    return t < 0.5 ? 2.0 * t * t : 1.0 - pow(-2.0 * t + 2.0, 2.0) / 2.0;
}

// https://easings.net/#easeOutQuart
static float EaseOutQuart(float t)
{
    return 1.0 - pow(1.0 - t, 4.0);
}

#ifndef ROUND_UP
#define ROUND_UP(size, align) (((size) + (align) - 1) & ~((align) - 1))
#endif

#ifndef M_PI
#define M_PI 3.1415926535f
#endif

#ifndef M_INV_PI
#define M_INV_PI 1.0f / M_PI
#endif

#ifndef M_DEG2RAD
#define M_DEG2RAD  M_PI / 180.0f
#endif

#ifndef M_RAD2DEG
#define M_RAD2DEG  180.0f / M_PI
#endif
