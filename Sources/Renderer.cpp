#include "Renderer.hpp"
#include "Window.hpp"
#include "Utils/DxrUtil.h"
#include "Utils/MathUtil.h"
#include "Utils/ShaderCompiler.h"

#include <memory>
#include <utility>
#include <imgui.h>
#include <imgui_impl_dx12.h>
#include <imgui_impl_win32.h>
#include <fpng.h>

using namespace DirectX;

Renderer::Renderer(UINT width, UINT height, std::wstring name, int maxFrame, bool hasWindow) :
    m_hasWindow(hasWindow),
    m_isRunning(false),
    m_width(width),
    m_height(height),
    m_currentFrame(0),
    m_maxFrame(maxFrame),
    m_name(std::move(name)),
    m_imGuiParam(),
    m_dispatchRayDesc(),
    m_keyState(),
    m_isMouseDragging(false),
    m_lastMousePos{0, 0},
    m_mouseSensitivity(0.0001f)
{
}

void Renderer::OnInit()
{
    Print(PrintInfoType::RENDERER, L"=======RENDERER START=======");
    if (m_hasWindow) { Print(PrintInfoType::RENDERER, L"--------[ON SCREEN]---------"); }
    m_isRunning = true;
    // アプリケーションの時間計測開始
    m_startTime = std::chrono::system_clock::now();
    m_lastFrameTime = m_startTime;

    // グラフィックデバイスの初期化
    if (!InitGraphicDevice(Window::GetHWND())) return;

    // 初期化関数内でBLASの構築
    m_pScene = std::make_shared<Scene>();
    m_pScene->OnInit(m_pDevice, GetAspect(), m_maxFrame);

    // グローバルルートシグネチャの用意
    CreateGlobalRootSignature();
    // ローカルルートシグネチャの用意
    CreateLocalRootSignature();
    // ステートオブジェクトの構築
    CreateStateObject();
    // 出力バッファの作成
    CreateOutputBuffer();
    // シェーダーテーブルの作成
    CreateShaderTable();

    // コマンドリストの用意
    m_pCmdList = m_pDevice->CreateCommandList();
    m_pCmdList->Close();

    if (m_hasWindow)
    {
        // ImGuiの初期化
        InitImGui();
    }
}

void Renderer::OnUpdate()
{
    if (!m_isRunning) return;
    // デルタタイムの計算
    auto currentTime = std::chrono::system_clock::now();
    float deltaTime = std::chrono::duration<float>(currentTime - m_lastFrameTime).count();
    m_lastFrameTime = currentTime;

    // BLAS/TLASの更新
    m_pScene->OnUpdate(m_pDevice, m_currentFrame, m_maxFrame);
    if (m_hasWindow)
    {
        // キー入力処理
        ProcessInput(deltaTime);
        UpdateImGui();
    }
}

