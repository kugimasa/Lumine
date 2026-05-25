#ifndef METAL_BRDF_HLSLI
#define METAL_BRDF_HLSLI

#include "Common.hlsli"

// Fresnel - Schlick近似
float3 FresnelSchlick(float3 F0, float mu)
{
    return F0 + (1.0f - F0) * pow(1.0 - mu, 5.0);
}

// Fresnel - F82 Tint [Kutz2021]
float3 FresnelF82Tint(float mu, in float3 F0, in float3 F82tint)
{
    const float mu_bar = 1.0 / 7.0;
    const float numerator = mu * pow(1.0 - mu, 6.0);
    const float denominator = mu_bar * pow(1.0 - mu_bar, 6.0);
    float3 Fschlick_bar = FresnelSchlick(F0, mu_bar);
    float3 Fschlick = FresnelSchlick(F0, mu);
    return Fschlick - numerator / max(EPS, denominator) * (1.0f - F82tint) * Fschlick_bar;
}

// GGX法線分布関数
float GGX_NDF(in float3 m, in float alphaX, in float alphaY)
{
    float mx2 = (m.x * m.x) / max(alphaX * alphaX, EPS);
    float my2 = (m.y * m.y) / max(alphaY * alphaY, EPS);
    float mz2 = m.z * m.z;
    float term = mx2 + my2 + mz2;
    float denominator = PI * alphaX * alphaY * term * term;
    return 1.0 / max(denominator, EPS);
}

// GGXラムダ関数
float GGX_Lambda(in float3 w, float alphaX, float alphaY)
{
    float z2 = pow(w.z, 2.0f);
    float ax2 = pow(alphaX * w.x, 2.0f);
    float ay2 = pow(alphaY * w.y, 2.0f);
    float a2 = (ax2 + ay2) / max(z2, EPS);
    return (-1.0 + sqrt(1.0 + a2)) / 2.0;
}

// 片方向シャドウマスキング関数 - G1
float GGX_G1(in float3 w, float alphaX, float alphaY)
{
    return 1.0 / (1.0 + GGX_Lambda(w, alphaX, alphaY));
}

// Height-Correlated シャドウマスキング関数 - G2
float GGX_G2(in float3 wo, in float3 wi, float alphaX, float alphaY)
{
    return 1.0 / (1.0 + GGX_Lambda(wo, alphaX, alphaY) + GGX_Lambda(wi, alphaX, alphaY));
}

// Sample GGX visible normal distribution (VNDF) [Dupuy2023]
// reference: https://arxiv.org/pdf/2306.05044
float3 SampleVndf_GGX(in float3 wiL, float alphaX, float alphaY, inout uint seed)
{
    float2 Xi = float2(Rand(seed), Rand(seed));
    float3 V = wiL;
    float2 alpha = float2(alphaX, alphaY);

    // 半球空間に変換
    V = normalize(float3(V.xy * alpha, V.z));

    // (-wi.z, 1] の範囲の球冠(Spherical Cap)からサンプル
    float phi = 2.0 * PI * Xi.x;
    float z = (1.0 - Xi.y) * (1.0 + V.z) - V.z;
    float sinTheta = sqrt(saturate(1.0f - z * z));
    float x = sinTheta * cos(phi);
    float y = sinTheta * sin(phi);
    float3 c = float3(x, y, z);

    // Halfway方向
    float3 H = c + V;

    // 楕円空間に戻す
    H = normalize(float3(H.xy * alpha, H.z));
    return H;
}

// 異方性反射のための回転角
struct LocalFrameRotation
{
    float cos_angle;
    float sin_angle;
};

LocalFrameRotation GetLocalFrameRotation(float angle)
{
    LocalFrameRotation rot;
    rot.cos_angle = cos(angle);
    rot.sin_angle = sin(angle);
    return rot;
}

float3 LocalToRotated(float3 v, LocalFrameRotation rot)
{
    float2 xy_rotated = float2(
        rot.cos_angle * v.x + rot.sin_angle * v.y,
        -rot.sin_angle * v.x + rot.cos_angle * v.y
    );
    return float3(xy_rotated, v.z);
}

float3 RotatedToLocal(float3 v, LocalFrameRotation rot)
{
    float2 xy_local = float2(
        rot.cos_angle * v.x - rot.sin_angle * v.y,
        rot.sin_angle * v.x + rot.cos_angle * v.y
    );
    return float3(xy_local, v.z);
}

