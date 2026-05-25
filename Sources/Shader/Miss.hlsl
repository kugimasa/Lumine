#include "Common.hlsli"

[shader("miss")]
void Miss(inout HitInfo payload)
{
    // TODO: IBLの重点的サンプリング
    // float2 uv = CalcSphereUV(WorldRayDirection());
    // float3 bgCol = g_BgTex.SampleLevel(gSampler, uv, 0).rgb * gSceneParam.iblPower;
    // payload.color =  payload.attenuation * bgCol;

    // MEMO: 見た目次第でIBLは使わないかも
    payload.color = gSceneParam.ambientColor * payload.attenuation * gSceneParam.iblPower;

    payload.pathDepth = gSceneParam.maxPathDepth;
}

[shader("miss")]
void ShadowMiss(inout ShadowRayHitInfo payload)
{
    payload.occluded = false;
}