void Renderer::OnRender()
{
    if (!m_isRunning) return;
    // 最後のフレームが描画されたら終了
    if (m_maxFrame > 0 && m_currentFrame >= m_maxFrame)
    {
        // アプリケーションの時間計測開始
        m_endTime = std::chrono::system_clock::now();
        // 経過時間の算出
        double elapsed = (double)std::chrono::duration_cast<std::chrono::milliseconds>(
            m_endTime - m_startTime).count();
        std::wstringstream timeWSS;
        timeWSS << L"Total time: " << elapsed * 0.001 << L"(sec)";
        Print(PrintInfoType::RENDERER, timeWSS.str());
        // 終了処理
        Print(PrintInfoType::RENDERER, L"======================");
        OnDestroy();
        // Window関連の処理
        if (m_hasWindow)
        {
            auto hwnd = Window::GetHWND();
            PostMessage(hwnd, WM_QUIT, 0, 0);
        }
        else
        {
            PostQuitMessage(0);
        }
        return;
    }
    // chrono変数
    std::chrono::system_clock::time_point start, end;
    // 時間計測開始
    start = std::chrono::system_clock::now();

    auto d3d12Device = m_pDevice->GetDevice();
    auto renderTarget = m_pDevice->GetRenderTarget();
    auto currentFrameIndex = m_pDevice->GetCurrentFrameIndex();
    auto allocator = m_pDevice->GetCurrentCommandAllocator();
    allocator->Reset();
    m_pCmdList->Reset(allocator.Get(), nullptr);

    ID3D12DescriptorHeap* descriptorHeaps[] = {
        m_pDevice->GetDescriptorHeap().Get(),
    };
    m_pCmdList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

    // グローバルルートシグネチャの設定
    m_pCmdList->SetComputeRootSignature(m_pGlobalRootSignature.Get());
    // TLAS: t0
    m_pCmdList->SetComputeRootDescriptorTable(0, m_pScene->GetTLASDescHeap().gpuHandle);
    // SphereLightBuffer: t1
    m_pCmdList->SetComputeRootDescriptorTable(1, m_pScene->GetSphereLightBufferHeap().gpuHandle);
    // RectLightBuffer: t2
    m_pCmdList->SetComputeRootDescriptorTable(2, m_pScene->GetRectLightBufferHeap().gpuHandle);
    // BgTex: t3
    m_pCmdList->SetComputeRootDescriptorTable(3, m_pScene->GetBackgroundTex().srv.gpuHandle);
    // SceneCB: b0
    m_pCmdList->SetComputeRootConstantBufferView(4, m_pScene->GetSceneCB(currentFrameIndex)->GetGPUVirtualAddress());

    // レイトレース結果をUAVへ
    auto barrierToUAV = CD3DX12_RESOURCE_BARRIER::Transition(
        m_pOutputBuffer.Get(),
        D3D12_RESOURCE_STATE_COPY_SOURCE,
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS
    );
    m_pCmdList->ResourceBarrier(1, &barrierToUAV);

    // レイトレース
    m_pCmdList->SetPipelineState1(m_pRTStateObject.Get());
    m_pCmdList->DispatchRays(&m_dispatchRayDesc);

    // レイトレース結果をバックバッファへコピー
    D3D12_RESOURCE_BARRIER barriers[] = {
        CD3DX12_RESOURCE_BARRIER::Transition(
            m_pOutputBuffer.Get(),
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
            D3D12_RESOURCE_STATE_COPY_SOURCE
        ),
        CD3DX12_RESOURCE_BARRIER::Transition(
            renderTarget.Get(),
            D3D12_RESOURCE_STATE_PRESENT,
            D3D12_RESOURCE_STATE_COPY_DEST
        ),
    };
    m_pCmdList->ResourceBarrier(_countof(barriers), barriers);
    m_pCmdList->CopyResource(renderTarget.Get(), m_pOutputBuffer.Get());

    if (m_hasWindow)
    {
        // ImGui描画用の設定
        auto barrierToRT = CD3DX12_RESOURCE_BARRIER::Transition(
            renderTarget.Get(),
            D3D12_RESOURCE_STATE_COPY_DEST,
            D3D12_RESOURCE_STATE_RENDER_TARGET
        );
        m_pCmdList->ResourceBarrier(1, &barrierToRT);

        // ImGuiの描画
        RenderImGui();

        // レンダーターゲットからPresentする
        auto barrierToPresent = CD3DX12_RESOURCE_BARRIER::Transition(
            renderTarget.Get(),
            D3D12_RESOURCE_STATE_RENDER_TARGET,
            D3D12_RESOURCE_STATE_PRESENT
        );
        m_pCmdList->ResourceBarrier(1, &barrierToPresent);
    }
    else
    {
        // Present可能なようにバリアをセット
        auto barrierToPresent = CD3DX12_RESOURCE_BARRIER::Transition(
            renderTarget.Get(),
            D3D12_RESOURCE_STATE_COPY_DEST,
            D3D12_RESOURCE_STATE_PRESENT
        );
        m_pCmdList->ResourceBarrier(1, &barrierToPresent);
    }

    m_pCmdList->Close();

    m_pDevice->ExecuteCommandList(m_pCmdList);
    m_pDevice->Present(1);

    // 最大フレーム指定がある場合にのみ画像出力
    if (m_maxFrame > 0)
    {
        // 画像用のバッファを作成
        auto imageBuffer = m_pDevice->CreateImageBuffer(
            renderTarget,
            D3D12_RESOURCE_STATE_PRESENT,
            D3D12_RESOURCE_STATE_PRESENT
        );
        // 画像の出力
        OutputImage(imageBuffer);
    }
    // 時間計測終了
    end = std::chrono::system_clock::now();
    // 経過時間の算出
    double elapsed = (double)std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    std::ostringstream timeOSS;
    timeOSS << "Frame: " << std::setw(3) << std::setfill('0') << m_currentFrame << " | " << elapsed * 0.001 << "(sec)";
    Print(PrintInfoType::RENDERER, StrToWStr(timeOSS.str()).c_str());
    // フレームの更新
    m_currentFrame++;
}

void Renderer::OnDestroy()
{
    if (!m_isRunning) return;
    m_isRunning = false;

    // シーンの破棄
    if (m_pScene)
    {
        m_pScene->OnDestroy(m_pDevice);
        m_pScene.reset();
    }

    // グラフィックデバイスの破棄
    if (m_pDevice)
    {
        m_pDevice->OnDestroy();
        m_pDevice.reset();
    }
}

bool Renderer::InitGraphicDevice(HWND hwnd)
{
    m_pDevice = std::make_unique<Device>();
    // グラフィックデバイスの初期化
    if (!m_pDevice->OnInit())
    {
        Error(PrintInfoType::RENDERER, L"グラフィックデバイスの初期化に失敗しました");
        return false;
    }
    // スワップチェインの作成
    if (!m_pDevice->CreateSwapChain(GetWidth(), GetHeight(), hwnd))
    {
        Error(PrintInfoType::RENDERER, L"スワップチェインの作成に失敗しました");
        return false;
    }
    Print(PrintInfoType::RENDERER, L"デバイスの初期化 完了");
    return true;
}

