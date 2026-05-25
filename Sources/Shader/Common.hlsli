#ifndef COMMON_HLSLI
#define COMMON_HLSLI
// パストレース用ペイロード
struct HitInfo
{
    float3 hitPos;
    float3 reflectDir;
    float3 color;
    float3 attenuation;
    uint pathDepth;
    uint seed;
};

// シャドウレイ用ペイロード
struct ShadowRayHitInfo
{
    bool occluded;
};

// レイヒット時のアトリビュート
struct Attributes
{
    float2 bary;
};

struct Basis
{
    float3 normal;
    float3 tangent;
    float3 bitangent;
};

// 球面光源
struct SphereLight
{
    float3 intensity;
    float  area;
    float3 origin;
    float  radius;
};

// 矩形光源
struct RectLight
{
    float3 color;
    float  area;
    float3 origin;
    float  width;
    float3 normal;
    float  height;
    float3 tangent;
    float  intensity;
    float3 bitangent;
    float  padding2;
};

// シーンパラメーター
struct SceneParam
{
    matrix viewMtx;        // ビュー行列
    matrix projMtx;        // プロジェクション行列
    matrix invViewMtx;     // ビュー逆行列
    matrix invProjMtx;     // プロジェクション逆行列
    uint currentFrameNum;  // 現在のフレーム
    uint frameIndex;       // 描画中のフレームインデックス
    uint maxPathDepth;     // 最大反射回数
    uint maxSPP;           // Sample Per Pixel
    uint numSphereLights;  // 球面光源の数
    uint numRectLights;    // 矩形光源の数
    float iblPower;        // IBLの強度調整用パラメーター
    float whitePoint;      // ホワイトポイント（Reinhardトーンマッピング用）
    float3 ambientColor;   // 環境光カラー
    float padding;
};

// サンプリングされたライトの情報
struct SampledLightInfo
{
    float3 position;
    float  padding1;
    float3 normal;
    float  padding2;
    float3 intensity;
    float  pdf;
};

// 頂点情報
struct Vertex
{
    float3 position;
    float3 normal;
    float2 uv;
};

// マテリアルパラメーター
// reference: https://academysoftwarefoundation.github.io/OpenPBR/#parameterreference
struct MaterialParam
{
    // OpenPBR Base
    float baseWeight;           // 4 bytes
    float3 baseColor;           // 12 bytes
    float baseMetalness;        // 4 bytes
    float baseDiffuseRoughness; // 4 bytes

    // Fluorescence
    float fluorWeight;          // 4 bytes

    // OpenPBR Specular
    float specularWeight;       // 4 bytes
    float3 specularColor;       // 12 bytes
    float specularRoughness;    // 4 bytes
    float specularAnisotropy;   // 4 bytes
    float specularIOR;          // 4 bytes

    float padding2;             // 4 bytes

    // TODO: OpenPBR Transmission
    // TODO: OpenPBR Subsurface
    // TODO: OpenPBR Coat
    // TODO: OpenPBR Fuzz

    // OpenPBR Emission
    float emissionLuminance;    // 4 bytes
    float3 emissionColor;       // 12 bytes

    // TODO: OpenPBR Thin Film
    // TODO: OpenPBR Geometry

    // Total: 24 + (4) + 28 + (4) + 16 = 76 bytes
};

// グローバルルートシグネチャ
RaytracingAccelerationStructure gSceneBVH : register(t0);
// TODO: リファクタでPrefixをg_に統一
StructuredBuffer<SphereLight> gSphereLights : register(t1);
StructuredBuffer<RectLight> gRectLights : register(t2);
Texture2D<float4> g_BgTex : register(t3);
ConstantBuffer<SceneParam> gSceneParam : register(b0);
SamplerState gSampler : register(s0);

// ローカルルートシグネチャ
StructuredBuffer<uint> l_indexBuffer : register(t0, space1);
StructuredBuffer<Vertex> l_vertexBuffer : register(t1, space1);

Texture2D<float4> l_diffuseTex : register(t0, space2);
ConstantBuffer<MaterialParam> l_matCB : register(b0, space2);

#define PI 3.14159265359
#define INV_PI 0.318309886184
#define RAY_T_MIN 0.001
#define RAY_T_MAX 10000
#define EPS 1e-6
#define INF 1e10
#define RAY_EPSILON 0.001

