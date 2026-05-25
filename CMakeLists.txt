cmake_minimum_required(VERSION 3.31)
project(Lumine)

set(CMAKE_CXX_STANDARD 23)

find_library(D3D12_LIB d3d12)
find_library(DXGI_LIB dxgi)

# Sources
set(SOURCE_DIR "${CMAKE_SOURCE_DIR}/Sources")
file(GLOB_RECURSE SOURCES "${SOURCE_DIR}/*.cpp")

# ImGui
set(IMGUI_SOURCE_DIR ${CMAKE_SOURCE_DIR}/externals/imgui)
set(IMGUI_BACKENDS_DIR ${IMGUI_SOURCE_DIR}/backends)
set(IMGUI_SOURCES
        ${IMGUI_SOURCE_DIR}/imgui.cpp
        ${IMGUI_SOURCE_DIR}/imgui_draw.cpp
        ${IMGUI_SOURCE_DIR}/imgui_tables.cpp
        ${IMGUI_SOURCE_DIR}/imgui_widgets.cpp
        ${IMGUI_BACKENDS_DIR}/imgui_impl_dx12.cpp
        ${IMGUI_BACKENDS_DIR}/imgui_impl_win32.cpp
)

# fpng
set(FPNG_SOURCE_DIR ${CMAKE_SOURCE_DIR}/externals/fpng/src)
set(FPNG_SOURCES
        ${FPNG_SOURCE_DIR}/fpng.h
        ${FPNG_SOURCE_DIR}/fpng.cpp
)

# DirectX-Headers
set(DIRECTX_HEADERS_DIR ${CMAKE_SOURCE_DIR}/externals/DirectX-Headers)
set(DIRECTX_HEADERS_INCLUDE_DIR ${DIRECTX_HEADERS_DIR}/include/directx)
add_subdirectory(${DIRECTX_HEADERS_DIR})

# DirectXTex
set(DIRECTX_TEX_DIR ${CMAKE_SOURCE_DIR}/externals/DirectXTex)
add_subdirectory(${DIRECTX_TEX_DIR})

# DXC
set(DXC_DIR ${CMAKE_SOURCE_DIR}/externals/dxc)
set(DXC_INCLUDE_DIR ${DXC_DIR}/inc)
set(DXC_LIB_DIR ${DXC_DIR}/lib/x64)
set(DXC_BIN_DIR ${DXC_DIR}/bin/x64)

# assimp
set(ASSIMP_DIR ${CMAKE_SOURCE_DIR}/externals/assimp)
add_subdirectory(${ASSIMP_DIR})

# 実行ファイル追加
add_executable(${PROJECT_NAME}
        ${SOURCES}
        ${IMGUI_SOURCES}
        ${FPNG_SOURCES}
)


# ビルド実行時にDLLをコピー (DXC)
add_custom_command(
        TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${DXC_BIN_DIR}/dxcompiler.dll"
        "${DXC_BIN_DIR}/dxil.dll"
        "${CMAKE_BINARY_DIR}"
        COMMENT "Copying DXC DLLs"
)

# アセットディレクトリをコピー
add_custom_command(
        TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/resources"
        "${CMAKE_BINARY_DIR}/resources"
        COMMENT "Copying resources"
)

# 実行用設定ファイルをコピー
file(GLOB SETTINGS_FILES "${CMAKE_SOURCE_DIR}/settings/*")
add_custom_command(
        TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${SETTINGS_FILES}
        ${CMAKE_BINARY_DIR}
        COMMENT "Copying settings"
)

# シェーダー関連
set(SHADER_SOURCE_DIR ${SOURCE_DIR}/Shader)
set(SHADER_BIN_DIR ${CMAKE_BINARY_DIR}/resources/Shader)
# リリースビルド時にはシェーダを事前コンパイル
if (CMAKE_BUILD_TYPE STREQUAL "Release")
    message("シェーダーコンパイル")
    set(DXC_COMPILER "${DXC_BIN_DIR}/dxc.exe")
    if (NOT EXISTS ${DXC_COMPILER})
        message(FATAL_ERROR "DXCコンパイラが見つかりません: ${DXC_COMPILER}")
    endif ()
    function(compile_shader hlsl_file output_file profile)
        add_custom_command(
                OUTPUT ${output_file}
                COMMAND ${DXC_COMPILER} -T ${profile} -E main -Fo ${output_file} ${hlsl_file}
                DEPENDS ${hlsl_file}
                COMMENT "HLSLコンパイル: ${hlsl_file}"
                VERBATIM
        )
    endfunction()

    # シェーダーリスト
    set(HLSL_FILES
            ${SHADER_SOURCE_DIR}/Raygen.hlsl
            ${SHADER_SOURCE_DIR}/Miss.hlsl
            ${SHADER_SOURCE_DIR}/ClosestHitDiffuse.hlsl
            ${SHADER_SOURCE_DIR}/ClosestHitEmissive.hlsl
            ${SHADER_SOURCE_DIR}/ClosestHitOpenpbr.hlsl
            ${SHADER_SOURCE_DIR}/ClosestHitFluorescence.hlsl
    )

    foreach (HLSL_FILE ${HLSL_FILES})
        get_filename_component(SHADER_NAME ${HLSL_FILE} NAME_WE)
        set(SHADER_OUTPUT "${SHADER_BIN_DIR}/${SHADER_NAME}.dxlib")
        compile_shader(${HLSL_FILE} ${SHADER_OUTPUT} "lib_6_4")
        add_custom_target(${SHADER_NAME}_shader ALL DEPENDS ${SHADER_OUTPUT})
        add_dependencies(${PROJECT_NAME} ${SHADER_NAME}_shader)
    endforeach ()
endif ()

# コンパイル時マクロ定義
target_compile_definitions(${PROJECT_NAME} PRIVATE
        SHADER_SOURCE_DIR="${SHADER_SOURCE_DIR}"
        SHADER_BIN_DIR="Resources/Shader"
        GEO_DIR="Resources/Geo"
        TEX_DIR="Resources/Tex"
        OUTPUT_DIR="./"
)

target_include_directories(${PROJECT_NAME} PRIVATE
        ${SOURCE_DIR}
        ${IMGUI_SOURCE_DIR}
        ${IMGUI_BACKENDS_DIR}
        ${FPNG_SOURCE_DIR}
        ${DIRECTX_HEADERS_INCLUDE_DIR}
        ${DIRECTX_TEX_DIR}
)

target_link_libraries(${PROJECT_NAME}
        d3d12
        dxgi
        dxguid
        DirectX-Headers
        DirectXTex
        dxcompiler
        assimp
)


# Windows SDK
if (WIN32)
    target_compile_definitions(${PROJECT_NAME} PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
endif ()

# Compiler Warning Settings
if (MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W4 /WX- /utf-8)
    target_compile_options(${PROJECT_NAME} PRIVATE
            /wd4996  # deprecated function warnings
            /wd4244  # conversion warnings
            /wd4267  # size_t conversion warnings
    )
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -pedantic)
endif ()