/// <summary>
/// グローバルルートシグネチャの作成
/// </summary>
void Renderer::CreateGlobalRootSignature()
{
    std::vector<D3D12_ROOT_PARAMETER> rootParams{};
    D3D12_ROOT_PARAMETER rootParam{};
    std::vector<D3D12_STATIC_SAMPLER_DESC> samplerDescs{};
    D3D12_STATIC_SAMPLER_DESC samplerDesc{};

    // TLAS: t0
    rootParam = CreateRootParam(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0);
    rootParams.push_back(rootParam);
    // SphereLightBuffer: t1
    rootParam = CreateRootParam(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1);
    rootParams.push_back(rootParam);
    // RectLightBuffer: t2
    rootParam = CreateRootParam(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2);
    rootParams.push_back(rootParam);
    // BgTex: t3
    rootParam = CreateRootParam(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 3);
    rootParams.push_back(rootParam);
    // SceneCB: b0
    rootParam = CreateRootParam(D3D12_ROOT_PARAMETER_TYPE_CBV, 0);
    rootParams.push_back(rootParam);
    // Sampler: s0
    samplerDesc = CreateStaticSamplerDesc(D3D12_FILTER_MIN_MAG_MIP_LINEAR, 0);
    samplerDescs.push_back(samplerDesc);

    // グローバルルートシグネチャの作成
    m_pGlobalRootSignature = m_pDevice->CreateRootSignature(rootParams, samplerDescs, L"GlobalRootSignature");
    Print(PrintInfoType::RENDERER, L"グローバルルートシグネチャ作成 完了");
}

/// <summary>
/// ローカルルートシグネチャの作成
/// </summary>
void Renderer::CreateLocalRootSignature()
{
    std::vector<D3D12_ROOT_PARAMETER> rootParams{};
    D3D12_ROOT_PARAMETER rootParam{};
    std::vector<D3D12_STATIC_SAMPLER_DESC> samplerDescs{};

    // RayGenシェーダー用のルートシグネチャ
    rootParams = {};
    rootParam = {};
    samplerDescs = {};
    // OutputBuffer : u0
    rootParam = CreateRootParam(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0);
    rootParams.push_back(rootParam);
    // ローカルルートシグネチャの作成
    m_pRayGenLocalRootSignature = m_pDevice->CreateRootSignature(rootParams, samplerDescs,
                                                                 L"LocalRootSignature:RayGen", /*isLocal*/ true);

    // ClosestHitシェーダー用のルートシグネチャ
    rootParams = {};
    rootParam = {};
    samplerDescs = {};

    //// Register Space 1 - ジオメトリデータ ////
    // IB: t0
    rootParam = CreateRootParam(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 1);
    rootParams.push_back(rootParam);
    // VB: t1
    rootParam = CreateRootParam(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1);
    rootParams.push_back(rootParam);

    //// Register Space 2 - マテリアルデータ ////
    // MatCB: b0
    rootParam = CreateRootParam(D3D12_ROOT_PARAMETER_TYPE_CBV, 0, 2);
    rootParams.push_back(rootParam);
    // DiffuseTex: t0
    rootParam = CreateRootParam(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 2);
    rootParams.push_back(rootParam);

    // ローカルルートシグネチャの作成
    m_pClosestHitLocalRootSignature = m_pDevice->CreateRootSignature(rootParams, samplerDescs,
                                                                     L"LocalRootSignature:ClosestHit", /*isLocal*/
                                                                     true);

    Print(PrintInfoType::RENDERER, L"ローカルルートシグネチャ作成 完了");
}

