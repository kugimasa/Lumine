#include "Window.hpp"
#include "Renderer.hpp"
#include <imgui.h>
#include <imgui_impl_win32.h>


HWND Window::m_hWnd = nullptr;

int Window::Run(Renderer* renderer, HINSTANCE hInstance)
{
    if (!renderer) return EXIT_FAILURE;

    try
    {
        // ウィンドウ情報のセット
        WNDCLASSEXW windowClass{};
        windowClass.cbSize = sizeof(WNDCLASSEXW);
        windowClass.style = CS_HREDRAW | CS_VREDRAW;
        windowClass.hInstance = hInstance;
        windowClass.hCursor = LoadCursor(NULL, IDC_ARROW);
        windowClass.lpfnWndProc = WindowProc;
        windowClass.lpszClassName = renderer->GetName();
        RegisterClassExW(&windowClass);

        // ウィンドウサイズの設定
        RECT lpRect = {0, 0, LONG(renderer->GetWidth()), LONG(renderer->GetHeight())};
        DWORD dwStyle = WS_OVERLAPPEDWINDOW;
        dwStyle &= ~(WS_MAXIMIZEBOX | WS_MINIMIZEBOX | WS_SIZEBOX);
        AdjustWindowRect(&lpRect, dwStyle, FALSE);

        // ウィンドウ生成
        m_hWnd = CreateWindowW(
            windowClass.lpszClassName,
            renderer->GetName(),
            dwStyle,
            CW_USEDEFAULT, CW_USEDEFAULT,
            lpRect.right - lpRect.left, lpRect.bottom - lpRect.top,
            nullptr,
            nullptr,
            hInstance,
            renderer
        );

        // デフォルトではウィンドウ非表示
        int nCmdShow = SW_HIDE;
        // ウィンドウを表示する場合
        if (renderer->HasWindow())
        {
            nCmdShow = SW_SHOWNORMAL;
            // ImGui関連の初期化
            IMGUI_CHECKVERSION();
            ImGui::CreateContext();
            ImGui_ImplWin32_Init(m_hWnd);
        }
        // レンダラーの初期化
        renderer->OnInit();

        ShowWindow(m_hWnd, nCmdShow);

        // メインループ
        MSG msg{};
        while (msg.message != WM_QUIT)
        {
            if (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE))
            {
                TranslateMessage(&msg);
                DispatchMessageW(&msg);
            }
            else
            {
                // ウィンドウがある場合はこちらで描画
                renderer->OnUpdate();
                renderer->OnRender();
            }
        }

        // レンダラーの終了処理
        if (renderer->HasWindow())
        {
            // ImGui関連の破棄
            renderer->DestroyImGui();
        }
        renderer->OnDestroy();
        return EXIT_SUCCESS;
    }
    catch (std::exception& e)
    {
        if (renderer->HasWindow())
        {
            // ImGui関連の破棄
            renderer->DestroyImGui();
        }
        renderer->OnDestroy();
        std::wstring err = L"エラー終了: " + StrToWStr(std::string(e.what()));
        Error(PrintInfoType::RENDERER, err);
        return EXIT_FAILURE;
    }
}

LRESULT CALLBACK Window::WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    auto* renderer = (Renderer*)GetWindowLongPtr(hWnd, GWLP_USERDATA);

    if (renderer && renderer->HasWindow())
    {
        if (ImGui_ImplWin32_WndProcHandler(hWnd, message, wParam, lParam))
        {
            return TRUE;
        }
    }

    switch (message)
    {
    case WM_CREATE:
        {
            auto pCreateStruct = reinterpret_cast<LPCREATESTRUCT>(lParam);
            SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pCreateStruct->lpCreateParams));
        }
        return 0;

    case WM_KEYDOWN:
        {
            if (renderer)
            {
                renderer->OnKeyDown(wParam);
            }
        }
        return 0;

    case WM_KEYUP:
        {
            if (renderer)
            {
                renderer->OnKeyUp(wParam);
            }
        }
        return 0;

    case WM_MOUSEMOVE:
        {
            if (renderer)
            {
                int x = LOWORD(lParam);
                int y = HIWORD(lParam);
                renderer->OnMouseMove(x, y);
            }
        }
        return 0;

    case WM_LBUTTONDOWN:
        {
            if (renderer)
            {
                renderer->OnMouseLeftDown();
            }
        }
        return 0;

    case WM_LBUTTONUP:
        {
            if (renderer)
            {
                renderer->OnMouseLeftUp();
            }
        }
        return 0;

    case WM_PAINT:
        {
            if (renderer)
            {
                renderer->OnUpdate();
                renderer->OnRender();
            }
        }
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    default:
        break;
    }

    return DefWindowProcW(hWnd, message, wParam, lParam);
}
