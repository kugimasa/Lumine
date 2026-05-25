#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include "Mesh.hpp"
#include "Scene/Materials/Material.hpp"
#include "Utils/DxrUtil.h"
#include "Utils/TextureUtil.h"

Mesh::Mesh(const std::wstring& path, Float3 origin, Float3 rotation, Float3 scale) :
    m_path(path)
{
    m_origin = origin;
    m_rotation = rotation;
    m_scale = scale;
    UpdateWorldMatrix();
}

void Mesh::OnInit(const std::unique_ptr<Device>& device, const std::shared_ptr<Material>& material)
{
    Object::OnInit(device, material);
    if (!m_baseColorTexPath.empty())
    {
        m_texture = LoadTexture(m_baseColorTexPath, device);
    }
}

void Mesh::GenerateGeometry()
{
    LoadFromFile();
}

void Mesh::LoadFromFile()
{
    m_vertices.clear();
    m_indices.clear();

    // Assimpで読み込み（パスはUTF-8に変換して渡す）
    std::string pathUtf8;
    {
        // 簡易的に: wstring -> UTF-8
        int sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, m_path.c_str(), -1, nullptr, 0, nullptr, nullptr);
        pathUtf8.resize(sizeNeeded > 0 ? sizeNeeded - 1 : 0);
        if (sizeNeeded > 0)
            WideCharToMultiByte(CP_UTF8, 0, m_path.c_str(), -1, pathUtf8.data(), sizeNeeded, nullptr, nullptr);
    }

    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(pathUtf8,
                                             aiProcess_Triangulate |
                                             aiProcess_GenNormals |
                                             aiProcess_JoinIdenticalVertices |
                                             aiProcess_ImproveCacheLocality |
                                             aiProcess_OptimizeMeshes |
                                             aiProcess_OptimizeGraph |
                                             aiProcess_FlipUVs);
    if (!scene || !scene->HasMeshes())
    {
        Error(PrintInfoType::SCENE, "Faild to load mesh via Assimp:", importer.GetErrorString());
    }

    const aiMesh* mesh = scene->mMeshes[0];

    // テクスチャ情報を取得
    if (scene->HasMaterials() && mesh->mMaterialIndex < scene->mNumMaterials)
    {
        const aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];
        if (material->GetTextureCount(aiTextureType_DIFFUSE) > 0)
        {
            aiString texPath;
            if (material->GetTexture(aiTextureType_DIFFUSE, 0, &texPath) == AI_SUCCESS)
            {
                m_baseColorTexPath = StrToWStr(texPath.C_Str());
                Print(PrintInfoType::SCENE, L"テクスチャパス取得:" + m_baseColorTexPath);
            }
        }
    }

    m_vertices.resize(mesh->mNumVertices);
    for (unsigned i = 0; i < mesh->mNumVertices; ++i)
    {
        Vertex v{};
        if (mesh->HasPositions())
        {
            v.position = Float3(mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z);
        }
        if (mesh->HasNormals())
        {
            v.normal = Float3(mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z);
        }
        else
        {
            v.normal = Float3(0, 1, 0);
        }
        if (mesh->HasTextureCoords(0))
        {
            v.uv = Float2(mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y);
        }
        else
        {
            v.uv = Float2(0, 0);
        }
        m_vertices[i] = v;
    }

    // インデックス
    for (unsigned f = 0; f < mesh->mNumFaces; ++f)
    {
        const aiFace& face = mesh->mFaces[f];
        if (face.mNumIndices != 3) continue; // 三角形のみ
        m_indices.push_back(face.mIndices[0]);
        m_indices.push_back(face.mIndices[1]);
        m_indices.push_back(face.mIndices[2]);
    }
}

// シェーダーレコードの書き込み（テクスチャ対応）
uint8_t* Mesh::WriteHitGroupShaderRecord(uint8_t* dst, UINT hitGroupRecordSize,
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
    if (HasTexture())
        dst += WriteGPUDescriptorHeap(dst, m_texture.srv); // t0
    else
        dst += WriteGPUDescriptorHeap(dst, m_dummyTexture.srv); // t0 (ダミー)

    dst = recordStart + hitGroupRecordSize;
    return dst;
}