/// <summary>
/// ステートオブジェクトの構築
/// </summary>
void Renderer::CreateStateObject()
{
    // ステートオブジェクト設定
    CD3DX12_STATE_OBJECT_DESC stateObjDesc;
    stateObjDesc.SetStateObjectType(D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE);

    // シェーダ登録
    auto rayGenBin = SetupShader(L"Raygen");
    D3D12_SHADER_BYTECODE raygenShader{rayGenBin.data(), rayGenBin.size()};
    auto rayGenDXIL = stateObjDesc.CreateSubobject<CD3DX12_DXIL_LIBRARY_SUBOBJECT>();
    rayGenDXIL->SetDXILLibrary(&raygenShader);
    rayGenDXIL->DefineExport(L"RayGen");

    auto missBin = SetupShader(L"Miss");
    D3D12_SHADER_BYTECODE missShader{missBin.data(), missBin.size()};
    auto missDXIL = stateObjDesc.CreateSubobject<CD3DX12_DXIL_LIBRARY_SUBOBJECT>();
    missDXIL->SetDXILLibrary(&missShader);
    missDXIL->DefineExport(L"Miss");
    missDXIL->DefineExport(L"ShadowMiss");

    auto chDiffuseBin = SetupShader(L"ClosestHitDiffuse");
    D3D12_SHADER_BYTECODE chDiffuseShader{chDiffuseBin.data(), chDiffuseBin.size()};
    auto closestHitDXIL = stateObjDesc.CreateSubobject<CD3DX12_DXIL_LIBRARY_SUBOBJECT>();
    closestHitDXIL->SetDXILLibrary(&chDiffuseShader);
    closestHitDXIL->DefineExport(L"ClosestHit_Diffuse");

    auto chEmissiveBin = SetupShader(L"ClosestHitEmissive");
    D3D12_SHADER_BYTECODE chEmissiveShader{chEmissiveBin.data(), chEmissiveBin.size()};
    closestHitDXIL = stateObjDesc.CreateSubobject<CD3DX12_DXIL_LIBRARY_SUBOBJECT>();
    closestHitDXIL->SetDXILLibrary(&chEmissiveShader);
    closestHitDXIL->DefineExport(L"ClosestHit_Emissive");

    auto chOpenPbrBin = SetupShader(L"ClosestHitOpenpbr");
    D3D12_SHADER_BYTECODE chOpenPbrShader{chOpenPbrBin.data(), chOpenPbrBin.size()};
    closestHitDXIL = stateObjDesc.CreateSubobject<CD3DX12_DXIL_LIBRARY_SUBOBJECT>();
    closestHitDXIL->SetDXILLibrary(&chOpenPbrShader);
    closestHitDXIL->DefineExport(L"ClosestHit_OpenPBR");

    auto chFluorBin = SetupShader(L"ClosestHitFluorescence");
    D3D12_SHADER_BYTECODE chFluorShader{chFluorBin.data(), chFluorBin.size()};
    closestHitDXIL = stateObjDesc.CreateSubobject<CD3DX12_DXIL_LIBRARY_SUBOBJECT>();
    closestHitDXIL->SetDXILLibrary(&chFluorShader);
    closestHitDXIL->DefineExport(L"ClosestHit_Fluorescence");

    // ヒットグループ設定 (Diffuse)
    auto hitGroupDiffuse = stateObjDesc.CreateSubobject<CD3DX12_HIT_GROUP_SUBOBJECT>();
    hitGroupDiffuse->SetHitGroupType(D3D12_HIT_GROUP_TYPE_TRIANGLES);
    hitGroupDiffuse->SetClosestHitShaderImport(L"ClosestHit_Diffuse");
    hitGroupDiffuse->SetHitGroupExport(L"HitGroup_Diffuse");

    // ヒットグループ設定 (Emissive)
    auto hitGroupEmissive = stateObjDesc.CreateSubobject<CD3DX12_HIT_GROUP_SUBOBJECT>();
    hitGroupEmissive->SetHitGroupType(D3D12_HIT_GROUP_TYPE_TRIANGLES);
    hitGroupEmissive->SetClosestHitShaderImport(L"ClosestHit_Emissive");
    hitGroupEmissive->SetHitGroupExport(L"HitGroup_Emissive");

    // ヒットグループ設定 (OpenPBR)
    auto hitGroupOpenPBR = stateObjDesc.CreateSubobject<CD3DX12_HIT_GROUP_SUBOBJECT>();
    hitGroupOpenPBR->SetHitGroupType(D3D12_HIT_GROUP_TYPE_TRIANGLES);
    hitGroupOpenPBR->SetClosestHitShaderImport(L"ClosestHit_OpenPBR");
    hitGroupOpenPBR->SetHitGroupExport(L"HitGroup_OpenPBR");

    // ヒットグループ設定 (Fluorescence)
    auto hitGroupFluorescence = stateObjDesc.CreateSubobject<CD3DX12_HIT_GROUP_SUBOBJECT>();
    hitGroupFluorescence->SetHitGroupType(D3D12_HIT_GROUP_TYPE_TRIANGLES);
    hitGroupFluorescence->SetClosestHitShaderImport(L"ClosestHit_Fluorescence");
    hitGroupFluorescence->SetHitGroupExport(L"HitGroup_Fluorescence");

    // グローバルルートシグネチャ設定
    auto globalRootSig = stateObjDesc.CreateSubobject<CD3DX12_GLOBAL_ROOT_SIGNATURE_SUBOBJECT>();
    globalRootSig->SetRootSignature(m_pGlobalRootSignature.Get());

    // ローカルルートシグネチャ設定: RayGen
    auto rayGenLocalRootSig = stateObjDesc.CreateSubobject<CD3DX12_LOCAL_ROOT_SIGNATURE_SUBOBJECT>();
    rayGenLocalRootSig->SetRootSignature(m_pRayGenLocalRootSignature.Get());
    auto rgLocalRootSigExpAssoc = stateObjDesc.CreateSubobject<CD3DX12_SUBOBJECT_TO_EXPORTS_ASSOCIATION_SUBOBJECT>();
    rgLocalRootSigExpAssoc->AddExport(L"RayGen");
    rgLocalRootSigExpAssoc->SetSubobjectToAssociate(*rayGenLocalRootSig);
    // ローカルルートシグネチャ設定: ClosestHit (全マテリアルタイプのHitGroupに関連付け)
    // MEMO: HitGroup別にローカルルートシグネチャを作成する場合はこちらで作成
    auto closesHitLocalRootSig = stateObjDesc.CreateSubobject<CD3DX12_LOCAL_ROOT_SIGNATURE_SUBOBJECT>();
    closesHitLocalRootSig->SetRootSignature(m_pClosestHitLocalRootSignature.Get());
    auto chLocalRootSigExpAssoc = stateObjDesc.CreateSubobject<CD3DX12_SUBOBJECT_TO_EXPORTS_ASSOCIATION_SUBOBJECT>();
    chLocalRootSigExpAssoc->AddExport(L"HitGroup_Diffuse");
    chLocalRootSigExpAssoc->AddExport(L"HitGroup_Emissive");
    chLocalRootSigExpAssoc->AddExport(L"HitGroup_OpenPBR");
    chLocalRootSigExpAssoc->AddExport(L"HitGroup_Fluorescence");
    chLocalRootSigExpAssoc->SetSubobjectToAssociate(*closesHitLocalRootSig);

    // レイトレーシングパイプライン用設定
    // MEMO: common.hlsliと揃える
    const UINT MaxPayloadSize = sizeof(HitInfo);
    const UINT MaxAttributeSize = sizeof(XMFLOAT2);
    const UINT MaxRecursionDepth = 2; // 最大再帰段数

    // シェーダー設定
    auto rtShaderConfig = stateObjDesc.CreateSubobject<CD3DX12_RAYTRACING_SHADER_CONFIG_SUBOBJECT>();
    rtShaderConfig->Config(MaxPayloadSize, MaxAttributeSize);

    // パイプライン設定
    auto rtPipelineConfig = stateObjDesc.CreateSubobject<CD3DX12_RAYTRACING_PIPELINE_CONFIG_SUBOBJECT>();
    rtPipelineConfig->Config(MaxRecursionDepth);

    auto d3d12Device = m_pDevice->GetDevice();
    HRESULT hr = d3d12Device->CreateStateObject(
        stateObjDesc,
        IID_PPV_ARGS(m_pRTStateObject.ReleaseAndGetAddressOf())
    );
    if (FAILED(hr))
    {
        std::wstring err = &L"ステートオブジェクトの構築に失敗しました: "[(int)hr];
        Error(PrintInfoType::RENDERER, err);
    }
    Print(PrintInfoType::RENDERER, L"ステートオブジェクトの構築 完了");
}

