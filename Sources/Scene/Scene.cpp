#include "Scene.hpp"
#include "Materials/Material.hpp"
#include "Materials/LightMaterial.hpp"
#include "Materials/OpenpbrMaterial.hpp"
#include "Materials/FluorescenceMaterial.hpp"
#include "Utils/ColorUtil.h"
#include "Utils/DxrUtil.h"

Scene::Scene()
    :
    m_pCamera(),
    m_pSphere(),
    m_pPlane(),
    m_pFluorRect(),
    m_pMesh(),
    m_pRectLight1(),
    m_pRectLight2(),
    m_pDynamicRectLight(),
    m_sceneParam(),
    m_maxPathDepth(8),
    m_maxSPP(100),
    m_ambientColor(Float3(0.8, 0.8, 0.8)),
    m_iblPower(1.0f),
    m_whitePoint(4.0f),
    m_totalHitGroupCount(0)
{
}

void Scene::OnInit(const std::unique_ptr<Device>& device, float aspect, int maxFrame)
{
    // 初期設定
    float fovY = 45.0f;
    float nearZ = 0.1f;
    float farZ = 100.0f;
    Float3 startPos(0.0f, 5.0f, -25.0f);
    Float3 target(0.0f, 5.0f, -5.0f);
    Float3 fluorRectPos(0.0f, 5.0f, -5.0f);
    Float3 bunnyPos(-5.0f, 0.0f, -10.0f);
    Float3 bunnyRot(0.0f, 180.0f, 0.0f);
    Float3 bunnyScale(2.0f, 2.0f, 2.0f);

    if (maxFrame > 0)
    {
        startPos = {50.0f, 50.0f, -10.0f};
        // TODO: 揃える
        bunnyPos = {-5.0f, 0.0f, 5.0f};
        bunnyRot = {0.0f, 0.0f, 0.0f};
    }

    m_pCamera = std::make_shared<Camera>(fovY, aspect, nearZ, farZ, startPos, target);

    // モデルの初期設定とBLAS構築
    auto diffuseGreen = std::make_shared<Material>(L"Sea Green (Green)", MaterialType::Diffuse, COL_SEA_GREEN);
    auto diffuseWhite = std::make_shared<Material>(L"Snow (White)", MaterialType::Diffuse, COL_SNOW);

    auto fluorTEXTYELL = std::make_shared<FluorescenceMaterial>(L"", COL_YELLOW);
    // 蛍光用のRect
    m_pFluorRect = std::make_shared<::Rect>(10.0f, 10.0f,
                                            fluorRectPos,
                                            Float3(0.0f, 0.0f, -1.0f), true,
                                            L"DummyWhite.png");
    m_pFluorRect->OnInit(device, fluorTEXTYELL);

    // 床面
    auto planeBlack = std::make_shared<OpenPBRMaterial>(L"Jet (Black)", COL_BLACK);
    planeBlack->SetBaseMetalness(0.8f);
    planeBlack->SetSpecularAnisotropy(0.15f);
    planeBlack->SetSpecularRoughness(0.15f);
    m_pPlane = std::make_shared<::Rect>(100.0f, Float3(0.0f, 0.0f, 0.0f), Float3(0.0f, 1.0f, 0.0f));
    m_pPlane->OnInit(device, planeBlack);

    // モデルをロード（OpenPBRマテリアルを適用）
    auto openPBRMat = std::make_shared<OpenPBRMaterial>(L"OpenPBR-Default", COL_SEA_GREEN);
    std::wstring modelPath = GEO_DIR L"/bunny.obj";
    m_pMesh = std::make_shared<Mesh>(modelPath, bunnyPos);
    m_pMesh->OnInit(device, openPBRMat);
    m_pMesh->SetRotation(bunnyRot);
    m_pMesh->SetScale(bunnyScale);

    // Staticライト
    auto emissiveCyan = std::make_shared<LightMaterial>(L"Light Aqua (Cyan)", COL_AQUA, 50);
    auto emissiveMagenta = std::make_shared<LightMaterial>(L"Light Ultra Pink (Magenta)", COL_ULTRA_PINK, 50);
    auto rectLight1Pos = Float3(-10.0f, 4.5f, -10.0f);
    m_pRectLight1 = std::make_shared<::Rect>(8.0f, rectLight1Pos, fluorRectPos);
    m_pRectLight1->OnInit(device, emissiveCyan);
    auto rectLight2Pos = Float3(10.0f, 4.5f, -10.0f);
    m_pRectLight2 = std::make_shared<::Rect>(8.0f, rectLight2Pos, fluorRectPos);
    m_pRectLight2->OnInit(device, emissiveMagenta);
    // Dynamicライト
    auto dynamicLightPos = Float3(0.0f, 15.0f, -15.0f);
    m_pDynamicRectLightMaterial = std::make_shared<LightMaterial>(L"Light Dynamic", COL_SNOW, 15);
    m_pDynamicRectLight = std::make_shared<::Rect>(15.0f, dynamicLightPos, fluorRectPos);
    m_pDynamicRectLight->OnInit(device, m_pDynamicRectLightMaterial);

    // 総HitGroup数を計算
    SetTotalHitGroupCount();

    // TLAS構築
    BuildTLAS(device);

    // 面光源リストの構築
    BuildLightList();
    // 光源バッファ(SRV)の作成
    CreateLightBuffers(device);

    // 定数バッファの作成
    if (device->CreateConstantBuffer(m_pConstantBuffers, sizeof(SceneParam), L"SceneCB"))
    {
        UpdateSceneParam(0, device->GetCurrentFrameIndex());
    }

    // 背景テクスチャのロード
    m_bgTex = LoadTexture(L"DummyWhite.png", device);

    Print(PrintInfoType::SCENE, L"シーン構築 完了");
}

