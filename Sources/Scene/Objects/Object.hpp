#pragma once

#include "Device.hpp"
#include "Utils/GeoUtil.h"
#include "Utils/TextureUtil.h"

class Material;

class Object
{
public:
    virtual ~Object() = default;

    // 初期化
    virtual void OnInit(const std::unique_ptr<Device>& device, const std::shared_ptr<Material>& material = nullptr);

    // 更新
    virtual void OnUpdate(const std::unique_ptr<Device>& device);

    // BLASの構築
    virtual void BuildBLAS(const std::unique_ptr<Device>& device);
    // BLASの更新
    virtual void UpdateBLAS(const ComPtr<ID3D12GraphicsCommandList4>& cmdList);
    // HitGroup用シェーダーレコードの書き込み
    virtual uint8_t* WriteHitGroupShaderRecord(uint8_t* dst, UINT hitGroupRecordSize,
                                               ComPtr<ID3D12StateObjectProperties> rtStateObjectProps);

    // ダミーテクスチャのロード
    void LoadDummyTexture(const std::unique_ptr<Device>& device);

    // ジオメトリデータ
    UINT GetVertexCount() const { return static_cast<UINT>(m_vertices.size()); }
    UINT GetIndexCount() const { return static_cast<UINT>(m_indices.size()); }
    UINT GetTriangleCount() const { return GetIndexCount() / 3; }

    // TRS
    virtual void SetOrigin(Float3 origin);
    virtual void SetRotation(Float3 rotation);
    virtual void SetScale(Float3 scale);
    Float3 GetOrigin() const { return m_origin; }
    Float3 GetRotation() const;
    Float3 GetScale() const { return m_scale; }
    Matrix GetWorldMtx() const { return m_worldMtx; }
    void SetWorldMtx(const Matrix& worldMtx) { m_worldMtx = worldMtx; }

    ComPtr<ID3D12Resource> GetBLAS() { return m_pBLAS; }

    // マテリアル関連
    std::shared_ptr<Material> GetMaterial() const { return m_pMaterial; }
    UINT GetHitGroupCount() const { return 1; }

protected:
    // ジオメトリの設定は各派生クラスで実装
    virtual void GenerateGeometry() = 0;
    virtual std::wstring GetObjectTypeName() const = 0;

    // ジオメトリ情報の設定
    virtual D3D12_RAYTRACING_GEOMETRY_DESC& CreateGeometryDesc();

    // 行列の更新
    void UpdateWorldMatrix();

protected:
    // ジオメトリデータ
    std::vector<Vertex> m_vertices;
    std::vector<UINT> m_indices;

    // マテリアル
    std::shared_ptr<Material> m_pMaterial;

    // TRS
    Float3 m_origin;
    Float3 m_rotation;
    Float3 m_scale;

    Matrix m_worldMtx;

    // D3D12リソース
    ComPtr<ID3D12Resource> m_pVertexBuffer;
    ComPtr<ID3D12Resource> m_pIndexBuffer;
    ComPtr<ID3D12Resource> m_pBLAS;
    ComPtr<ID3D12Resource> m_pBLASUpdateBuffer;
    ComPtr<ID3D12Resource> m_pMaterialBuffer;

    // SRV用のディスクリプタヒープ
    DescriptorHeap m_vertexBufferHeap;
    DescriptorHeap m_indexBufferHeap;
    TextureResource m_dummyTexture;
};
