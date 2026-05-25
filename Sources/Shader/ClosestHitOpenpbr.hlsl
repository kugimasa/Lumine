#include "Common.hlsli"
#include "OpenpbrSurface.hlsli"

// OpenPBRマテリアル
[shader("closesthit")]
void ClosestHit_OpenPBR(inout HitInfo payload, Attributes attrib)
{
    Vertex vtx = GetHitVertex(attrib);
    float3 worldPos = WorldRayOrigin() + WorldRayDirection() * RayTCurrent();
    float3 worldNorm = mul(vtx.normal, (float3x3)ObjectToWorld4x3());

    // 自己交差を避けるために法線方向にオフセット
    payload.hitPos = worldPos + worldNorm * RAY_EPSILON;

    // 入射方向
    float3 wi = normalize(-WorldRayDirection());
    // 正規直交規定
    float3 norm = normalize(worldNorm);
    Basis basis = MakeBasis(norm);
    float3 wiLocal = WorldToLocal(wi, basis);

    // テクスチャサンプリング
    // ベースカラーの取得
    float3 baseColor = l_matCB.baseColor.rgb * l_diffuseTex.SampleLevel(gSampler, vtx.uv, 0).rgb;

    // マテリアルパラメータの準備
    MaterialParam mat = l_matCB;
    mat.baseColor.rgb = baseColor;

    // ローブの事前準備
    LobeWeights weights;
    LobeAlbedos albedos;
    LobeProbs probs;
    CalcLobeWeights(mat, wiLocal, norm, weights, albedos, payload.seed);
    CalcLobeProb(weights, albedos, probs);

    // Emissionの処理
    if (mat.emissionLuminance > EPS)
    {
        payload.color += payload.attenuation * mat.emissionColor.rgb * mat.emissionLuminance;
    }

    uint totalLights = gSceneParam.numRectLights + gSceneParam.numSphereLights;
    if (totalLights > 0)
    {
        // 光源サンプリング
        float selectionPdf = 1.0f;
        SampledLightInfo lightInfo = SampleAllLights(payload.seed, payload.hitPos, selectionPdf);
        float3 toLight = lightInfo.position - payload.hitPos;
        float3 lightDir = normalize(toLight);
        float lightDist = length(toLight);

        // 光源方向へレイトレースして、光源と接続できた場合に寄与の計算
        if (!TraceShadowRay(payload.seed, payload.hitPos, lightDir, lightDist))
        {
            // 出射方向
            float3 woLocal = WorldToLocal(lightDir, basis);

            // 全体のPDF = 光源選択のPDF × 個別光源のPDF
            float totalLightPdf = selectionPdf * lightInfo.pdf;

            // ヤコビアンの計算
            float cos1 = CalcCos(norm, lightDir);
            float cos2 = CalcCos(lightInfo.normal, -lightDir);
            float G = (cos1 * cos2) / max((lightDist * lightDist), EPS);

            // BSDFの評価
            float bsdfPdf = EPS;
            float3 bsdf = EvaluateOpenPBRBsdf(mat, wiLocal, woLocal, norm, weights, probs, bsdfPdf);

            // MISウェイトの計算
            float misWeight = totalLightPdf / max(bsdfPdf + totalLightPdf, EPS);

            // NEEによる寄与計算
            payload.color += payload.attenuation * (bsdf * G / max(totalLightPdf, EPS)) * lightInfo.intensity * misWeight;
        }
    }

    // BSDFに基づく方向サンプリング
    float3 woLocal;
    float samplePdf;
    float3 bsdf = SampleOpenPBRBsdf(mat, wiLocal, woLocal, basis, weights, probs, samplePdf, payload.seed);

    // 寄与の計算
    float3 wo = LocalToWorld(woLocal, basis);
    float cosTheta = CalcCos(norm, wo);
    payload.attenuation *= bsdf * cosTheta / max(samplePdf, EPS);
    payload.reflectDir = wo;
}