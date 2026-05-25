#ifndef OPENPBR_SURFACE_HLSLI
#define OPENPBR_SURFACE_HLSLI

#include "Common.hlsli"
#include "DiffuseBrdf.hlsli"
#include "MetalBrdf.hlsli"
// #include "SpecularBrdf.hlsli"

// 各LobeのID
#define ID_META_BRDF 0
#define ID_SPEC_BRDF 1
#define ID_DIFF_BRDF 2
// TODO: Fluor用のID
#define NUM_BASE_LOBES 3

struct LobeWeights
{
    float3 metal;
    float3 specular;
    float3 diffuse;
};

struct LobeAlbedos
{
    float3 metal;
    float3 specular;
    float3 diffuse;
};

struct LobeProbs
{
    float metal;
    float specular;
    float diffuse;
};

struct LobePDFs
{
    float metal;
    float specular;
    float diffuse;
};

// Lobeでの重みとアルベドの計算
void CalcLobeWeights(in MaterialParam mat, in float3 wi, in float3 norm, out LobeWeights weights, out LobeAlbedos albedos, inout uint seed)
{
    float metal = mat.baseMetalness;
    bool isMetallic = (metal > 0.0);
    bool isFullyMetallic = (metal >= 1.0);

    // アルベドの算出
    albedos.metal = isMetallic ? MetalBrdfAlbedo(mat, wi, norm, seed) : 0.0f;
    // TODO : Speculatの実装
    // albedos.specular = !fully_metallic ? Specular_BrdfAlbedo(mat, wi, norm, seed) : float3(0, 0, 0);
    albedos.specular = 0.0f;
    albedos.diffuse = !isFullyMetallic ? DiffuseBrdfAlbedo(mat, wi) : 0.0f;

    // Slabの合成
    // Metal BRDF weight
    weights.metal = metal;

    // dielectricな割合
    float3 dielectric = max(0.0, 1.0 - metal);

    // Specular BRDF
    weights.specular = dielectric;

    // Diffuse BRDF
    weights.diffuse = dielectric * (1.0f - albedos.specular);
}

// アルベドに応じて各Lobeが選ばれる確率を算出
void CalcLobeProb(in LobeWeights weights, in LobeAlbedos albedos, out LobeProbs probs)
{
    float metal = length(weights.metal * albedos.metal);
    float specular = length(weights.specular * albedos.specular);
    float diffuse = length(weights.diffuse * albedos.diffuse);

    float total = max(metal + specular + diffuse, EPS);

    probs.metal = metal / total;
    probs.specular = specular / total;
    probs.diffuse = diffuse / total;
}

// OpenPBRローブの評価
float3 EvaluateOpenPBRLobes(in MaterialParam mat, in LobeWeights weights, in LobeProbs probs, in float3 wi, in float3 wo, in float3 norm, int skipLobeid, inout LobePDFs pdfs)
{
    float3 lobeBsdf = float3(0, 0, 0);
    // Metal BRDF
    if (skipLobeid != ID_META_BRDF && probs.metal > EPS)
    {
        float pdf = EPS;
        float3 brdf = EvaluateMetalBrdf(mat, wi, wo, pdf);
        lobeBsdf += weights.metal * brdf;
        pdfs.metal = pdf;
    }
    else
    {
        pdfs.metal = EPS;
    }

    // TOOD: Specular BRDFの実装
    // if (skipLobeid != ID_SPEC_BRDF && probs.specular > EPS)
    // {
    //     float pdf_specular = EPS;
    //     float3 f_specular = Specular_EvaluateBrdf(mat, wi, wo, pdf_specular);
    //     f += weights.specular * f_specular;
    //     pdfs.specular = pdf_specular;
    // }
    // else
    // {
    //     pdfs.specular = EPS;
    // }
    pdfs.specular = EPS;

    // Diffuse BRDF
    if (skipLobeid != ID_DIFF_BRDF && probs.diffuse > EPS)
    {
        float pdf = EPS;
        // TODO: EFONへの置き換え
        float3 brdf = EvaluateLambertDiffuseBrdf(mat.baseColor, wi, wo, norm, pdf);
        lobeBsdf += weights.diffuse * brdf;
        pdfs.diffuse = pdf;
    }
    else
    {
        pdfs.diffuse = EPS;
    }

    return lobeBsdf;
}

float TotalLobePdf(in LobeProbs probs, in LobePDFs pdfs)
{
    return probs.metal * pdfs.metal +
           probs.specular * pdfs.specular +
           probs.diffuse * pdfs.diffuse;
}

// OpenPBR BSDFの評価
float3 EvaluateOpenPBRBsdf(in MaterialParam mat, in float3 wi, in float3 wo, in float3 norm, in LobeWeights weights, in LobeProbs probs, out float pdf)
{
    LobePDFs pdfs;
    float3 bsdf = EvaluateOpenPBRLobes(mat, weights, probs, wi, wo, norm, -1, pdfs);
    pdf = TotalLobePdf(probs, pdfs);
    return bsdf;
}

// OpenPBR BSDFのサンプル
float3 SampleOpenPBRBsdf(in MaterialParam mat, in float3 wi, out float3 wo, in Basis basis, in LobeWeights weights, in LobeProbs probs, out float pdf, inout uint seed)
{
    float r = Rand(seed);
    float cdf = 0.0;
    int sampledLobeId = -1;
    float lobePdf = EPS;
    float3 lobeBsdf = 0.0f;

    // Metal BRDFのサンプル
    cdf += probs.metal;
    if (r < cdf && sampledLobeId < 0)
    {
        sampledLobeId = ID_META_BRDF;
        lobeBsdf = SampleMetalBrdf(mat, wi, wo, lobePdf, seed);
    }

    // TODO: Specular BRDFのサンプル
    // cdf += probs.specular;
    // if (r < cdf && sampledLobeId < 0)
    // {
    //     sampledLobeId = ID_SPEC_BRDF;
    //     f_lobe = Specular_SampleBrdf(mat, wi, wo, pdf_lobe, seed);
    // }

    // Diffuse BRDFのサンプル
    cdf += probs.diffuse;
    if (r < cdf && sampledLobeId < 0)
    {
        sampledLobeId = ID_DIFF_BRDF;
        // TODO: EFONに置き換え
        lobeBsdf = SampleLambertDiffuseBrdf(mat.baseColor, wi, wo, basis, lobePdf, seed);
    }

    // どのローブもサンプルされなかった場合
    if (sampledLobeId < 0)
    {
        pdf = 1.0;
        wo = float3(0, 0, 1);
        return float3(0, 0, 0);
    }

    // サンプルされた方向に対するローブの評価
    LobePDFs pdfs;
    float3 bsdf = EvaluateOpenPBRLobes(mat, weights, probs, wi, wo, basis.normal, sampledLobeId, pdfs);
    bsdf += weights.metal * (sampledLobeId == ID_META_BRDF ? lobeBsdf : 0.0f);
    bsdf += weights.specular * (sampledLobeId == ID_SPEC_BRDF ? lobeBsdf : 0.0f);
    bsdf += weights.diffuse * (sampledLobeId == ID_DIFF_BRDF ? lobeBsdf : 0.0f);

    // PDFの計算
    if (sampledLobeId == ID_META_BRDF) pdfs.metal = lobePdf;
    else if (sampledLobeId == ID_SPEC_BRDF) pdfs.specular = lobePdf;
    else if (sampledLobeId == ID_DIFF_BRDF) pdfs.diffuse = lobePdf;
    pdf = TotalLobePdf(probs, pdfs);

    return bsdf;
}

#endif // OPENPBR_SURFACE_HLSLI
