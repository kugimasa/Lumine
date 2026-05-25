#pragma once

#include "Utils/MathUtil.h"

class Camera
{
public:
    Camera(float fovY, float aspect, float nearZ, float farZ, Float3 origin, Float3 target,
           Float3 up = Float3(0.0f, 1.0f, 0.0f));
    ~Camera() = default;

    void SetOrigin(Float3 origin);
    void SetTarget(Float3 target);
    void SetFovY(float fovY);
    void SetMoveSpeed(float speed) { m_moveSpeed = speed; }
    void SetRotateSpeed(float speed) { m_rotateSpeed = speed; }

    Float3 GetOrigin() const { return m_origin; }
    Float3 GetTarget() const { return m_target; }
    Float3 GetForward() const;
    Float3 GetRight() const;
    Float3 GetUp() const { return m_up; }
    float GetFovY() const { return m_fovY; }
    float GetMoveSpeed() const { return m_moveSpeed; }
    float GetRotateSpeed() const { return m_rotateSpeed; }

    void Move(Float3 direction, float distance);
    void Rotate(float yaw, float pitch);

    Matrix GetViewMatrix() const { return m_viewMtx; }

    Matrix GetProjMatrix() const { return m_projMtx; }

private:
    float m_fovY;
    float m_aspect;
    float m_nearZ;
    float m_farZ;
    float m_moveSpeed;
    float m_rotateSpeed;
    Float3 m_origin;
    Float3 m_target;
    Float3 m_up;
    Matrix m_viewMtx;
    Matrix m_projMtx;
};
