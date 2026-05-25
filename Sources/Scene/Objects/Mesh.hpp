#pragma once

#include "Object.hpp"
#include "Utils/TextureUtil.h"
#include <string>

class Mesh : public Object
{
public:
    explicit Mesh(const std::wstring& path,
                  Float3 origin = Float3(0.0f, 0.0f, 0.0f),
                  Float3 rotation = Float3(0.0f, 0.0f, 0.0f),
                  Float3 scale = Float3(1.0f, 1.0f, 1.0f));
    ~Mesh() = default;

    bool HasTexture() const { return m_texture.resource != nullptr; }

    // 初期化をオーバーライド（テクスチャロードのため）
    void OnInit(const std::unique_ptr<Device>& device, const std::shared_ptr<Material>& material = nullptr) override;

    // シェーダーレコード書き込みをオーバーライド（テクスチャ対応）
    uint8_t* WriteHitGroupShaderRecord(uint8_t* dst, UINT hitGroupRecordSize,
                                       ComPtr<ID3D12StateObjectProperties> rtStateObjectProps) override;

protected:
    void GenerateGeometry() override;
    std::wstring GetObjectTypeName() const override { return L"Mesh"; }

private:
    // ファイルからモデルデータを読み込み
    void LoadFromFile();

private:
    std::wstring m_path;
    std::wstring m_baseColorTexPath;

    // テクスチャリソース
    TextureResource m_texture;
};
