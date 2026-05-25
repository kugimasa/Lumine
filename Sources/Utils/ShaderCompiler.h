#pragma once

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <d3d12.h>
#include <dxcapi.h>

using Microsoft::WRL::ComPtr;
namespace fs = std::filesystem;

std::vector<char> inline CompileShaderLibrary(const fs::path& shaderPath)
{
    // DXC のインスタンス作成
    ComPtr<IDxcUtils> dxcUtils;
    ComPtr<IDxcCompiler3> dxcCompiler;
    ComPtr<IDxcIncludeHandler> includeHandler;

    HRESULT hr = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&dxcUtils));
    if (FAILED(hr))
    {
        Error(PrintInfoType::SHADER, L"DxcUtilsの作成に失敗しました");
    }
    hr = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&dxcCompiler));
    if (FAILED(hr))
    {
        Error(PrintInfoType::SHADER, L"DxcCompilerの作成に失敗しました");
    }
    hr = dxcUtils->CreateDefaultIncludeHandler(&includeHandler);
    if (FAILED(hr))
    {
        Error(PrintInfoType::SHADER, L"インクルードハンドラーの作成に失敗しました");
    }

    // シェーダーファイルの読み込み
    std::ifstream shaderFile(shaderPath, std::ios::binary);
    if (!shaderFile.is_open())
    {
        Error(PrintInfoType::SHADER, "シェーダーの読み込みに失敗しました :", shaderPath.string());
    }

    std::stringstream buffer;
    buffer << shaderFile.rdbuf();
    std::string shaderSource = buffer.str();
    shaderFile.close();

    // DXC用のソースバッファ作成
    ComPtr<IDxcBlobEncoding> sourceBlob;
    hr = dxcUtils->CreateBlob(shaderSource.c_str(),
                              static_cast<UINT32>(shaderSource.size()),
                              CP_UTF8,
                              &sourceBlob);
    if (FAILED(hr))
    {
        Error(PrintInfoType::SHADER, L"ソースBlobの作成に失敗しました");
    }

    // コンパイル引数の設定
    std::vector<LPCWSTR> arguments = {
        L"-T", L"lib_6_6", // ライブラリターゲット (Shader Model 6.6)
        L"-E", L"", // ライブラリの場合、エントリーポイントは空
        L"-Zi", // デバッグ情報を含める
        L"-Od", // 最適化を無効化（デバッグ用）
        L"-Qembed_debug", // デバッグ情報を埋め込み
        L"-all_resources_bound", // すべてのリソースがバインドされていると仮定
        L"-I" SHADER_SOURCE_DIR // インクルード
    };

    // ファイル名をワイド文字列に変換
    std::wstring wShaderPath = shaderPath.wstring();

    // DXC引数構造体の作成
    DxcBuffer sourceBuffer = {};
    sourceBuffer.Ptr = sourceBlob->GetBufferPointer();
    sourceBuffer.Size = sourceBlob->GetBufferSize();
    sourceBuffer.Encoding = DXC_CP_ACP;

    // コンパイル実行
    ComPtr<IDxcResult> compileResult;
    hr = dxcCompiler->Compile(&sourceBuffer,
                              arguments.data(),
                              static_cast<UINT32>(arguments.size()),
                              includeHandler.Get(),
                              IID_PPV_ARGS(&compileResult));
    if (FAILED(hr))
    {
        Error(PrintInfoType::SHADER, L"シェーダーコンパイルに失敗しました");
    }

    // コンパイル結果の確認
    HRESULT compileStatus;
    compileResult->GetStatus(&compileStatus);

    // エラーメッセージの取得と表示
    ComPtr<IDxcBlobUtf8> errors;
    compileResult->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&errors), nullptr);
    if (errors && errors->GetStringLength() > 0)
    {
        std::string errorMsg(errors->GetStringPointer(), errors->GetStringLength());
        Error(PrintInfoType::SHADER, L"コンパイルエラー: " +
              std::wstring(errorMsg.begin(), errorMsg.end()));
    }

    if (FAILED(compileStatus))
    {
        Error(PrintInfoType::SHADER, L"シェーダーコンパイルに失敗しました");
    }

    // コンパイル済みバイトコードの取得
    ComPtr<IDxcBlob> shaderBlob;
    hr = compileResult->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shaderBlob), nullptr);
    if (FAILED(hr) || !shaderBlob)
    {
        Error(PrintInfoType::SHADER, L"コンパイル済みバイトコードの取得に失敗しました");
    }

    // バイトコードをvector<char>に変換
    std::vector<char> shaderBytecode(
        reinterpret_cast<const char*>(shaderBlob->GetBufferPointer()),
        reinterpret_cast<const char*>(shaderBlob->GetBufferPointer()) + shaderBlob->GetBufferSize()
    );

    Print(PrintInfoType::SHADER, L"シェーダーコンパイル完了 Size: " +
          std::to_wstring(shaderBytecode.size()) + L" bytes");

    return shaderBytecode;
}

std::vector<char> inline LoadPreCompiledShaderLibrary(const fs::path& shaderLibPath)
{
    // シェーダーロード
    std::ifstream file(shaderLibPath, std::ios::binary);
    if (!file.is_open())
    {
        std::wstring err = L"シェーダーライブラリの読み込みに失敗しました :" + StrToWStr(shaderLibPath.string());
        Error(PrintInfoType::SHADER, err);
        throw std::runtime_error("");
    }
    std::vector<char> shaderSource;
    shaderSource.resize(file.seekg(0, std::ios::end).tellg());
    file.seekg(0, std::ios::beg).read(shaderSource.data(), shaderSource.size());
    return shaderSource;
}

/// <summary>
/// シェーダーセットアップ
/// </summary>
/// <param name="shaderName">シェーダー名</param>
/// <returns></returns>
std::vector<char> inline SetupShader(const std::wstring& shaderName)
{
    std::vector<char> shaderBin;
#if defined(_DEBUG) || defined(DEBUG) || !defined(NDEBUG)
    // デバッグビルドではシェーダーのランタイムコンパイル
    Print(PrintInfoType::SHADER, L"Shader: " + shaderName);
    const fs::path shaderPath{SHADER_SOURCE_DIR L"/" + shaderName + L".hlsl"};
    shaderBin = CompileShaderLibrary(shaderPath);
#else
    // リリースビルドではビルド時に事前コンパイルされたシェーダーバイナリのロード
    const fs::path shaderPath{SHADER_BIN_DIR L"/" + shaderName + L".dxlib"};
    shaderBin = LoadPreCompiledShaderLibrary(shaderPath);
#endif
    return shaderBin;
}
