#include "Rect.hpp"

#include "Scene/Materials/Material.hpp"
#include "Utils/DxrUtil.h"

::Rect::Rect(float width, float height, Float3 origin, Float3 normal, bool hasTex, const std::wstring& texPath)
{
    m_width = width;
    m_height = height;
    m_origin = origin;

    // Normal / Tangent / Bitangent
    SetupBasis(normal);

    m_hasTex = hasTex;
    m_texPath = L"";
    if (hasTex)
    {
        m_texPath = texPath;
    }
    m_worldMtx = XMMatrixTranslation(origin.x, origin.y, origin.z);
}

::Rect::Rect(float width, Float3 origin, Float3 target)
{
    m_width = width;
    m_height = width;
    m_origin = origin;

    m_target = target;

    // ターゲット方向を法線とする
    Float3 normal;
    XMStoreFloat3(&normal, XMVectorSubtract(XMLoadFloat3(&target), XMLoadFloat3(&origin)));

    // Normal / Tangent / Bitangent
    SetupBasis(normal);

    m_hasTex = false;
    m_texPath = L"";
    m_worldMtx = XMMatrixTranslation(origin.x, origin.y, origin.z);
}

::Rect::Rect(float width, Float3 origin)
{
    m_width = width;
    m_height = width;
    m_origin = origin;

    Float3 normal(-origin.x, -origin.y, -origin.z);
    m_normal = normal;

    // Normal / Tangent / Bitangent
    SetupBasis(normal);

    m_hasTex = false;
    m_texPath = L"";
    m_worldMtx = XMMatrixTranslation(origin.x, origin.y, origin.z);
}

void ::Rect::SetupBasis(Float3 normal)
{
    // Normal
    XMVECTOR normalVec = XMVector3Normalize(XMLoadFloat3(&normal));
    XMStoreFloat3(&m_normal, normalVec);

    // Tangent
    XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    XMVECTOR right = XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);
    // 法線が上方向と平行に近い場合は右方向を使用
    XMVECTOR tangentVec;
    float dotUp = fabsf(XMVectorGetX(XMVector3Dot(normalVec, up)));
    if (dotUp > 0.999f)
    {
        tangentVec = XMVector3Normalize(XMVector3Cross(normalVec, right));
    }
    else
    {
        tangentVec = XMVector3Normalize(XMVector3Cross(normalVec, up));
    }
    XMStoreFloat3(&m_tangent, tangentVec);

    // Bitangent
    XMVECTOR bitangentVec = XMVector3Normalize(XMVector3Cross(normalVec, tangentVec));
    XMStoreFloat3(&m_bitangent, bitangentVec);
}

void ::Rect::OnInit(const std::unique_ptr<Device>& device, const std::shared_ptr<Material>& material)
{
    Object::OnInit(device, material);
    // テクスチャがある場合は
    if (m_hasTex)
    {
        m_texture = LoadTexture(m_texPath, device);
    }
}

void ::Rect::OnUpdate(const std::unique_ptr<Device>& device)
{
    Object::OnUpdate(device);

    // ジオメトリバッファの更新
    UpdateGeometry(device);
}

void ::Rect::SetSize(float size)
{
    // FIXEME: サイズ更新ができるようにする
    m_width = m_height = size;
    // ジオメトリの再生成
    GenerateGeometry();
}

void ::Rect::UpdateGeometry(const std::unique_ptr<Device>& device)
{
    auto vtxSize = m_vertices.size() * sizeof(Vertex);
    device->WriteBuffer(m_pVertexBuffer, m_vertices.data(), vtxSize);
    auto idxSize = m_indices.size() * sizeof(UINT);
    device->WriteBuffer(m_pIndexBuffer, m_indices.data(), idxSize);
}

uint8_t* ::Rect::WriteHitGroupShaderRecord(uint8_t* dst, UINT hitGroupRecordSize,
                                           ComPtr<ID3D12StateObjectProperties> rtStateObjectProps)
{
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
    // テクスチャ（ある場合は実際のテクスチャ、ない場合はダミー）
    if (m_hasTex)
        dst += WriteGPUDescriptorHeap(dst, m_texture.srv); // t0
    else
        dst += WriteGPUDescriptorHeap(dst, m_dummyTexture.srv); // t0 (ダミー)

    dst = recordStart + hitGroupRecordSize;
    return dst;
}

void ::Rect::SetOrigin(Float3 origin)
{
    m_origin = origin;
    // マトリックス更新
    m_worldMtx = XMMatrixTranslation(m_origin.x, m_origin.y, m_origin.z);
}

// ジオメトリの生成
void ::Rect::GenerateGeometry()
{
    m_vertices.clear();
    m_indices.clear();

    float halfWidth = m_width * 0.5f;
    float halfHeight = m_height * 0.5f;

    XMVECTOR tangentVec = XMLoadFloat3(&m_tangent);
    XMVECTOR bitangentVec = XMLoadFloat3(&m_bitangent);

    Vertex vertices[4];
    XMVECTOR pos0 = XMVectorAdd(XMVectorScale(tangentVec, -halfWidth), XMVectorScale(bitangentVec, -halfHeight));
    XMStoreFloat3(&vertices[0].position, pos0);
    vertices[0].normal = m_normal;
    vertices[0].uv = Float2(0.0f, 0.0f);

    XMVECTOR pos1 = XMVectorAdd(XMVectorScale(tangentVec, halfWidth), XMVectorScale(bitangentVec, -halfHeight));
    XMStoreFloat3(&vertices[1].position, pos1);
    vertices[1].normal = m_normal;
    vertices[1].uv = Float2(1.0f, 0.0f);

    XMVECTOR pos2 = XMVectorAdd(XMVectorScale(tangentVec, halfWidth), XMVectorScale(bitangentVec, halfHeight));
    XMStoreFloat3(&vertices[2].position, pos2);
    vertices[2].normal = m_normal;
    vertices[2].uv = Float2(1.0f, 1.0f);

    XMVECTOR pos3 = XMVectorAdd(XMVectorScale(tangentVec, -halfWidth), XMVectorScale(bitangentVec, halfHeight));
    XMStoreFloat3(&vertices[3].position, pos3);
    vertices[3].normal = m_normal;
    vertices[3].uv = Float2(0.0f, 1.0f);

    for (int i = 0; i < 4; ++i)
    {
        m_vertices.push_back(vertices[i]);
    }

    // 三角形1
    m_indices.push_back(0);
    m_indices.push_back(1);
    m_indices.push_back(3);

    // 三角形2
    m_indices.push_back(1);
    m_indices.push_back(2);
    m_indices.push_back(3);

    // マトリックス更新
    m_worldMtx = XMMatrixTranslation(m_origin.x, m_origin.y, m_origin.z);
}
