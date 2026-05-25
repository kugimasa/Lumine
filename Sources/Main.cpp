#include "Renderer.hpp"
#include "Window.hpp"

int main(int argc, char* argv[])
{
    // コンソールのコードページをUTF-8に設定
    SetConsoleCP(CP_UTF8);
    SetConsoleOutputCP(CP_UTF8);

    int maxFrame = -1;
    // コマンドライン入力形式
    // ./[renderer].exe --frame {max_frame}
    if (argc == 3)
    {
        if (strcmp(argv[1], "--frame") == 0)
        {
            maxFrame = atoi(argv[2]);
        }
    }

    // オンスクリーン描画するかの判定
#if defined(_DEBUG) || defined(DEBUG) || !defined(NDEBUG)
    bool hasWindow = true;
#else
    bool hasWindow = false;
#endif

    Renderer renderer(1024, 1024, L"Lumine", maxFrame, hasWindow);
    return Window::Run(&renderer, nullptr);
}