inline float3 CalcHitAttrib(float3 vtxAttr[3], float2 bary)
{
    float3 ret;
    ret = vtxAttr[0];
    ret += bary.x * (vtxAttr[1] - vtxAttr[0]);
    ret += bary.y * (vtxAttr[2] - vtxAttr[0]);
    return ret;
}

inline float2 CalcSphereUV(float3 dir)
{
    dir = normalize(dir);
    float theta = atan2(dir.x, dir.z);
    float phi = acos(dir.y);
    float u = saturate((theta + PI) * INV_PI * 0.5);
    float v = saturate(phi * INV_PI);
    return float2(u, v);
}

// https://en.wikipedia.org/wiki/Xorshift
inline float Rand(inout uint seed)
{
    uint rnd = seed;
    rnd ^= rnd << 13;
    rnd ^= rnd >> 7;
    rnd ^= rnd << 5;
    seed = rnd;
    return (float) (rnd & 0x00FFFFFF) / (float) 0x01000000;
}

inline Basis MakeBasis(in float3 norm)
{
    Basis basis;
    float3 up = abs(norm.z) < 0.999 ? float3(0.0, 0.0, 1.0) : float3(1.0, 0.0, 0.0);
    float3 tangent = normalize(up - (dot(up, norm) * norm));
    float3 bitangent = cross(norm, tangent);
    basis.normal = norm;
    basis.tangent = tangent;
    basis.bitangent = bitangent;
    return basis;
}

inline float3 WorldToLocal(float3 worldDir, in Basis basis)
{
    return float3(
        dot(worldDir, basis.tangent),
        dot(worldDir, basis.bitangent),
        dot(worldDir, basis.normal)
    );
}

inline float3 LocalToWorld(float3 localDir, in Basis basis)
{
    return localDir.x * basis.tangent + localDir.y * basis.bitangent + localDir.z * basis.normal;
}

inline float3 SampleHemisphereCos(in Basis basis, in uint seed)
{
    float r1 = Rand(seed);
    float r2 = Rand(seed);
    float phi = 2.0 * PI * r1;
    float x = cos(phi) * sqrt(r2);
    float y = sin(phi) * sqrt(r2);
    float z = sqrt(max(0.0, 1.0 - r2));
    return float3(x, y, z);;
}

///// 球面光源 /////

// 球面上の点をサンプリング
inline float3 SampleSphere(in uint seed, float3 center, float radius)
{
    float r1 = Rand(seed);
    float r2 = Rand(seed);
    float theta = 2.0 * PI * r1;
    float phi = acos(1.0 - 2.0 * r2);
    float x = sin(phi) * cos(theta);
    float y = sin(phi) * sin(theta);
    float z = cos(phi);
    return center + radius * float3(x, y, z);
}

// 面光源の立体角に関するPDF（solid angle based）
inline float AreaLightPdf(float area, float3 hitPos, float3 samplePos, float3 lightNorm)
{
    float areaPdf = area > 0.0 ? (1.0 / area) : 1.0;
    // 立体角に関するPDFへの変換
    float3 dir = samplePos - hitPos;
    float distSq = dot(dir, dir);
    float absCosTheta = abs(dot(lightNorm, normalize(dir)));
    // PDF = areaPdf * dist^2 / |cos(theta)|
    return areaPdf * distSq / max(absCosTheta, EPS);
}

// 球面光源のサンプリング
inline SampledLightInfo SampleSphereLight(inout uint seed, SphereLight light, float3 hitPos)
{
    SampledLightInfo info;
    // 球面上の点をサンプリング
    info.position = SampleSphere(seed, light.origin, light.radius);
    info.normal = normalize(info.position - light.origin);
    info.intensity = light.intensity;
    float3 lightDir = light.origin - info.position;
    info.pdf = AreaLightPdf(light.area, hitPos, info.position, info.normal);
    return info;
}

///// 矩形光源 /////

inline float3 SampleRect(in uint seed, float3 center, float3 normal, float3 tangent, float3 bitangent, float width, float height)
{
    float r1 = Rand(seed);
    float r2 = Rand(seed);
    float u = (r1 - 0.5) * width;
    float v = (r2 - 0.5) * height;
    return center + u * tangent + v * bitangent;
}