void Scene::OnUpdate(const std::unique_ptr<Device>& device, int currentFrame, int maxFrame)
{
    // Dynamic光源の値を更新
    // 光源バッファの更新
    UpdateLightBuffers(device);

    // アニメーション処理
    if (maxFrame > 0)
    {
        constexpr float MAX_ANIMATION_TIME = 10.0f;
        float timeDuration = float(currentFrame) * MAX_ANIMATION_TIME / float(maxFrame);
        OnAnimation(timeDuration);
    }

    // シーンパラメータの更新
    UINT frameIndex = device->GetCurrentFrameIndex();
    UpdateSceneParam(currentFrame, frameIndex);

    // シーンバッファの書き込み
    auto sceneCB = m_pConstantBuffers[frameIndex];
    device->WriteBuffer(sceneCB, &m_sceneParam, sizeof(SceneParam));

    // オブジェクト関連の更新
    m_pFluorRect->OnUpdate(device);
    m_pDynamicRectLight->OnUpdate(device);

    // コマンド作成 - BLAS/TLAS更新
    auto cmdList = device->CreateCommandList();
    UpdateBLAS(cmdList);
    UpdateTLAS(device, cmdList);
    // コマンド終了
    cmdList->Close();

    // コマンド実行 - BLAS/TLAS更新
    device->ExecuteCommandList(cmdList);
    // コマンドの完了を待機
    device->WaitForGpu();
}

void Scene::OnDestroy(const std::unique_ptr<Device>& device)
{
    if (m_pCamera)
    {
        m_pCamera.reset();
    }
    if (m_pSphere)
    {
        m_pSphere.reset();
    }
    if (m_pFluorRect)
    {
        m_pFluorRect.reset();
    }
    if (m_pPlane)
    {
        m_pPlane.reset();
    }
    if (m_pMesh)
    {
        m_pMesh.reset();
    }
    if (m_pRectLight1)
    {
        m_pRectLight1.reset();
    }
    if (m_pRectLight2)
    {
        m_pRectLight2.reset();
    }
    if (m_pDynamicRectLight)
    {
        m_pDynamicRectLight.reset();
    }
    m_sphereLights.clear();
    m_rectLights.clear();
    for (auto& cb : m_pConstantBuffers)
    {
        if (cb)
        {
            cb.Reset();
        }
    }
    if (m_pSphereLightBuffer)
    {
        m_pSphereLightBuffer.Reset();
    }
    if (m_pRectLightBuffer)
    {
        m_pRectLightBuffer.Reset();
    }
}

