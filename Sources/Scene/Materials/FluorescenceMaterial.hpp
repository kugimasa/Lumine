#pragma once

#include "Material.hpp"

class FluorescenceMaterial : public Material
{
public:
    FluorescenceMaterial(const std::wstring& name, Float3 baseColor = Float3(1, 1, 1))
        : Material(name)
    {
        m_type = MaterialType::Fluorescence;
        m_baseColor = baseColor;
        m_fluorWeight = 1.0f;
    }

    MaterialParam GetMaterialData() const override
    {
        MaterialParam data{};
        data.baseColor = m_baseColor;
        data.fluorWeight = m_fluorWeight;
        return data;
    }

    void SetFluorWeight(float value) { m_fluorWeight = value; }

private:
    float m_fluorWeight;
};