// 矩形光源のサンプリング
inline SampledLightInfo SampleRectLight(inout uint seed, RectLight light, float3 hitPos)
{
    SampledLightInfo info;
    // 矩形上の点をサンプリング
    info.position = SampleRect(seed, light.origin, light.normal, light.tangent, light.bitangent, light.width, light.height);
    info.normal = light.normal;
    info.intensity = light.color * light.intensity;
    info.pdf = AreaLightPdf(light.area, hitPos, info.position, info.normal);
    return info;
}

// 全ライトからランダムに1つをサンプリング
inline SampledLightInfo SampleAllLights(inout uint seed, float3 hitPos, out float selectionPdf)
{
    // ライトの総数 = 球面光源の数 + 矩形光源の数
    uint totalLights = gSceneParam.numSphereLights + gSceneParam.numRectLights;
    float r = Rand(seed);
    uint lightIdx = min((uint)(r * totalLights), totalLights - 1);
    selectionPdf = 1.0 / totalLights;
    if (lightIdx < gSceneParam.numSphereLights)
    {
        // 球面光源のサンプリング
        SphereLight light = gSphereLights[lightIdx];
        return SampleSphereLight(seed, light, hitPos);
    }
    else
    {
        // 矩形光源のサンプリング
        lightIdx -= gSceneParam.numSphereLights;
        RectLight light = gRectLights[lightIdx];
        return SampleRectLight(seed, light, hitPos);
    }
}

Vertex GetHitVertex(Attributes attrib)
{
    Vertex v = (Vertex) 0;
    uint idxStart = PrimitiveIndex() * 3;
    
    float3 position[3];
    float3 normal[3];
    float3 uv[3];
    for (int i = 0; i < 3; ++i)
    {
        uint idx = l_indexBuffer[idxStart + i];
        position[i] = l_vertexBuffer[idx].position;
        normal[i] = l_vertexBuffer[idx].normal;
        uv[i] = float3(l_vertexBuffer[idx].uv, 1);
    }

    v.position = CalcHitAttrib(position, attrib.bary);
    v.normal = normalize(CalcHitAttrib(normal, attrib.bary));
    v.uv = CalcHitAttrib(uv, attrib.bary).xy;
    return v;
}

// NEE用のShadow Rayのトレース
bool TraceShadowRay(in uint seed, in float3 origin, in float3 direction, in float lightDist)
{
    RayDesc ray;
    ray.Origin = origin;
    ray.Direction = normalize(direction);
    ray.TMin = RAY_T_MIN;
    ray.TMax = lightDist - RAY_T_MIN;

    RAY_FLAG flags = RAY_FLAG_NONE;
    flags |= RAY_FLAG_SKIP_CLOSEST_HIT_SHADER;
    uint rayMask = ~(0x08); // LightMaterialのビットを除外
    uint rayIdx = 0;
    uint geoMulVal = 1;
    uint missIdx = 1;
    ShadowRayHitInfo payload;
    payload.occluded = true;
    TraceRay(gSceneBVH, flags, rayMask, rayIdx, geoMulVal, missIdx, ray, payload);
    return payload.occluded;
}

inline float CalcCos(float3 a, float3 b)
{
    float cos = dot(normalize(a), normalize(b));
    return max(abs(cos), EPS);
}

inline float HemisphereCosPdf(float3 dir, float3 norm)
{
    float cos = CalcCos(dir, norm);
    return max(cos * INV_PI, 0.0);
}

inline float AreaSpherePdf(float radius)
{
    return INV_PI / (4 * radius * radius);
}

inline float3 GammaCorrect(float3 color, float gamma = 2.2)
{
    return pow(color, 1.0 / gamma);
}

// Reinhard Tone Mapping
// https://dl.acm.org/doi/pdf/10.1145/566654.566575
inline float3 ReinhardToneMapping(float3 color, float3 whitePoint = 4.0)
{
    float3 a = color * (1.0 + color / (whitePoint * whitePoint));
    float3 b = 1.0 + color;
    return a / b;
}

// ACES Filmic Tone Mapping by Krzysztof Narkowicz
// reference: https://knarkowicz.wordpress.com/2016/01/06/aces-filmic-tone-mapping-curve/
inline float3 ACESFilmicToneMapping(float3 color)
{
    const float a = 2.51f;
    const float b = 0.03f;
    const float c = 2.43f;
    const float d = 0.59f;
    const float e = 0.14f;
    color = (color * (a * color + b)) / (color * (c * color + d) + e);
    return saturate(color);
}
#endif // COMMON_HLSLI