// TLASの構築
void Scene::BuildTLAS(const std::unique_ptr<Device>& device)
{
    // インスタンス情報設定の初期化
    std::vector<D3D12_RAYTRACING_INSTANCE_DESC> instanceDescs;
    CreateRTInstanceDesc(instanceDescs);

    // インスタンス情報用のバッファを確保
    m_pRTInstanceBuffers.resize(device->BackBufferCount);
    auto instanceDescSize = UINT(ROUND_UP(instanceDescs.size() * sizeof(D3D12_RAYTRACING_INSTANCE_DESC), 256));
    for (auto& rtInstanceBuff : m_pRTInstanceBuffers)
    {
        rtInstanceBuff = device->CreateBuffer(
            instanceDescSize,
            D3D12_RESOURCE_FLAG_NONE,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            D3D12_HEAP_TYPE_UPLOAD,
            L"RTInstanceDescBuffer"
        );
        device->WriteBuffer(rtInstanceBuff, instanceDescs.data(), instanceDescSize);
    }

    // TLASの情報をセット
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC buildASDesc{};
    auto& inputs = buildASDesc.Inputs;
    inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
    inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
    inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE;
    inputs.NumDescs = UINT(instanceDescs.size());
    inputs.InstanceDescs = m_pRTInstanceBuffers.front()->GetGPUVirtualAddress();

    // TLAS関連のバッファを確保
    auto tlas = CreateASBuffers(device, buildASDesc, L"TLAS");
    auto tlasScratch = tlas.scratchBuffer;
    m_pTLAS = tlas.asBuffer;
    m_pTLASUpdate = tlas.updateBuffer;
    // Acceleration Structure 構築
    buildASDesc.ScratchAccelerationStructureData = tlasScratch->GetGPUVirtualAddress();
    buildASDesc.DestAccelerationStructureData = m_pTLAS->GetGPUVirtualAddress();

    // SRVの作成
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.RaytracingAccelerationStructure.Location = m_pTLAS->GetGPUVirtualAddress();
    m_tlasDescHeap = device->CreateSRV(nullptr, &srvDesc);

    // TLAS構築コマンド
    auto cmdList = device->CreateCommandList();
    cmdList->BuildRaytracingAccelerationStructure(&buildASDesc, 0, nullptr);
    auto barrier = CD3DX12_RESOURCE_BARRIER::UAV(m_pTLAS.Get());
    cmdList->ResourceBarrier(1, &barrier);
    cmdList->Close();

    // コマンド実行 - TLAS構築
    device->ExecuteCommandList(cmdList);
    // コマンドの完了を待機
    device->WaitForGpu();

    Print(PrintInfoType::SCENE, L"TLAS構築 完了");
}

// BLASの更新
void Scene::UpdateBLAS(const ComPtr<ID3D12GraphicsCommandList4>& cmdList)
{
    m_pFluorRect->UpdateBLAS(cmdList);
    m_pPlane->UpdateBLAS(cmdList);
    m_pMesh->UpdateBLAS(cmdList);
    m_pRectLight1->UpdateBLAS(cmdList);
    m_pRectLight2->UpdateBLAS(cmdList);
    m_pDynamicRectLight->UpdateBLAS(cmdList);
}