/// <summary>
/// レイトレーシング結果の書き込み用バッファの作成
/// </summary>
void Renderer::CreateOutputBuffer()
{
    auto width = GetWidth();
    auto height = GetHeight();

    // 書き込み用バッファの作成
    m_pOutputBuffer = m_pDevice->CreateTexture2D(
        width, height,
        DXGI_FORMAT_R8G8B8A8_UNORM,
        D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
        D3D12_RESOURCE_STATE_COPY_SOURCE,
        D3D12_HEAP_TYPE_DEFAULT
    );

    // UAVの作成(TLAS 特有)
    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
    m_outputBufferDescHeap = m_pDevice->CreateUAV(m_pOutputBuffer.Get(), &uavDesc);
    Print(PrintInfoType::RENDERER, L"出力用バッファ(UAV)の作成 完了");
}

/// <summary>
/// シェーダーテーブルの構築
/// </summary>
void Renderer::CreateShaderTable()
{
    // RayGen: ShaderId
    UINT rayGenRecordSize = 0;
    rayGenRecordSize += D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
    rayGenRecordSize += sizeof(D3D12_GPU_DESCRIPTOR_HANDLE); // OutputBuffer: u0
    rayGenRecordSize = ROUND_UP(rayGenRecordSize, D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT);

    // Miss: ShaderId
    UINT missRecordSize = 0;
    missRecordSize += D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
    missRecordSize = ROUND_UP(missRecordSize, D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT);

    // HitGroup: ShaderId, IB, VB, MatCB, DiffuseTex
    UINT hitGroupRecordSize = 0;
    hitGroupRecordSize += D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
    hitGroupRecordSize += sizeof(D3D12_GPU_DESCRIPTOR_HANDLE); // IB: t0
    hitGroupRecordSize += sizeof(D3D12_GPU_DESCRIPTOR_HANDLE); // VB: t1
    hitGroupRecordSize += sizeof(D3D12_GPU_DESCRIPTOR_HANDLE); // MatCB: b0
    hitGroupRecordSize += sizeof(D3D12_GPU_DESCRIPTOR_HANDLE); // DiffuseTex: t0
    hitGroupRecordSize = ROUND_UP(hitGroupRecordSize, D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT);

    // シーン中のHitGroupの取得
    UINT hitGroupCount = m_pScene->GetTotalHitGroupCount();

    // シェーダーテーブルサイズを計算
    UINT rayGenSize = 1 * rayGenRecordSize;
    UINT missSize = 2 * missRecordSize;
    UINT hitGroupSize = hitGroupCount * hitGroupRecordSize;

    // 各テーブルでの開始位置のアライメント制約
    auto tableAlign = D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT;
    UINT rayGenEntrySize = ROUND_UP(rayGenSize, tableAlign);
    UINT missEntrySize = ROUND_UP(missSize, tableAlign);
    UINT hitGroupEntrySize = ROUND_UP(hitGroupSize, tableAlign);

    // シェーダーテーブルの確保
    auto tableSize = rayGenEntrySize + missEntrySize + hitGroupEntrySize;
    m_pShaderTable = m_pDevice->CreateBuffer(
        tableSize,
        D3D12_RESOURCE_FLAG_NONE,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        D3D12_HEAP_TYPE_UPLOAD
    );

    ComPtr<ID3D12StateObjectProperties> pRTStateObjectProps;
    if (m_pRTStateObject == nullptr)
    {
        Error(PrintInfoType::RENDERER, L"ステートオブジェクトが存在しません");
    }
    m_pRTStateObject.As(&pRTStateObjectProps);

    // 各シェーダーレコードの書き込み
    void* mapped = nullptr;
    m_pShaderTable->Map(0, nullptr, &mapped);
    uint8_t* pStart = static_cast<uint8_t*>(mapped);
    // RayGenシェーダー
    auto rayGenShaderStart = pStart;
    {
        uint8_t* p = rayGenShaderStart;
        std::wstring exportName = L"RayGen";
        auto id = pRTStateObjectProps->GetShaderIdentifier(exportName.c_str());
        if (id == nullptr)
        {
            auto message = L"シェーダーIDが見つかりません: " + exportName;
            Error(PrintInfoType::RENDERER, message);
        }
        p += WriteShaderId(p, id);
        // OutputBuffer: u0
        p += WriteGPUDescriptorHeap(p, m_outputBufferDescHeap);
    }

    // Missシェーダー
    auto missShaderStart = pStart + rayGenEntrySize;
    {
        uint8_t* p = missShaderStart;
        std::wstring exportName = L"Miss";
        auto id = pRTStateObjectProps->GetShaderIdentifier(exportName.c_str());
        if (id == nullptr)
        {
            auto message = L"シェーダーIDが見つかりません: " + exportName;
            Error(PrintInfoType::RENDERER, message);
        }
        p += WriteShaderId(p, id);

        // シャドウレイ用Missシェーダー
        p = missShaderStart + missRecordSize;
        exportName = L"ShadowMiss";
        id = pRTStateObjectProps->GetShaderIdentifier(exportName.c_str());
        if (id == nullptr)
        {
            auto message = L"シェーダーIDが見つかりません: " + exportName;
            Error(PrintInfoType::RENDERER, message);
        }
        p += WriteShaderId(p, id);
    }

    // HitGroup設定
    auto hitGroupStart = pStart + rayGenEntrySize + missEntrySize;
    {
        auto recordStart = hitGroupStart;
        recordStart = m_pScene->WriteHitGroupShaderRecord(recordStart, hitGroupRecordSize, m_pRTStateObject);
    }
    m_pShaderTable->Unmap(0, nullptr);
    Print(PrintInfoType::RENDERER, L"シェーダーテーブル作成 完了");

    // DispatchRays用の情報をセット
    auto& dispatchRayDesc = m_dispatchRayDesc;
    auto startAddress = m_pShaderTable->GetGPUVirtualAddress();
    auto& rayGenShaderRecord = dispatchRayDesc.RayGenerationShaderRecord;
    rayGenShaderRecord.StartAddress = startAddress;
    rayGenShaderRecord.SizeInBytes = rayGenSize;
    startAddress += rayGenEntrySize;

    auto& missShaderTable = dispatchRayDesc.MissShaderTable;
    missShaderTable.StartAddress = startAddress;
    missShaderTable.SizeInBytes = missSize;
    missShaderTable.StrideInBytes = missRecordSize;
    startAddress += missEntrySize;

    auto& hitGroupTable = dispatchRayDesc.HitGroupTable;
    hitGroupTable.StartAddress = startAddress;
    hitGroupTable.SizeInBytes = hitGroupSize;
    hitGroupTable.StrideInBytes = hitGroupRecordSize;
    startAddress += hitGroupEntrySize;

    dispatchRayDesc.Width = GetWidth();
    dispatchRayDesc.Height = GetHeight();
    dispatchRayDesc.Depth = 1;
    Print(PrintInfoType::RENDERER, L"DispatchRayDesc設定 完了");
}

