#pragma once

#include "Material.hpp"

class OpenPBRMaterial : public Material
{
public:
    OpenPBRMaterial(const std::wstring& name,
                    const Float3& baseColor = Float3(0.8f, 0.8f, 0.8f))
        : m_baseWeight(1.0f)
          , m_baseMetalness(0.0f)
          , m_baseDiffuseRoughness(0.0f)
          , m_specularWeight(1.0f)
          , m_specularRoughness(0.3f)
          , m_specularIOR(1.5f)
          , m_specularAnisotropy(0.0f)
          , m_emissionLuminance(0.0f)
    {
        m_name = name;
        m_type = MaterialType::OpenPBR;
        m_baseColor = baseColor;
        m_specularColor = Float3(1.0f, 1.0f, 1.0f);
        m_emissionColor = Float3(0.0f, 0.0f, 0.0f);
        m_rtInstanceMask = 0xFF;
    }

    // Base
    Float3 GetBaseColor() const { return m_baseColor; }
    void SetBaseColor(const Float3& color) { m_baseColor = color; }

    float GetBaseWeight() const { return m_baseWeight; }
    void SetBaseWeight(float value) { m_baseWeight = value; }

    float GetBaseMetalness() const { return m_baseMetalness; }
    void SetBaseMetalness(float value) { m_baseMetalness = value; }

    float GetBaseDiffuseRoughness() const { return m_baseDiffuseRoughness; }
    void SetBaseDiffuseRoughness(float value) { m_baseDiffuseRoughness = value; }

    // Specular Layer getters/setters
    float GetSpecularWeight() const { return m_specularWeight; }
    void SetSpecularWeight(float value) { m_specularWeight = value; }

    float GetSpecularRoughness() const { return m_specularRoughness; }
    void SetSpecularRoughness(float value) { m_specularRoughness = value; }

    float GetSpecularIOR() const { return m_specularIOR; }
    void SetSpecularIOR(float value) { m_specularIOR = value; }

    float GetSpecularAnisotropy() const { return m_specularAnisotropy; }
    void SetSpecularAnisotropy(float value) { m_specularAnisotropy = value; }

    // Emission getters/setters
    float GetEmissionLuminance() const { return m_emissionLuminance; }
    void SetEmissionLuminance(float value) { m_emissionLuminance = value; }

    void SetEmissionColorAndLuminance(const Float3& color, float luminance)
    {
        m_emissionColor = color;
        m_emissionLuminance = luminance;
    }

    // MaterialDataのオーバーライド
    MaterialParam GetMaterialData() const override
    {
        MaterialParam data{};

        // OpenPBR Base
        data.baseWeight = m_baseWeight;
        data.baseColor = Float3(m_baseColor.x, m_baseColor.y, m_baseColor.z);
        data.baseMetalness = m_baseMetalness;
        data.baseDiffuseRoughness = m_baseDiffuseRoughness;

        // OpenPBR Specular
        data.specularWeight = m_specularWeight;
        data.specularColor = Float3(m_specularColor.x, m_specularColor.y, m_specularColor.z);
        data.specularRoughness = m_specularRoughness;
        data.specularIOR = m_specularIOR;
        data.specularAnisotropy = m_specularAnisotropy;

        // OpenPBR Emission
        data.emissionLuminance = m_emissionLuminance;
        data.emissionColor = m_emissionColor;

        return data;
    }

private:
    // Base
    float m_baseWeight;
    float m_baseMetalness;
    float m_baseDiffuseRoughness;

    // Specular
    float m_specularWeight;
    float m_specularRoughness;
    float m_specularIOR;
    float m_specularAnisotropy;

    // Emission
    float m_emissionLuminance;
};