// TLASの更新
void Scene::UpdateTLAS(const std::unique_ptr<Device>& device, const ComPtr<ID3D12GraphicsCommandList4>& cmdList)
{
    auto frameIdx = device->GetCurrentFrameIndex();
    // インスタンス情報設定の初期化
    std::vector<D3D12_RAYTRACING_INSTANCE_DESC> instanceDescs;
    CreateRTInstanceDesc(instanceDescs);
    auto instanceDescSize = UINT(instanceDescs.size() * sizeof(D3D12_RAYTRACING_INSTANCE_DESC));
    auto rtInstanceBuff = m_pRTInstanceBuffers[frameIdx];
    device->WriteBuffer(rtInstanceBuff, instanceDescs.data(), instanceDescSize);

    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC updateASDesc{};
    auto& inputs = updateASDesc.Inputs;
    inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
    inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
    inputs.Flags =
        D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE |
        D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PERFORM_UPDATE;
    inputs.NumDescs = UINT(instanceDescs.size());
    inputs.InstanceDescs = rtInstanceBuff->GetGPUVirtualAddress();

    // TLASを直接更新
    updateASDesc.SourceAccelerationStructureData = m_pTLAS->GetGPUVirtualAddress();
    updateASDesc.DestAccelerationStructureData = m_pTLAS->GetGPUVirtualAddress();
    updateASDesc.ScratchAccelerationStructureData = m_pTLASUpdate->GetGPUVirtualAddress();

    // TLAS更新コマンド
    cmdList->BuildRaytracingAccelerationStructure(&updateASDesc, 0, nullptr);
    auto barrier = CD3DX12_RESOURCE_BARRIER::UAV(m_pTLAS.Get());
    cmdList->ResourceBarrier(1, &barrier);
}

uint8_t*
Scene::WriteHitGroupShaderRecord(uint8_t* dst, UINT hitGroupRecordSize, ComPtr<ID3D12StateObject>& rtStateObject)
{
    ComPtr<ID3D12StateObjectProperties> rtStateObjectProps;
    rtStateObject.As(&rtStateObjectProps);
    // MEMO: シーンで管理されているオブジェクトの分だけシェーダーレコードの書き込み
    dst = m_pFluorRect->WriteHitGroupShaderRecord(dst, hitGroupRecordSize, rtStateObjectProps);
    dst = m_pPlane->WriteHitGroupShaderRecord(dst, hitGroupRecordSize, rtStateObjectProps);
    dst = m_pMesh->WriteHitGroupShaderRecord(dst, hitGroupRecordSize, rtStateObjectProps);
    dst = m_pRectLight1->WriteHitGroupShaderRecord(dst, hitGroupRecordSize, rtStateObjectProps);
    dst = m_pRectLight2->WriteHitGroupShaderRecord(dst, hitGroupRecordSize, rtStateObjectProps);
    dst = m_pDynamicRectLight->WriteHitGroupShaderRecord(dst, hitGroupRecordSize, rtStateObjectProps);
    return dst;
}

