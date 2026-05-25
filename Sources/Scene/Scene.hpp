#pragma once

#include <memory>

#include "Device.hpp"
#include "Camera.hpp"
#include "Materials/LightMaterial.hpp"
#include "Objects/Rect.hpp"
#include "Objects/Sphere.hpp"
#include "Objects/Mesh.hpp"

class LightMaterial;

class Scene
{
public:
    Scene();
    ~Scene() = default;

    // 球面光源
    struct SphereLight
    {
        Float3 intensity;
        float area;
        Float3 origin;
        float radius;
    };

    // 矩形光源
    struct RectLight
    {
        Float3 emissiveColor;
        float area;
        Float3 origin;
        float width;
        Float3 normal;
        float height;
        Float3 tangent;
        float intensity;
        Float3 bitangent;
        float padding2;
    };

    struct SceneParam
    {
        Matrix viewMtx;
        Matrix projMtx;
        Matrix invViewMtx;
        Matrix invProjMtx;
        UINT currentFrameNum;
        UINT frameIndex;
        UINT maxPathDepth;
        UINT maxSPP;
        UINT numSphereLights;
        UINT numRectLights;
        float iblPower;
        float whitePoint;
        Float3 ambientColor;
        float padding1;
    };

    void OnInit(const std::unique_ptr<Device>& device, float aspect, int maxFrame);
    void OnUpdate(const std::unique_ptr<Device>& device, int currentFrame, int maxFrame);
    void OnDestroy(const std::unique_ptr<Device>& device);

    void BuildTLAS(const std::unique_ptr<Device>& device);
    void UpdateBLAS(const ComPtr<ID3D12GraphicsCommandList4>& cmdList);
    void UpdateTLAS(const std::unique_ptr<Device>& device, const ComPtr<ID3D12GraphicsCommandList4>& cmdList);
    uint8_t* WriteHitGroupShaderRecord(uint8_t* dst, UINT hitGroupRecordSize, ComPtr<ID3D12StateObject>& rtStateObject);

    void SetMaxPathDepth(UINT maxPathDepth) { m_maxPathDepth = maxPathDepth; }
    void SetMaxSPP(UINT maxSPP) { m_maxSPP = maxSPP; }

    // ライト関連
    void SetDynamicRectLightPosition(Float3 position);
    void SetDynamicRectLightSize(float size);
    void SetDynamicRectLightColor(Float3 color);
    void SetDynamicRectLightIntensity(float intensity);
    void SetAmbientColor(Float3 ambientColor) { m_ambientColor = ambientColor; }
    void SetIblPower(float iblPower) { m_iblPower = iblPower; }
    void SetWhitePoint(float whitePoint) { m_whitePoint = whitePoint; }

    UINT GetMaxPathDepth() { return m_maxPathDepth; }
    UINT GetMaxSPP() { return m_maxSPP; }
    Float3 GetAmbientColor() { return m_ambientColor; }
    Float3 GetDynamicRectLightPosition() { return m_pDynamicRectLight->GetOrigin(); }
    float GetDynamicRectLightSize() { return m_pDynamicRectLight->GetWidth(); }
    Float3 GetDynamicRectLightColor() { return m_pDynamicRectLightMaterial->GetEmissionColor(); }
    float GetDynamicRectLightIntensity() { return m_pDynamicRectLightMaterial->GetEmissionLuminance(); }
    float GetIblPower() { return m_iblPower; }
    float GetWhitePoint() { return m_whitePoint; }
    std::shared_ptr<Camera> GetCamera() { return m_pCamera; }
    std::shared_ptr<Mesh> GetMesh() { return m_pMesh; }
    std::shared_ptr<::Rect> GetFluorRect() { return m_pFluorRect; }
    UINT GetTotalHitGroupCount() { return m_totalHitGroupCount; }
    ComPtr<ID3D12Resource> GetSceneCB(UINT frameIndex) { return m_pConstantBuffers[frameIndex]; }
    DescriptorHeap GetTLASDescHeap() { return m_tlasDescHeap; }
    DescriptorHeap GetSphereLightBufferHeap() { return m_sphereLightBufferHeap; }
    DescriptorHeap GetRectLightBufferHeap() { return m_rectLightBufferHeap; }
    TextureResource GetBackgroundTex() { return m_bgTex; }

private:
    void UpdateSceneParam(UINT currentFrame, UINT frameIndex);
    void CreateRTInstanceDesc(std::vector<D3D12_RAYTRACING_INSTANCE_DESC>& instanceDescs);
    void SetTotalHitGroupCount();
    void CreateLightBuffers(const std::unique_ptr<Device>& device);
    void UpdateDynamicLightParam();
    void UpdateLightBuffers(const std::unique_ptr<Device>& device);
    void BuildLightList();

    // アニメーション処理
    void OnAnimation(float timeDuration);

private:
    SceneParam m_sceneParam;
    UINT m_maxPathDepth;
    UINT m_maxSPP;
    float m_iblPower;
    float m_whitePoint;
    Float3 m_ambientColor;
    UINT m_totalHitGroupCount;
    std::shared_ptr<Camera> m_pCamera;
    std::shared_ptr<Sphere> m_pSphere; // 今回は使わない
    std::shared_ptr<::Rect> m_pPlane;
    std::shared_ptr<::Rect> m_pFluorRect;
    std::shared_ptr<::Rect> m_pRectLight1;
    std::shared_ptr<::Rect> m_pRectLight2;
    std::shared_ptr<LightMaterial> m_pDynamicRectLightMaterial;
    std::shared_ptr<::Rect> m_pDynamicRectLight;
    std::shared_ptr<Mesh> m_pMesh;
    ComPtr<ID3D12Resource> m_pTLAS;
    ComPtr<ID3D12Resource> m_pTLASUpdate;
    DescriptorHeap m_tlasDescHeap;
    std::vector<ComPtr<ID3D12Resource>> m_pRTInstanceBuffers;
    std::vector<ComPtr<ID3D12Resource>> m_pConstantBuffers;

    // 光源データ
    // TODO: 別クラスで管理したい
    // TODO: オブジェクトリストを受け取って、ライトリストの構築を行う
    std::vector<SphereLight> m_sphereLights;
    std::vector<RectLight> m_rectLights;
    ComPtr<ID3D12Resource> m_pSphereLightBuffer;
    ComPtr<ID3D12Resource> m_pRectLightBuffer;
    DescriptorHeap m_sphereLightBufferHeap;
    DescriptorHeap m_rectLightBufferHeap;

    // 背景用テクスチャ
    TextureResource m_bgTex;
};
