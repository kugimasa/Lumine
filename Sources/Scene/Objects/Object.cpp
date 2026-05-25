#include "Object.hpp"
#include "Scene/Materials/Material.hpp"
#include "Utils/DxrUtil.h"

// 初期化
void Object::OnInit(const std::unique_ptr<Device>& device, const std::shared_ptr<Material>& material)
{
    m_pMaterial = material;
    // ジオメトリの生成（派生クラスで実装）
    GenerateGeometry();

    auto flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    auto heapType = D3D12_HEAP_TYPE_DEFAULT;

    // 頂点バッファの作成
    auto vtxSize = m_vertices.size() * sizeof(Vertex);
    std::wstring vtxBufferName = GetObjectTypeName() + L"VertexBuffer";
    m_pVertexBuffer = device->InitializeBuffer(vtxSize, m_vertices.data(),
                                               flags, heapType,
                                               vtxBufferName.c_str());

    // インデックスバッファの作成
    auto idxSize = m_indices.size() * sizeof(UINT);
    std::wstring idxBufferName = GetObjectTypeName() + L"IndexBuffer";
    m_pIndexBuffer = device->InitializeBuffer(idxSize, m_indices.data(),
                                              flags, heapType,
                                              idxBufferName.c_str());

    // SRV生成
    m_vertexBufferHeap = device->CreateSRV(m_pVertexBuffer, m_vertices.size(), 0, sizeof(Vertex));
    m_indexBufferHeap = device->CreateSRV(m_pIndexBuffer, m_indices.size(), 0, sizeof(UINT));

    // マテリアルバッファの作成
    MaterialParam materialData = m_pMaterial->GetMaterialData();
    if (device->CreateConstantBuffer(m_pMaterialBuffer, sizeof(MaterialParam), L"MatCB"))
    {
        device->WriteBuffer(m_pMaterialBuffer, &materialData, sizeof(MaterialParam));
    }

    // ダミーテクスチャのロード
    LoadDummyTexture(device);

    // BLASの構築
    BuildBLAS(device);

    std::wstring completionMsg = GetObjectTypeName() + L"初期化完了";
    Print(PrintInfoType::SCENE, completionMsg);
}

void Object::OnUpdate(const std::unique_ptr<Device>& device)
{
    // マテリアルパラメータの更新
    MaterialParam materialData = m_pMaterial->GetMaterialData();
    device->WriteBuffer(m_pMaterialBuffer, &materialData, sizeof(MaterialParam));
}

D3D12_RAYTRACING_GEOMETRY_DESC& Object::CreateGeometryDesc()
{
    auto geometryDesc = new D3D12_RAYTRACING_GEOMETRY_DESC{};
    geometryDesc->Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
    geometryDesc->Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;

    auto& triangles = geometryDesc->Triangles;
    triangles.VertexBuffer.StartAddress = m_pVertexBuffer->GetGPUVirtualAddress();
    triangles.VertexBuffer.StrideInBytes = sizeof(Vertex);
    triangles.VertexCount = GetVertexCount();
    triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
    triangles.IndexBuffer = m_pIndexBuffer->GetGPUVirtualAddress();
    triangles.IndexCount = GetIndexCount();
    triangles.IndexFormat = DXGI_FORMAT_R32_UINT;
    return *geometryDesc;
}

// BLASの構築
void Object::BuildBLAS(const std::unique_ptr<Device>& device)
{
    // BLASの情報をセット
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC buildASDesc{};
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS& inputs = buildASDesc.Inputs;
    inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
    inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE;
    inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
    inputs.pGeometryDescs = &CreateGeometryDesc();
    inputs.NumDescs = 1;

    // BLAS関連のバッファを確保
    std::wstring blasName = GetObjectTypeName() + L"BLAS";
    auto blas = CreateASBuffers(device, buildASDesc, blasName.c_str());
    m_pBLAS = blas.asBuffer;
    m_pBLASUpdateBuffer = blas.updateBuffer;
    buildASDesc.ScratchAccelerationStructureData = blas.scratchBuffer->GetGPUVirtualAddress();
    buildASDesc.DestAccelerationStructureData = blas.asBuffer->GetGPUVirtualAddress();

    // BLAS構築コマンド
    auto cmdList = device->CreateCommandList();
    cmdList->BuildRaytracingAccelerationStructure(&buildASDesc, 0, nullptr);
    auto barrier = CD3DX12_RESOURCE_BARRIER::UAV(m_pBLAS.Get());
    cmdList->ResourceBarrier(1, &barrier);
    cmdList->Close();

    // コマンド実行 - BLAS構築
    device->ExecuteCommandList(cmdList);
    // コマンドの完了を待機
    device->WaitForGpu();

    std::wstring blasCompletionMsg = GetObjectTypeName() + L"用BLAS構築完了";
    Print(PrintInfoType::SCENE, blasCompletionMsg);
}