void Scene::CreateRTInstanceDesc(std::vector<D3D12_RAYTRACING_INSTANCE_DESC>& instanceDescs)
{
    // TODO: オブジェクト追加のたびに肥大化するのやめたい
    instanceDescs.clear();
    UINT instanceHitGroupOffset = 0;
    // FluorRect
    {
        D3D12_RAYTRACING_INSTANCE_DESC desc{};
        auto mtxTrans = m_pFluorRect->GetWorldMtx();
        XMStoreFloat3x4(reinterpret_cast<Mtx3x4*>(&desc.Transform), mtxTrans);
        desc.InstanceID = 0;
        desc.InstanceMask = m_pFluorRect->GetMaterial()->GetRTInstanceMask();
        desc.InstanceContributionToHitGroupIndex = instanceHitGroupOffset;
        desc.Flags = D3D12_RAYTRACING_INSTANCE_FLAG_NONE;
        desc.AccelerationStructure = m_pFluorRect->GetBLAS()->GetGPUVirtualAddress();
        instanceDescs.push_back(desc);
        instanceHitGroupOffset += 1; // 単一メッシュ
    }
    // Plane
    {
        D3D12_RAYTRACING_INSTANCE_DESC desc{};
        auto mtxTrans = m_pPlane->GetWorldMtx();
        XMStoreFloat3x4(reinterpret_cast<Mtx3x4*>(&desc.Transform), mtxTrans);
        desc.InstanceID = 0;
        desc.InstanceMask = m_pPlane->GetMaterial()->GetRTInstanceMask();
        desc.InstanceContributionToHitGroupIndex = instanceHitGroupOffset;
        desc.Flags = D3D12_RAYTRACING_INSTANCE_FLAG_NONE;
        desc.AccelerationStructure = m_pPlane->GetBLAS()->GetGPUVirtualAddress();
        instanceDescs.push_back(desc);
        instanceHitGroupOffset += 1; // 単一メッシュ
    }
    // Mesh
    {
        D3D12_RAYTRACING_INSTANCE_DESC desc{};
        auto mtxTrans = m_pMesh->GetWorldMtx();
        XMStoreFloat3x4(reinterpret_cast<Mtx3x4*>(&desc.Transform), mtxTrans);
        desc.InstanceID = 0;
        desc.InstanceMask = m_pMesh->GetMaterial()->GetRTInstanceMask();
        // MEMO: 複数マテリアルの場合はこちらを調整する必要がある
        desc.InstanceContributionToHitGroupIndex = instanceHitGroupOffset;
        desc.Flags = D3D12_RAYTRACING_INSTANCE_FLAG_NONE;
        desc.AccelerationStructure = m_pMesh->GetBLAS()->GetGPUVirtualAddress();
        instanceDescs.push_back(desc);
        instanceHitGroupOffset += 1; // 単一メッシュ
    }
    auto rectLights = {m_pRectLight1, m_pRectLight2, m_pDynamicRectLight};
    for (auto light : rectLights)
    {
        {
            D3D12_RAYTRACING_INSTANCE_DESC desc{};
            auto mtxTrans = light->GetWorldMtx();
            XMStoreFloat3x4(reinterpret_cast<Mtx3x4*>(&desc.Transform), mtxTrans);
            desc.InstanceID = 0;
            desc.InstanceMask = light->GetMaterial()->GetRTInstanceMask();
            desc.InstanceContributionToHitGroupIndex = instanceHitGroupOffset;
            desc.Flags = D3D12_RAYTRACING_INSTANCE_FLAG_NONE;
            desc.AccelerationStructure = light->GetBLAS()->GetGPUVirtualAddress();
            instanceDescs.push_back(desc);
            instanceHitGroupOffset += 1; // 単一メッシュ
        }
    }
}

void Scene::UpdateSceneParam(UINT currentFrame, UINT frameIndex)
{
    m_sceneParam.viewMtx = m_pCamera->GetViewMatrix();
    m_sceneParam.projMtx = m_pCamera->GetProjMatrix();
    m_sceneParam.invViewMtx = XMMatrixInverse(nullptr, m_sceneParam.viewMtx);
    m_sceneParam.invProjMtx = XMMatrixInverse(nullptr, m_sceneParam.projMtx);
    m_sceneParam.currentFrameNum = currentFrame;
    m_sceneParam.frameIndex = frameIndex;
    m_sceneParam.maxPathDepth = m_maxPathDepth;
    m_sceneParam.maxSPP = m_maxSPP;
    m_sceneParam.numSphereLights = static_cast<UINT>(m_sphereLights.size());
    m_sceneParam.numRectLights = static_cast<UINT>(m_rectLights.size());
    m_sceneParam.ambientColor = m_ambientColor;
    m_sceneParam.iblPower = m_iblPower;
    m_sceneParam.whitePoint = m_whitePoint;
}

void Scene::SetTotalHitGroupCount()
{
    m_totalHitGroupCount = 0;
    if (m_pFluorRect) m_totalHitGroupCount += m_pFluorRect->GetHitGroupCount();
    if (m_pPlane) m_totalHitGroupCount += m_pPlane->GetHitGroupCount();
    if (m_pMesh) m_totalHitGroupCount += m_pMesh->GetHitGroupCount();
    if (m_pRectLight1) m_totalHitGroupCount += m_pRectLight1->GetHitGroupCount();
    if (m_pRectLight2) m_totalHitGroupCount += m_pRectLight2->GetHitGroupCount();
    if (m_pDynamicRectLight) m_totalHitGroupCount += m_pDynamicRectLight->GetHitGroupCount();
}