void Renderer::OutputImage(ComPtr<ID3D12Resource> imageBuffer)
{
    //// CPU側で画像の出力
    std::ostringstream sout;
    sout << std::setw(3) << std::setfill('0') << m_currentFrame;
    std::string filename = OUTPUT_DIR + sout.str() + ".png";
    void* pixel = nullptr;
    imageBuffer->Map(0, nullptr, &pixel);
    fpng::fpng_encode_image_to_file(filename.c_str(), pixel, m_width, m_height, 4, 0);
    imageBuffer->Unmap(0, nullptr);
}

void Renderer::InitImGui()
{
    auto heap = m_pDevice->GetDescriptorHeap();
    m_imguiDescHeap = m_pDevice->AllocateDescriptorHeap();
    // バックエンド設定
    ImGui_ImplDX12_InitInfo initInfo{};
    initInfo.Device = m_pDevice->GetDevice().Get();
    initInfo.CommandQueue = m_pDevice->GetCommandQueue().Get();
    initInfo.NumFramesInFlight = Device::BackBufferCount;
    initInfo.RTVFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    initInfo.DSVFormat = DXGI_FORMAT_UNKNOWN;
    initInfo.SrvDescriptorHeap = heap.Get();
    initInfo.LegacySingleSrvCpuDescriptor = m_imguiDescHeap.cpuHandle;
    initInfo.LegacySingleSrvGpuDescriptor = m_imguiDescHeap.gpuHandle;

    // MEMO: ImGui ver 1.92 から初期化方法が変わった
    ImGui_ImplDX12_Init(&initInfo);

    // シーン関連のパラメータセット
    m_imGuiParam.cameraMoveSpeed = m_pScene->GetCamera()->GetMoveSpeed();
    m_imGuiParam.cameraRotateSpeed = m_pScene->GetCamera()->GetRotateSpeed();
    m_imGuiParam.cameraFovY = m_pScene->GetCamera()->GetFovY();
    m_imGuiParam.cameraPos = m_pScene->GetCamera()->GetOrigin();
    m_imGuiParam.cameraTarget = m_pScene->GetCamera()->GetTarget();
    m_imGuiParam.maxPathDepth = m_pScene->GetMaxPathDepth();
    m_imGuiParam.maxSPP = m_pScene->GetMaxSPP();
    m_imGuiParam.ambientColor = m_pScene->GetAmbientColor();
    m_imGuiParam.iblPower = m_pScene->GetIblPower();
    m_imGuiParam.whitePoint = m_pScene->GetWhitePoint();
    Print(PrintInfoType::RENDERER, L"ImGui初期化完了");
}

