#pragma once

#include <string>
#include "Utils/MathUtil.h"

enum class MaterialType : UINT8
{
    Diffuse = 0u,
    Emissive = 1u,
    OpenPBR = 2u,
    Fluorescence = 3u,
};

struct MaterialParam
{
    // reference: https://academysoftwarefoundation.github.io/OpenPBR/#parameterreference
    // OpenPBR Base
    float baseWeight; // 4 bytes
    Float3 baseColor; // 12 bytes
    float baseMetalness; // 4 bytes
    float baseDiffuseRoughness; // 4 bytes

    // Fluorescent用の拡張
    float fluorWeight; // 4 bytes

    // OpenPBR Specular
    float specularWeight; // 4 bytes
    Float3 specularColor; // 12 bytes
    float specularRoughness; // 4 bytes
    float specularAnisotropy; // 4 bytes
    float specularIOR; // 4 bytes

    float padding2 = 0;

    // TODO: OpenPBR Transmission
    // TODO: OpenPBR Subsurface
    // TODO: OpenPBR Coat
    // TODO: OpenPBR Fuzz

    // OpenPBR Emission
    float emissionLuminance; // 4 bytes
    Float3 emissionColor; // 12 bytes

    // TODO: OpenPBR Thin Film
    // TODO: OpenPBR Geometry

    // Total: 24 + (4) + 28 + (4) + 16 = 76 bytes
};

// MaterialTypeを文字列に変換するヘルパー関数
inline std::wstring MaterialTypeToWStr(MaterialType type)
{
    switch (type)
    {
    case MaterialType::Diffuse:
        return L"Diffuse";
    case MaterialType::Emissive:
        return L"Emissive";
    case MaterialType::OpenPBR:
        return L"OpenPBR";
    case MaterialType::Fluorescence:
        return L"Fluorescence";
    default:
        return L"Diffuse";
    }
}

class Material
{
public:
    Material()
        : m_name(L"")
          , m_type(MaterialType::Diffuse)
          , m_rtInstanceMask(0xFF)
          , m_baseColor(1, 1, 1)
          , m_specularColor(0, 0, 0)
          , m_emissionColor(0, 0, 0)
    {
    }

    Material(const std::wstring& name, MaterialType type = MaterialType::Diffuse,
             const Float3& baseColor = Float3(1, 1, 1), const Float3& emissiveColor = Float3(0, 0, 0))
        : m_name(name)
          , m_type(type)
          , m_baseColor(baseColor)
          , m_specularColor(0, 0, 0)
          , m_emissionColor(emissiveColor)
    {
        // マスクなし
        m_rtInstanceMask = 0xFF;
    }

    virtual ~Material() = default;

    std::wstring GetName() const { return m_name; }
    MaterialType GetType() const { return m_type; }
    UINT GetRTInstanceMask() const { return m_rtInstanceMask; }
    Float3 GetDiffuseColor() const { return m_baseColor; }
    Float3 GetEmissionColor() const { return m_emissionColor; }

    void SetBaseColor(const Float3& color) { m_baseColor = color; }
    void SetEmissionColor(const Float3& emissive) { m_emissionColor = emissive; }

    virtual MaterialParam GetMaterialData() const
    {
        MaterialParam data{};
        // OpenPBR parameters
        data.baseWeight = 1.0f;
        data.baseColor = Float3(m_baseColor.x, m_baseColor.y, m_baseColor.z);
        data.baseMetalness = 0.0f;
        data.baseDiffuseRoughness = 0.0f;

        data.specularWeight = 1.0f;
        data.specularColor = Float3(m_specularColor.x, m_specularColor.y, m_specularColor.z);
        data.specularRoughness = 0.3f;
        data.specularIOR = 1.5f;
        data.specularAnisotropy = 0.0f;

        data.emissionLuminance = 0.0f;
        data.emissionColor = Float3(m_emissionColor.x, m_emissionColor.y, m_emissionColor.z);

        data.fluorWeight = 0.0f;

        return data;
    }

protected:
    std::wstring m_name;
    MaterialType m_type;
    UINT m_rtInstanceMask;
    Float3 m_baseColor;
    Float3 m_specularColor;
    Float3 m_emissionColor;
};
