#include "Camera.hpp"

Camera::Camera(float fovY, float aspect, float nearZ, float farZ, Float3 origin, Float3 target, Float3 up)
    : m_fovY(fovY), m_aspect(aspect), m_nearZ(nearZ), m_farZ(farZ),
      m_moveSpeed(10.0f), m_rotateSpeed(5.0f),
      m_origin(origin), m_target(target), m_up(up)
{
    m_projMtx = XMMatrixPerspectiveFovLH(fovY * M_DEG2RAD, aspect, nearZ, farZ);
    m_viewMtx = XMMatrixLookAtLH(XMLoadFloat3(&origin), XMLoadFloat3(&target), XMLoadFloat3(&up));
}

void Camera::SetOrigin(Float3 origin)
{
    m_origin = origin;
    m_viewMtx = XMMatrixLookAtLH(XMLoadFloat3(&origin), XMLoadFloat3(&m_target), XMLoadFloat3(&m_up));
}

void Camera::SetTarget(Float3 target)
{
    m_target = target;
    m_viewMtx = XMMatrixLookAtLH(XMLoadFloat3(&m_origin), XMLoadFloat3(&target), XMLoadFloat3(&m_up));
}

void Camera::SetFovY(float fovY)
{
    m_fovY = fovY;
    m_projMtx = XMMatrixPerspectiveFovLH(fovY * M_DEG2RAD, m_aspect, m_nearZ, m_farZ);
}


Float3 Camera::GetForward() const
{
    Float3 forward;
    XMVECTOR forwardVec = XMVector3Normalize(XMVectorSubtract(XMLoadFloat3(&m_target), XMLoadFloat3(&m_origin)));
    XMStoreFloat3(&forward, forwardVec);
    return forward;
}

Float3 Camera::GetRight() const
{
    Float3 right;
    XMVECTOR forwardVec = XMVector3Normalize(XMVectorSubtract(XMLoadFloat3(&m_target), XMLoadFloat3(&m_origin)));
    XMVECTOR upVec = XMLoadFloat3(&m_up);
    XMVECTOR rightVec = XMVector3Normalize(XMVector3Cross(forwardVec, upVec));
    XMStoreFloat3(&right, rightVec);
    return right;
}

void Camera::Move(Float3 direction, float distance)
{
    XMVECTOR dirVec = XMLoadFloat3(&direction);
    XMVECTOR movementVec = XMVectorScale(dirVec, distance);
    XMVECTOR originVec = XMLoadFloat3(&m_origin);
    XMVECTOR targetVec = XMLoadFloat3(&m_target);

    originVec = XMVectorAdd(originVec, movementVec);
    targetVec = XMVectorAdd(targetVec, movementVec);

    XMStoreFloat3(&m_origin, originVec);
    XMStoreFloat3(&m_target, targetVec);

    m_viewMtx = XMMatrixLookAtLH(originVec, targetVec, XMLoadFloat3(&m_up));
}

void Camera::Rotate(float yaw, float pitch)
{
    // カメラからターゲットへの方向ベクトルを計算
    XMVECTOR originVec = XMLoadFloat3(&m_origin);
    XMVECTOR targetVec = XMLoadFloat3(&m_target);
    XMVECTOR upVec = XMLoadFloat3(&m_up);
    XMVECTOR forwardVec = XMVectorSubtract(targetVec, originVec);
    float distance = XMVectorGetX(XMVector3Length(forwardVec));
    forwardVec = XMVector3Normalize(forwardVec);

    // 右方向ベクトルを計算
    XMVECTOR rightVec = XMVector3Normalize(XMVector3Cross(forwardVec, upVec));

    // Yaw回転（上方向軸周り）
    XMMATRIX yawRotation = XMMatrixRotationAxis(upVec, yaw);
    forwardVec = XMVector3TransformNormal(forwardVec, yawRotation);
    rightVec = XMVector3TransformNormal(rightVec, yawRotation);

    // Pitch回転（右方向軸周り）
    XMMATRIX pitchRotation = XMMatrixRotationAxis(rightVec, pitch);
    forwardVec = XMVector3TransformNormal(forwardVec, pitchRotation);

    // 新しいターゲット位置を計算
    targetVec = XMVectorAdd(originVec, XMVectorScale(forwardVec, distance));
    XMStoreFloat3(&m_target, targetVec);

    m_viewMtx = XMMatrixLookAtLH(originVec, targetVec, upVec);
}
