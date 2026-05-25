#include "Common.hlsli"

float3 GetEmissionColor()
{
    return l_matCB.emissionColor.rgb;
}

// 発光マテリアル(球面光源や矩形光源用)
[shader("closesthit")]
void ClosestHit_Emissive(inout HitInfo payload, Attributes attrib)
{
    Vertex vtx = GetHitVertex(attrib);
    float3 worldPos = WorldRayOrigin() + WorldRayDirection() * RayTCurrent();
    float3 worldNorm = mul(vtx.normal, (float3x3)ObjectToWorld4x3());

    // 自己交差を避けるために法線方向にオフセット
    payload.hitPos = worldPos + worldNorm * RAY_EPSILON;

    // 発光マテリアル自体の寄与
    if (payload.pathDepth == 0)
    {
        payload.color += payload.attenuation * GetEmissionColor();
    }
    // レイトレース終了
    payload.pathDepth = gSceneParam.maxPathDepth;
}