void Renderer::UpdateImGui()
{
    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    auto frameRate = ImGui::GetIO().Framerate;
    ImGui::Begin("Info");
    ImGui::Text("Framerate %.3f ms", 1000.0f / frameRate);

    // シーン関連のパラメータ更新
    // カメラパラメータ
    ImGui::Text("Camera");
    std::shared_ptr<Camera> pCamera = m_pScene->GetCamera();
    m_imGuiParam.cameraPos = pCamera->GetOrigin();
    ImGui::SliderFloat3("Position", &m_imGuiParam.cameraPos.x, -50.0, 50.0);
    m_imGuiParam.cameraTarget = pCamera->GetTarget();
    ImGui::SliderFloat3("Target", &m_imGuiParam.cameraTarget.x, -50.0, 50.0);
    m_imGuiParam.cameraMoveSpeed = pCamera->GetMoveSpeed();
    ImGui::SliderFloat("Move Speed", &m_imGuiParam.cameraMoveSpeed, 0.0, 100.0);
    m_imGuiParam.cameraRotateSpeed = pCamera->GetRotateSpeed();
    ImGui::SliderFloat("Rotate Speed", &m_imGuiParam.cameraRotateSpeed, 0.0, 10.0);
    m_imGuiParam.cameraFovY = pCamera->GetFovY();
    ImGui::SliderFloat("FOV", &m_imGuiParam.cameraFovY, 5.0f, 90.0f);

    // Bunny
    ImGui::Text("Bunny");
    std::shared_ptr<Mesh> pBunny = m_pScene->GetMesh();
    m_imGuiParam.bunnyPos = pBunny->GetOrigin();
    ImGui::SliderFloat3("Bunny Position", &m_imGuiParam.bunnyPos.x, -10.0, 10.0);
    m_imGuiParam.bunnyRotY = pBunny->GetRotation().y;
    ImGui::SliderFloat("Bunny RotY", &m_imGuiParam.bunnyRotY, 0.0, 360.0);

    // FluorRect
    ImGui::Text("Fluor Rect");
    std::shared_ptr<::Rect> pFluorRect = m_pScene->GetFluorRect();
    m_imGuiParam.fluorBaseColor = pFluorRect->GetMaterial()->GetDiffuseColor();
    ImGui::ColorEdit3("FluorRect Albedo", &m_imGuiParam.fluorBaseColor.x);

    // ライトパラメータ
    ImGui::Text("Light");
    m_imGuiParam.dynamicLightPos = m_pScene->GetDynamicRectLightPosition();
    ImGui::SliderFloat3("Rect Light Position", &m_imGuiParam.dynamicLightPos.x, -50.0, 50.0);
    // TODO: ライトサイズ調整の実装
    // m_imGuiParam.dynamicLightSize = m_pScene->GetDynamicRectLightSize();
    // ImGui::SliderFloat("Rect Light Size", &m_imGuiParam.dynamicLightSize, 0.1, 50.0);
    m_imGuiParam.dynamicLightColor = m_pScene->GetDynamicRectLightColor();
    ImGui::ColorEdit3("Rect Light Color", &m_imGuiParam.dynamicLightColor.x);
    m_imGuiParam.dynamicLightIntensity = m_pScene->GetDynamicRectLightIntensity();
    ImGui::SliderFloat("Rect Light Intensity", &m_imGuiParam.dynamicLightIntensity, 0.0, 1000.0);

    m_imGuiParam.ambientColor = m_pScene->GetAmbientColor();
    ImGui::ColorEdit3("Ambient Color", &m_imGuiParam.ambientColor.x);
    m_imGuiParam.iblPower = m_pScene->GetIblPower();
    ImGui::SliderFloat("IBL Power", &m_imGuiParam.iblPower, 0.0f, 2.0f);
    m_imGuiParam.whitePoint = m_pScene->GetWhitePoint();
    ImGui::SliderFloat("White Point", &m_imGuiParam.whitePoint, 1.0f, 20.0f);

    // レイトレパラメータ
    ImGui::Text("RayTracing");
    // 反射回数
    m_imGuiParam.maxPathDepth = m_pScene->GetMaxPathDepth();
    ImGui::SliderInt("Max Path Depth", &m_imGuiParam.maxPathDepth, 1, 32);
    // SPP
    m_imGuiParam.maxSPP = m_pScene->GetMaxSPP();
    ImGui::SliderInt("Max SPP", &m_imGuiParam.maxSPP, 1, 1000);
    ImGui::End();

    // 更新
    pCamera->SetOrigin(m_imGuiParam.cameraPos);
    pCamera->SetTarget(m_imGuiParam.cameraTarget);
    pCamera->SetMoveSpeed(m_imGuiParam.cameraMoveSpeed);
    pCamera->SetRotateSpeed(m_imGuiParam.cameraRotateSpeed);
    pCamera->SetFovY(m_imGuiParam.cameraFovY);
    pBunny->SetOrigin(m_imGuiParam.bunnyPos);
    pBunny->SetRotation(Float3(0.0f, m_imGuiParam.bunnyRotY, 0.0f));
    pFluorRect->GetMaterial()->SetBaseColor(m_imGuiParam.fluorBaseColor);
    m_pScene->SetMaxPathDepth(m_imGuiParam.maxPathDepth);
    m_pScene->SetMaxSPP(m_imGuiParam.maxSPP);
    m_pScene->SetDynamicRectLightPosition(m_imGuiParam.dynamicLightPos);
    // m_pScene->SetDynamicRectLightSize(m_imGuiParam.dynamicLightSize);
    m_pScene->SetDynamicRectLightColor(m_imGuiParam.dynamicLightColor);
    m_pScene->SetDynamicRectLightIntensity(m_imGuiParam.dynamicLightIntensity);
    m_pScene->SetAmbientColor(m_imGuiParam.ambientColor);
    m_pScene->SetIblPower(m_imGuiParam.iblPower);
    m_pScene->SetWhitePoint(m_imGuiParam.whitePoint);
}

