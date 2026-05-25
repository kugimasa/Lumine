#include "Common.hlsli"
#include "DiffuseBrdf.hlsli"

// 拡散マテリアル
[shader("closesthit")]
void ClosestHit_Diffuse(inout HitInfo payload, Attributes attrib)
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

    // アルベドの取得
    float3 albedo = l_matCB.baseColor.rgb * l_diffuseTex.SampleLevel(gSampler, vtx.uv, 0).rgb;

    uint totalLights = gSceneParam.numRectLights + gSceneParam.numSphereLights;
    if (totalLights > 0)
    {
        // 光源サンプリング
        float selectionPdf = 1.0f;;
        SampledLightInfo lightInfo = SampleAllLights(payload.seed, payload.hitPos, selectionPdf);
        float3 toLight = lightInfo.position - payload.hitPos;
        float3 lightDir = normalize(toLight);
        float lightDist = length(toLight);
        // 光源方向へレイトレースして、光源と接続できた場合に寄与の計算
        if (!TraceShadowRay(payload.seed, payload.hitPos, lightDir, lightDist))
        {
            // 出射方向
            float3 wo = lightDir;
            float3 woLocal = WorldToLocal(lightDir, basis);

            // 全体のPDF = 光源選択のPDF × 個別光源のPDF
            float totalLightPdf = selectionPdf * lightInfo.pdf;

            // ヤコビアンの計算
            float cos1 = CalcCos(norm, lightDir);
            float cos2 = CalcCos(lightInfo.normal, -lightDir);
            float G = (cos1 * cos2) / max((lightDist * lightDist), EPS);

            // LambertBRDFの評価
            float brdfPdf = EPS;
            float3 brdf = EvaluateLambertDiffuseBrdf(albedo, wiLocal, woLocal, norm, brdfPdf);

            // MISウェイトの計算
            float misWeight = totalLightPdf / max((brdfPdf + totalLightPdf), EPS);

            // NEEによる寄与計算
            payload.color += payload.attenuation * (brdf * G / max(totalLightPdf, EPS)) * lightInfo.intensity * misWeight;
        }
    }

    // BRDFに基づく方向サンプリング
    float3 woLocal;
    float samplePdf;
    float3 brdf = SampleLambertDiffuseBrdf(albedo, wiLocal, woLocal, basis, samplePdf, payload.seed);
    
    // 寄与の計算
    float3 wo = LocalToWorld(woLocal, basis);
    float cosTheta = CalcCos(norm, wo);
    payload.attenuation *= brdf * cosTheta / max(samplePdf, EPS);
    payload.reflectDir = wo;
}