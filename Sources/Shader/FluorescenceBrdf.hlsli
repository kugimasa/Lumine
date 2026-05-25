#ifndef FLUORESCENCE_BRDF_HLSLI
#define FLUORESCENCE_BRDF_HLSLI

#include "Common.hlsli"

float3 Linear_to_Gamma(float3 color)
{
    color = saturate(color);
    float3 a = color * 12.92f;
    float3 b = 1.055f * pow(color, 0.41666f) - 0.055f;
    return lerp(a, b, step(0.0031308f, color));
}

float3 Gamma_to_Linear(float3 color)
{
    color = saturate(color);
    float3 a = color / 12.92f;
    float3 b = pow((color + 0.055f) / 1.055f, 2.4f);
    return lerp(a, b, step(0.04045, color));
}

float3 RGB_to_XYZ(float3 rgb)
{
    const static float3x3 RGB_TO_XYZ_MAT = float3x3(
        0.4124, 0.3576, 0.1805,
        0.2126, 0.7152, 0.0722,
        0.0193, 0.1192, 0.9505
    );
    return mul(RGB_TO_XYZ_MAT, rgb);
}

float3 XYZ_to_RGB(float3 xyz)
{
    const float3x3 XYZ_TO_RGB_MAT = float3x3(
        3.2406, -1.5372, -0.4986,
        -0.9689,  1.8758,  0.0415,
        0.0557, -0.2040,  1.0570
    );
    return mul(XYZ_TO_RGB_MAT, xyz);
}

float3 XYZ_to_sRGBGamma(float3 xyz, float EV=0)
{
    float3 exposedXYZ = xyz * pow(2.0, EV);
    float3 linearRGB = Linear_to_Gamma(exposedXYZ);
    return XYZ_to_RGB(linearRGB);
}

// Non-Orthogonal Reduction for Rendering Fluorescent Materials in Non-Spectral Engines [Fichet 2024]
float3 TEXTYELL_ReductionXYZ(float3 xyz)
{
    const float3x3 TEXTYELL = float3x3(
        0.73829955, 0.0331047, -0.04395359,
        0.02196004, 0.74096781, 0.17854782,
        -0.04524984, 0.06191856, 0.10331458
    );
    return mul(TEXTYELL, xyz);
}
#endif // FLUORESCENCE_BRDF_HLSLI