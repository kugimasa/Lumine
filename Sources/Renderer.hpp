#pragma once

#include "Device.hpp"
#include "Scene/Scene.hpp"

class Renderer
{
public:
    // maxFrameを指定しない限りは描画し続ける
    Renderer(UINT width, UINT height, std::wstring name, int maxFrame = -1, bool hasWindow = false);

    void OnInit();
    void OnUpdate();
    void OnRender();
    void OnDestroy();

    void OnKeyDown(WPARAM key);
    void OnKeyUp(WPARAM key);
    void OnMouseMove(int x, int y);
    void OnMouseLeftDown();
    void OnMouseLeftUp();

    bool IsRunning() const { return m_isRunning; }

    bool HasWindow() const { return m_hasWindow; }

    UINT GetWidth() const { return m_width; }

    UINT GetHeight() const { return m_height; }

    float GetAspect() const { return float(m_width) / float(m_height); }

    const wchar_t* GetName() const { return m_name.c_str(); }

private:
    // デバイスの初期化関数
    bool InitGraphicDevice(HWND hwnd);

    // グローバルルートシグネチャ生成
    void CreateGlobalRootSignature();

    // ローカルルートシグネチャ生成
    void CreateLocalRootSignature();

    // レイトレーシング用のステートオブジェクトの構築
    void CreateStateObject();

    // レイトレーシング結果の書き込み用バッファの生成
    void CreateOutputBuffer();

    // シェーダーテーブルの構築
    void CreateShaderTable();

    // 画像の出力
    void OutputImage(ComPtr<ID3D12Resource> imageBuffer);

    // キー入力処理
    void ProcessInput(float deltaTime);

private:
    bool m_hasWindow;
    bool m_isRunning;
    UINT m_width;
    UINT m_height;
    int m_currentFrame;
    int m_maxFrame;
    std::wstring m_name;

    std::unique_ptr<Device> m_pDevice;
    std::shared_ptr<Scene> m_pScene;

    ComPtr<ID3D12Resource> m_pOutputBuffer;
    ComPtr<ID3D12Resource> m_pShaderTable;
    ComPtr<ID3D12RootSignature> m_pGlobalRootSignature;
    ComPtr<ID3D12RootSignature> m_pRayGenLocalRootSignature;
    ComPtr<ID3D12RootSignature> m_pClosestHitLocalRootSignature;
    ComPtr<ID3D12StateObject> m_pRTStateObject;
    ComPtr<ID3D12GraphicsCommandList4> m_pCmdList;
    DescriptorHeap m_outputBufferDescHeap;
    D3D12_DISPATCH_RAYS_DESC m_dispatchRayDesc;

    struct HitInfo
    {
        Float3 hitPos;
        Float3 reflectDir;
        Float3 color;
        Float3 attenuation;
        UINT rayDepth;
        UINT seed;
    };

    std::chrono::system_clock::time_point m_startTime;
    std::chrono::system_clock::time_point m_endTime;
    std::chrono::system_clock::time_point m_lastFrameTime;

    // キー入力状態
    struct KeyState
    {
        bool W = false; // 前進
        bool A = false; // 左移動
        bool S = false; // 後退
        bool D = false; // 右移動
        bool Q = false; // 下降
        bool E = false; // 上昇
    };

    KeyState m_keyState;

    // マウス入力状態
    bool m_isMouseDragging;

    struct
    {
        int x;
        int y;
    } m_lastMousePos;

    float m_mouseSensitivity;

    // ImGui関連
public:
    void InitImGui();
    void UpdateImGui();
    void RenderImGui();
    void DestroyImGui();

private:
    struct ImGuiParam
    {
        float cameraMoveSpeed;
        float cameraRotateSpeed;
        float cameraFovY;
        Float3 cameraPos;
        Float3 cameraTarget;
        int maxPathDepth;
        int maxSPP;
        Float3 bunnyPos;
        float bunnyRotY;
        Float3 dynamicLightPos;
        float dynamicLightSize;
        Float3 dynamicLightColor;
        float dynamicLightIntensity;
        Float3 ambientColor;
        float iblPower;
        float whitePoint;
        Float3 fluorBaseColor;
    };

    ImGuiParam m_imGuiParam;
    DescriptorHeap m_imguiDescHeap;
};
