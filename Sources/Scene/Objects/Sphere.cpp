#include "Sphere.hpp"

Sphere::Sphere(float radius, int subdivisions, Float3 origin)
    : m_radius(radius), m_subdivisions(subdivisions)
{
    m_origin = origin;
    m_worldMtx = XMMatrixTranslation(origin.x, origin.y, origin.z);
}

// ジオメトリの生成
void Sphere::GenerateGeometry()
{
    m_vertices.clear();
    m_indices.clear();

    // UV球体の生成（経度・緯度ベース）
    const int latitudeBands = m_subdivisions;
    const int longitudeBands = m_subdivisions;

    // 頂点生成
    for (int lat = 0; lat <= latitudeBands; ++lat)
    {
        float theta = lat * XM_PI / latitudeBands;
        float sinTheta = sinf(theta);
        float cosTheta = cosf(theta);

        for (int lon = 0; lon <= longitudeBands; ++lon)
        {
            float phi = lon * 2.0f * XM_PI / longitudeBands;
            float sinPhi = sinf(phi);
            float cosPhi = cosf(phi);

            Vertex vertex;
            vertex.normal = Float3(cosPhi * sinTheta, cosTheta, sinPhi * sinTheta);
            vertex.position = Float3(
                m_radius * vertex.normal.x,
                m_radius * vertex.normal.y,
                m_radius * vertex.normal.z
            );
            vertex.uv = Float2(
                (float)lon / (float)longitudeBands,
                (float)lat / (float)latitudeBands
            );

            m_vertices.push_back(vertex);
        }
    }

    // インデックス生成
    for (int lat = 0; lat < latitudeBands; ++lat)
    {
        for (int lon = 0; lon < longitudeBands; ++lon)
        {
            int current = lat * (longitudeBands + 1) + lon;
            int next = current + longitudeBands + 1;

            // 四角形を2つの三角形に分割
            // 三角形1
            m_indices.push_back(current);
            m_indices.push_back(next);
            m_indices.push_back(current + 1);

            // 三角形2
            m_indices.push_back(current + 1);
            m_indices.push_back(next);
            m_indices.push_back(next + 1);
        }
    }
}
