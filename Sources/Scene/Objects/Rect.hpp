#pragma once

#include "Object.hpp"

// MEMO: DirectXTexを導入したことでRectが競合してしまう
// TODO: 名前空間を導入すべき
class Rect : public Object
{
public:
    Rect(float width, float height, Float3 origin, Float3 normal, bool hasTex, const std::wstring& texPath);
    Rect(float width, Float3 origin);
    Rect(float width, Float3 origin, Float3 target);

    ~Rect() = default;

    void OnInit(const std::unique_ptr<Device>& device, const std::shared_ptr<Material>& material = nullptr) override;
    void OnUpdate(const std::unique_ptr<Device>& device) override;
    uint8_t* WriteHitGroupShaderRecord(uint8_t* dst, UINT hitGroupRecordSize,
                                       ComPtr<ID3D12StateObjectProperties> rtStateObjectProps) override;

    void SetOrigin(Float3 origin) override;
    void SetSize(float size);
    void UpdateGeometry(const std::unique_ptr<Device>& device);
    float GetWidth() const { return m_width; }
    float GetHeight() const { return m_height; }
    Float3 GetNormal() const { return m_normal; }
    Float3 GetTangent() const { return m_tangent; }
    Float3 GetBitangent() const { return m_bitangent; }

protected:
    void GenerateGeometry() override;
    std::wstring GetObjectTypeName() const override { return L"Rect"; }

private:
    void SetupBasis(Float3 normal);

private:
    float m_width;
    float m_height;
    bool m_hasTex;
    Float3 m_target;
    Float3 m_normal;
    Float3 m_tangent{};
    Float3 m_bitangent{};

    // テクスチャリソース
    // TODO: テクスチャはマテリアル側に持たせたい
    std::wstring m_texPath;
    TextureResource m_texture;
};