void Scene::CreateLightBuffers(const std::unique_ptr<Device>& device)
{
    // 球面光源（空の場合はダミーバッファを作成）
    {
        UINT numElements = m_sphereLights.empty() ? 1 : static_cast<UINT>(m_sphereLights.size());
        UINT bufferSize = static_cast<UINT>(sizeof(SphereLight) * numElements);
        m_pSphereLightBuffer = device->CreateBuffer(
            bufferSize,
            D3D12_RESOURCE_FLAG_NONE,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            D3D12_HEAP_TYPE_UPLOAD,
            L"SphereLightBuffer"
        );

        // SRVの作成
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Format = DXGI_FORMAT_UNKNOWN;
        srvDesc.Buffer.FirstElement = 0;
        srvDesc.Buffer.NumElements = numElements;
        srvDesc.Buffer.StructureByteStride = sizeof(SphereLight);
        srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
        m_sphereLightBufferHeap = device->CreateSRV(m_pSphereLightBuffer.Get(), &srvDesc);

        // データ書き込み（データがある場合のみ）
        if (!m_sphereLights.empty())
        {
            device->WriteBuffer(m_pSphereLightBuffer, m_sphereLights.data(), bufferSize);
        }
        Print(PrintInfoType::SCENE, "球面光源バッファ作成 完了 ライト数: ", m_sphereLights.size());
    }
    // 矩形光源（空の場合はダミーバッファを作成）
    {
        UINT numElements = m_rectLights.empty() ? 1 : static_cast<UINT>(m_rectLights.size());
        UINT bufferSize = static_cast<UINT>(sizeof(RectLight) * numElements);
        m_pRectLightBuffer = device->CreateBuffer(
            bufferSize,
            D3D12_RESOURCE_FLAG_NONE,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            D3D12_HEAP_TYPE_UPLOAD,
            L"RectLightBuffer"
        );

        // SRVの作成
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Format = DXGI_FORMAT_UNKNOWN;
        srvDesc.Buffer.FirstElement = 0;
        srvDesc.Buffer.NumElements = numElements;
        srvDesc.Buffer.StructureByteStride = sizeof(RectLight);
        srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
        m_rectLightBufferHeap = device->CreateSRV(m_pRectLightBuffer.Get(), &srvDesc);

        // データ書き込み（データがある場合のみ）
        if (!m_rectLights.empty())
        {
            device->WriteBuffer(m_pRectLightBuffer, m_rectLights.data(), bufferSize);
        }
        Print(PrintInfoType::SCENE, "矩形光源バッファ作成 完了  ライト数: ", m_rectLights.size());
    }
}

void Scene::BuildLightList()
{
    // TODO: シーンに登録されたオブジェクトリストから、Emissiveマテリアルが割り当てられているものを抽出してライトリストに追加したい
    m_sphereLights.clear();
    m_rectLights.clear();

    // 球面光源
    if (m_pSphere)
    {
        if (auto lightMaterial = std::dynamic_pointer_cast<LightMaterial>(m_pSphere->GetMaterial()))
        {
            SphereLight light{};
            light.intensity = lightMaterial->GetEmissionColor();
            float radius = m_pSphere->GetRadius();
            light.area = 4.0f * M_PI * radius * radius;
            light.origin = m_pSphere->GetOrigin();
            light.radius = radius;
            m_sphereLights.push_back(light);
        }
    }

    // 矩形光源
    auto rectLight = {m_pRectLight1, m_pRectLight2, m_pDynamicRectLight};
    for (auto& rect : rectLight)
    {
        if (auto lightMaterial = std::dynamic_pointer_cast<LightMaterial>(rect->GetMaterial()))
        {
            RectLight light{};
            float width = rect->GetWidth();
            float height = rect->GetHeight();
            light.emissiveColor = lightMaterial->GetEmissionColor();
            light.area = width * height;
            light.origin = rect->GetOrigin();
            light.width = width;
            light.height = height;
            light.normal = rect->GetNormal();
            light.tangent = rect->GetTangent();
            light.bitangent = rect->GetBitangent();
            light.intensity = lightMaterial->GetEmissionLuminance();
            light.padding2 = 0;
            m_rectLights.push_back(light);
        }
    }
}