// BLASの更新
void Object::UpdateBLAS(const ComPtr<ID3D12GraphicsCommandList4>& cmdList)
{
    // BLASの情報をセット
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC buildASDesc{};
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS& inputs = buildASDesc.Inputs;
    inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
    // BLAS更新のためフラグを設定
    inputs.Flags =
        D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE |
        D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PERFORM_UPDATE;
    inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
    inputs.pGeometryDescs = &CreateGeometryDesc();
    inputs.NumDescs = 1;

    // BLAS更新用のアドレス設定
    buildASDesc.DestAccelerationStructureData = m_pBLAS->GetGPUVirtualAddress();
    buildASDesc.SourceAccelerationStructureData = m_pBLAS->GetGPUVirtualAddress();
    buildASDesc.ScratchAccelerationStructureData = m_pBLASUpdateBuffer->GetGPUVirtualAddress();

    // BLAS再構築
    cmdList->BuildRaytracingAccelerationStructure(&buildASDesc, 0, nullptr);
    auto barrier = CD3DX12_RESOURCE_BARRIER::UAV(m_pBLAS.Get());
    cmdList->ResourceBarrier(1, &barrier);
}

// ダミーテクスチャのロード
void Object::LoadDummyTexture(const std::unique_ptr<Device>& device)
{
    m_dummyTexture = LoadTexture(L"DummyWhite.png", device);
}

// シェーダーレコードの書き込み
uint8_t*
Object::WriteHitGroupShaderRecord(uint8_t* dst, UINT hitGroupRecordSize,
                                  ComPtr<ID3D12StateObjectProperties> rtStateObjectProps)
{
    // ローカルルートシグネチャの順番と合わせる
    auto recordStart = dst;

    // マテリアルタイプに応じたHitGroupIDを取得
    std::wstring hitGroupName = L"HitGroup_" + MaterialTypeToWStr(m_pMaterial->GetType());
    auto id = rtStateObjectProps->GetShaderIdentifier(hitGroupName.c_str());
    if (id == nullptr)
    {
        std::wstring errorMsg = L"ShaderIdが設定されていません: " + hitGroupName;
        Error(PrintInfoType::SCENE, errorMsg);
    }
    dst += WriteShaderId(dst, id);
    // Register Space 1
    dst += WriteGPUDescriptorHeap(dst, m_indexBufferHeap); // t0
    dst += WriteGPUDescriptorHeap(dst, m_vertexBufferHeap); // t1
    // Register Space 2
    dst += WriteGPUResourceAddress(dst, m_pMaterialBuffer); // b0
    dst += WriteGPUDescriptorHeap(dst, m_dummyTexture.srv); // t0 (ダミーテクスチャ)
    dst = recordStart + hitGroupRecordSize;
    return dst;
}

void Object::SetOrigin(Float3 origin)
{
    m_origin = origin;
    UpdateWorldMatrix();
}

void Object::SetRotation(Float3 rotation)
{
    m_rotation = Float3(rotation.x * M_DEG2RAD, rotation.y * M_DEG2RAD, rotation.z * M_DEG2RAD);
    UpdateWorldMatrix();
}

void Object::SetScale(Float3 scale)
{
    m_scale = scale;
    UpdateWorldMatrix();
}

Float3 Object::GetRotation() const
{
    return {m_rotation.x * M_RAD2DEG, m_rotation.y * M_RAD2DEG, m_rotation.z * M_RAD2DEG};
}

void Object::UpdateWorldMatrix()
{
    XMMATRIX scaleMtx = XMMatrixScaling(m_scale.x, m_scale.y, m_scale.z);
    XMMATRIX rotMtx = XMMatrixRotationRollPitchYaw(m_rotation.x, m_rotation.y, m_rotation.z);
    XMMATRIX transMtx = XMMatrixTranslation(m_origin.x, m_origin.y, m_origin.z);
    m_worldMtx = scaleMtx * rotMtx * transMtx;
}