void Renderer::RenderImGui()
{
    auto rtvHandle = m_pDevice->GetCurrentRTVDesc();
    auto viewport = m_pDevice->GetViewport();
    m_pCmdList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);
    m_pCmdList->RSSetViewports(1, &viewport);

    ImGui::Render();
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), m_pCmdList.Get());
}

void Renderer::DestroyImGui()
{
    ImGui_ImplDX12_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
}

void Renderer::OnKeyDown(WPARAM key)
{
    switch (key)
    {
    case 'W': m_keyState.W = true;
        break;
    case 'A': m_keyState.A = true;
        break;
    case 'S': m_keyState.S = true;
        break;
    case 'D': m_keyState.D = true;
        break;
    case 'Q': m_keyState.Q = true;
        break;
    case 'E': m_keyState.E = true;
        break;
    default: break;
    }
}

void Renderer::OnKeyUp(WPARAM key)
{
    switch (key)
    {
    case 'W': m_keyState.W = false;
        break;
    case 'A': m_keyState.A = false;
        break;
    case 'S': m_keyState.S = false;
        break;
    case 'D': m_keyState.D = false;
        break;
    case 'Q': m_keyState.Q = false;
        break;
    case 'E': m_keyState.E = false;
        break;
    default: break;
    }
}

void Renderer::OnMouseMove(int x, int y)
{
    if (!m_isMouseDragging)
    {
        m_lastMousePos.x = x;
        m_lastMousePos.y = y;
        return;
    }

    // マウスの移動量を計算
    int deltaX = x - m_lastMousePos.x;
    int deltaY = y - m_lastMousePos.y;
    m_lastMousePos.x = x;
    m_lastMousePos.y = y;

    // ImGuiがマウス入力を使用している場合はスキップ
    if (m_hasWindow && ImGui::GetIO().WantCaptureMouse) return;

    if (!m_pScene || !m_pScene->GetCamera()) return;

    auto camera = m_pScene->GetCamera();
    auto rotateSpeed = camera->GetRotateSpeed();

    // マウスの移動量を回転角度に変換
    float yaw = -deltaX * rotateSpeed * m_mouseSensitivity; // 左右回転（反転）
    float pitch = -deltaY * rotateSpeed * m_mouseSensitivity; // 上下回転（反転）

    camera->Rotate(yaw, pitch);
}

void Renderer::OnMouseLeftDown()
{
    // ImGuiがマウス入力を使用している場合はスキップ
    if (m_hasWindow && ImGui::GetIO().WantCaptureMouse) return;
    m_isMouseDragging = true;
}

void Renderer::OnMouseLeftUp()
{
    m_isMouseDragging = false;
}

void Renderer::ProcessInput(float deltaTime)
{
    if (!m_pScene || !m_pScene->GetCamera()) return;

    // ImGuiがキーボード入力を使用している場合はスキップ
    if (m_hasWindow && ImGui::GetIO().WantCaptureKeyboard) return;

    auto camera = m_pScene->GetCamera();
    float moveDistance = camera->GetMoveSpeed() * deltaTime;

    // カメラの方向ベクトルを取得
    Float3 forward = camera->GetForward();
    Float3 right = camera->GetRight();
    Float3 up = camera->GetUp();

    // 移動処理
    if (m_keyState.W) camera->Move(forward, moveDistance);
    if (m_keyState.S) camera->Move(forward, -moveDistance);
    if (m_keyState.D) camera->Move(right, -moveDistance);
    if (m_keyState.A) camera->Move(right, moveDistance);
    if (m_keyState.E) camera->Move(up, moveDistance);
    if (m_keyState.Q) camera->Move(up, -moveDistance);
}