void Scene::OnAnimation(float timeDuration)
{
    // キーフレーム
    constexpr float START_TIME = 0.0f;
    constexpr float CAMERA_FRONT_END_TIME = 3.0f;
    constexpr float CAMERA_ZOOMIN_START_TIME = 2.0f;
    constexpr float CAMERA_ZOOMIN_END_TIME = 5.0f;
    constexpr float BUNNY_START_TIME = 2.0f;
    constexpr float BUNNY_END_TIME = 4.75f;
    constexpr float AMBIENT_COLOR_CHANGE_START_TIME = 5.0f;
    constexpr float AMBIENT_COLOR_CHANGE_END_TIME = 6.0f;
    constexpr float DYNAMIC_LIGHT_CHANGE_START_TIME = 5.5f;
    constexpr float DYNAMIC_LIGHT_CHANGE_END_TIME = 6.5f;
    constexpr float BUNNY_ROTATION_START_TIME = 8.5f;
    constexpr float BUNNY_ROTATION_END_TIME = 9.0;
    constexpr float END_TIME = 10.0f;
    // カメラ移動
    if (START_TIME < timeDuration && timeDuration <= CAMERA_FRONT_END_TIME)
    {
        constexpr XMVECTOR cameraStartPos{50.0f, 50.0f, -10.0f};
        constexpr XMVECTOR cameraEndPos{0.0f, 12.0f, -50.0f};
        float cameraDuration = CAMERA_FRONT_END_TIME - START_TIME;
        float t = (timeDuration - START_TIME) / cameraDuration;
        t = EaseInOutQuad(t);
        auto cameraPosVec = XMVectorLerp(cameraStartPos, cameraEndPos, t);
        Float3 cameraPos;
        XMStoreFloat3(&cameraPos, cameraPosVec);
        m_pCamera->SetOrigin(cameraPos);
    }
    // カメラズーム
    if (CAMERA_ZOOMIN_START_TIME < timeDuration && timeDuration <= CAMERA_ZOOMIN_END_TIME)
    {
        constexpr float cameraFovYStart = 45.0f;
        constexpr float cameraFovYEnd = 17.5f;
        float cameraDuration = CAMERA_ZOOMIN_END_TIME - CAMERA_ZOOMIN_START_TIME;
        float t = (timeDuration - CAMERA_ZOOMIN_START_TIME) / cameraDuration;
        t = EaseInOutQuad(t);
        float cameraFovY = Lerp(cameraFovYStart, cameraFovYEnd, t);
        m_pCamera->SetFovY(cameraFovY);
    }
    // Bunnyの移動処理
    if (timeDuration <= BUNNY_START_TIME)
    {
        // Bunnyを隠す
        m_pMesh->SetOrigin(Float3(0.0f, -5.0f, 0.0f));
    }
    else if (BUNNY_START_TIME < timeDuration && timeDuration <= BUNNY_END_TIME)
    {
        float bunnyDuration = BUNNY_END_TIME - BUNNY_START_TIME;
        float t = (timeDuration - BUNNY_START_TIME) / bunnyDuration;

        // 回転
        float rotY = Lerp(0.0f, -180.0f, t);
        m_pMesh->SetRotation(Float3(0.0f, rotY, 0.0f));

        // XZ平面上の動き
        const float R = 5.5f;
        const Float2 C = Float2(-5.0f, -2.5f);
        float angle = rotY * M_DEG2RAD;
        float posX = C.x + R * std::sin(angle);
        float posZ = C.y + R * std::cos(angle);

        // Y軸の動き
        constexpr float JUMP_COUNT = 5.0f;
        constexpr float MAX_JUMP_HEIGHT = 2.0f;
        float jump_factor = std::sin(t * M_PI * JUMP_COUNT);
        jump_factor = std::abs(jump_factor);
        float posY = jump_factor * MAX_JUMP_HEIGHT;

        Float3 bunnyPos{posX, posY, posZ};
        m_pMesh->SetOrigin(bunnyPos);
    }
    // 背景カラー変更
    if (AMBIENT_COLOR_CHANGE_START_TIME < timeDuration && timeDuration <= AMBIENT_COLOR_CHANGE_END_TIME)
    {
        constexpr XMVECTOR ambientColorStart{0.8f, 0.8f, 0.8f};
        constexpr XMVECTOR ambientColorEnd{0.01f, 0.01f, 0.01f};
        float ambientColorDuration = AMBIENT_COLOR_CHANGE_END_TIME - AMBIENT_COLOR_CHANGE_START_TIME;
        float t = (timeDuration - AMBIENT_COLOR_CHANGE_START_TIME) / ambientColorDuration;
        t = EaseInOutQuad(t);
        auto ambientColorVec = XMVectorLerp(ambientColorStart, ambientColorEnd, t);
        Float3 ambientColor;
        XMStoreFloat3(&ambientColor, ambientColorVec);
        m_ambientColor = ambientColor;
    }
    // ダイナミックライトのカラー変更
    if (DYNAMIC_LIGHT_CHANGE_START_TIME < timeDuration && timeDuration <= DYNAMIC_LIGHT_CHANGE_END_TIME)
    {
        const Float3 snowWhite = COL_SNOW;
        const Float3 blue = COL_BLUE;
        XMVECTOR dynamicLightColorStart = XMLoadFloat3(&snowWhite);
        XMVECTOR dynamicLightColorEnd = XMLoadFloat3(&blue);
        float dynamicLightIntensityStart = 15.0f;
        float dynamicLightIntensityEnd = 400.0f;
        float dynamicLightColorDuration = DYNAMIC_LIGHT_CHANGE_END_TIME - DYNAMIC_LIGHT_CHANGE_START_TIME;
        float t = (timeDuration - DYNAMIC_LIGHT_CHANGE_START_TIME) / dynamicLightColorDuration;
        t = EaseInOutQuad(t);
        // ライトカラー変化
        auto dynamicLightColorVec = XMVectorLerp(dynamicLightColorStart, dynamicLightColorEnd, t);
        Float3 dynamicLightColor;
        XMStoreFloat3(&dynamicLightColor, dynamicLightColorVec);
        // ライト強度変化
        float dynamicLightIntensity = Lerp(dynamicLightIntensityStart, dynamicLightIntensityEnd, t);
        SetDynamicRectLightColor(dynamicLightColor);
        SetDynamicRectLightIntensity(dynamicLightIntensity);
    }
}

