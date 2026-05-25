#pragma once

#include "Object.hpp"

class Sphere : public Object
{
public:
    Sphere(float radius = 1.0f, int subdivisions = 32, Float3 origin = Float3(0.0f, 0.0f, 0.0f));
    ~Sphere() = default;

    float GetRadius() const { return m_radius; }

protected:
    void GenerateGeometry() override;
    std::wstring GetObjectTypeName() const override { return L"Sphere"; }

private:
    float m_radius;
    int m_subdivisions;
};