// 異方性ラフネス値の計算
void CalcAnisotropicRoughness(float roughness, float anisotropy, out float alphaX, out float alphaY)
{
    float rsqr = roughness * roughness;
    alphaX = rsqr * sqrt(2.0 / (1.0 + pow(1.0 - anisotropy, 2.0)));
    alphaY = (1.0 - anisotropy) * alphaX;

    // 最小値でclamp
    const float minAlpha = 1.0e-4;
    alphaX = max(minAlpha, alphaX);
    alphaY = max(minAlpha, alphaY);
}

// Metal BRDF の評価
float3 EvaluateMetalBrdf(in MaterialParam mat, in float3 wi, in float3 wo, inout float pdf)
{
    // 異方性ラフネス値の算出
    float alphaX, alphaY;
    CalcAnisotropicRoughness(mat.specularRoughness, mat.specularAnisotropy, alphaX, alphaY);

    // 回転の適用
    float rotation = 0.0;
    LocalFrameRotation rot = GetLocalFrameRotation(2.0 * PI * rotation);
    float3 wiR = LocalToRotated(wi, rot);
    float3 woR = LocalToRotated(wo, rot);

    // ハーフベクトル
    float3 mR = normalize(woR + wiR);

    // NDFとvNDFの算出
    float D = GGX_NDF(mR, alphaX, alphaY);
    float DV = D * GGX_G1(wiR, alphaX, alphaY) * max(0.0, dot(wiR, mR)) / max(EPS, wiR.z);

    // フレネル項 - F82-tint
    float3 F0 = mat.baseWeight * mat.baseColor;
    float3 F82 = FresnelF82Tint(abs(dot(wiR, mR)), F0, mat.specularColor);
    float3 F_metal = mat.specularWeight * F82;

    // シャドウマスキング関数 - G2
    float G2 = GGX_G2(wiR, woR, alphaX, alphaY);

    // BRDFを評価する
    return F_metal * D * G2 / max(4.0 * abs(wo.z) * abs(wi.z), EPS);
}

float3 SampleMetalBrdf(in MaterialParam mat, in float3 wi, out float3 wo, out float pdf, inout uint seed)
{
    // 異方性ラフネス値の算出
    // dielectric/specularとNDFを共有する
    float alphaX, alphaY;
    CalcAnisotropicRoughness(mat.specularRoughness, mat.specularAnisotropy, alphaX, alphaY);

    // 回転の適用
    float rotation = 0.0;
    LocalFrameRotation rot = GetLocalFrameRotation(2.0 * PI * rotation);
    float3 wiR = LocalToRotated(wi, rot);

    // NDFのサンプル
    float3 mR = SampleVndf_GGX(wiR, alphaX, alphaY, seed);

    // 反射方向の計算
    float3 woR = -wiR + 2.0 * dot(wiR, mR) * mR;

    // MEMO: 多重散乱の実装に合わせて修正が必要
    if (wiR.z * woR.z < EPS)
    {
        pdf = 1.0;
        wo = float3(0, 0, 1);
        return float3(0, 0, 0);
    }

    // ローカル空間に戻す
    wo = RotatedToLocal(woR, rot);

    // MEMO: 以降はEvaluateMetalBrdfとまとめられそう
    // NDFとvNDF
    float D = GGX_NDF(mR, alphaX, alphaY);
    float DV = D * GGX_G1(wiR, alphaX, alphaY) * max(0.0, dot(wiR, mR)) / max(EPS, wiR.z);

    // 出射方向のサンプリングに対するPDFを算出
    float dwh_dwo = 1.0 / max(abs(4.0 * dot(wiR, mR)), EPS);
    pdf = max(EPS, DV * dwh_dwo);

    // フレネル項 - F82-tint
    float3 F0 = mat.baseWeight * mat.baseColor;
    float3 F82 = FresnelF82Tint(abs(dot(wiR, mR)), F0, mat.specularColor);
    float3 F_metal = mat.specularWeight * F82;

    // シャドウマスキング関数 - G2
    float G2 = GGX_G2(wiR, woR, alphaX, alphaY);

    // BRDFを評価する
    return F_metal * D * G2 / max(4.0 * abs(wo.z) * abs(wi.z), EPS);
}

float3 MetalBrdfAlbedo(in MaterialParam mat, in float3 wi, in float3 norm, inout uint seed)
{
    if (wi.z < EPS)
    {
        return float3(0, 0, 0);
    }
    // アルベド算出
    float3 wo;
    float pdf;
    float3 brdf = SampleMetalBrdf(mat, wi, wo, pdf, seed);
    return brdf * CalcCos(wo, norm) / max(EPS, pdf);
}

#endif // METAL_BRDF_HLSLI