void Scene::SetDynamicRectLightPosition(Float3 position)
{
    m_pDynamicRectLight->SetOrigin(position);
}

void Scene::SetDynamicRectLightSize(float size)
{
    m_pDynamicRectLight->SetSize(size);
}

void Scene::SetDynamicRectLightColor(Float3 color)
{
    m_pDynamicRectLightMaterial->SetEmissionColor(color);
}

void Scene::SetDynamicRectLightIntensity(float intensity)
{
    m_pDynamicRectLightMaterial->SetEmissionLuminance(intensity);
}

void Scene::UpdateDynamicLightParam()
{
    auto& dynamicLight = m_rectLights.back();
    dynamicLight.origin = m_pDynamicRectLight->GetOrigin();
    dynamicLight.normal = m_pDynamicRectLight->GetNormal();
    dynamicLight.tangent = m_pDynamicRectLight->GetTangent();
    dynamicLight.bitangent = m_pDynamicRectLight->GetBitangent();
    dynamicLight.area = m_pDynamicRectLight->GetHeight() * m_pDynamicRectLight->GetWidth();
    dynamicLight.width = m_pDynamicRectLight->GetWidth();
    dynamicLight.height = m_pDynamicRectLight->GetHeight();
    dynamicLight.intensity = m_pDynamicRectLightMaterial->GetEmissionLuminance();
    dynamicLight.emissiveColor = m_pDynamicRectLightMaterial->GetEmissionColor();
}

void Scene::UpdateLightBuffers(const std::unique_ptr<Device>& device)
{
    // DynamicLightのパラメータ更新
    UpdateDynamicLightParam();
    // 面光源バッファの更新
    if (!m_rectLights.empty() && m_pRectLightBuffer)
    {
        UINT bufferSize = static_cast<UINT>(sizeof(RectLight) * m_rectLights.size());
        device->WriteBuffer(m_pRectLightBuffer, m_rectLights.data(), bufferSize);
    }
}
