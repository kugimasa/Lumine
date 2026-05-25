#ifndef DIFFUSE_BRDF_HLSLI
#define DIFFUSE_BRDF_HLSLI

#include "Common.hlsli"

float3 EvaluateLambertDiffuseBrdf(in float3 baseColor, in float3 wi, in float3 wo, in float3 norm, out float pdf)
{
    pdf = HemisphereCosPdf(wo, norm);
    return baseColor * INV_PI;
}

float3 SampleLambertDiffuseBrdf(in float3 baseColor,  in float3 wi, out float3 wo, in Basis basis, out float pdf, in uint seed)
{
    wo = SampleHemisphereCos(basis, seed);
    return EvaluateLambertDiffuseBrdf(baseColor, wi, wo, basis.normal ,pdf);
}

float3 DiffuseBrdfAlbedo(in MaterialParam mat, in float3 wi)
{
    if (wi.z < EPS)
    {
        return float3(0, 0, 0);
    }
    return mat.baseColor * mat.baseWeight;
}
#endif // DIFFUSE_BRDF_HLSLI