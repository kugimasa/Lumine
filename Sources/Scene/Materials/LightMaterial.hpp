#pragma once

#include "Material.hpp"

// 発光マテリアル
class LightMaterial : public Material
{
public:
    LightMaterial(const std::wstring& name, const Float3& emissiveColor, float emissionLuminance = 1.0f)
    {
        m_name = name;
        m_type = MaterialType::Emissive;
        m_baseColor = Float3(0, 0, 0);
        m_specularColor = Float3(0, 0, 0);
        // 発光マテリアル用マスク (NEEで利用)
        m_rtInstanceMask = 0x08;
        m_emissionColor = emissiveColor;
        m_emissionLuminance = emissionLuminance;
    }

    // MaterialDataのオーバーライド
    MaterialParam GetMaterialData() const override
    {
        MaterialParam data{};

        // OpenPBR Base
        data.baseWeight = 0.0f;
        data.baseColor = Float3(m_baseColor.x, m_baseColor.y, m_baseColor.z);
        data.baseMetalness = 0.0f;
        data.baseDiffuseRoughness = 0.0f;

        // OpenPBR Specular
        data.specularWeight = 0.0f;
        data.specularColor = Float3(m_specularColor.x, m_specularColor.y, m_specularColor.z);
        data.specularRoughness = 0.0f;
        data.specularIOR = 0.0f;
        data.specularAnisotropy = 0.0f;

        // OpenPBR Emission
        data.emissionLuminance = m_emissionLuminance;
        data.emissionColor = m_emissionColor;

        return data;
    }

    Float3 GetEmission()
    {
        return m_emissionColor;
    }

    float GetEmissionLuminance()
    {
        return m_emissionLuminance;
    }

    void SetEmissionColor(const Float3& emissive)
    {
        m_emissionColor = emissive;
    }

    void SetEmissionLuminance(float value)
    {
        m_emissionLuminance = value;
    }

private:
    float m_emissionLuminance;
};
