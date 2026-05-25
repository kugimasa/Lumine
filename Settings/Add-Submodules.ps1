Write-Host "Adding external submodules ..." -ForegroundColor Cyan

git submodule add https://github.com/ocornut/imgui.git Externals/imgui
git submodule add https://github.com/microsoft/DirectX-Headers.git Externals/DirectX-Headers
git submodule add https://github.com/assimp/assimp.git Externals/assimp
git submodule add https://github.com/microsoft/DirectXTex.git Externals/DirectXTex
git submodule add https://github.com/richgel999/fpng.git Externals/fpng

Write-Host "Updating submodlues ..." -ForegroundColor Cyan
git submodule update --init --recursive

if ($LASTEXITCODE -eq 0) {
    Write-Host "Success!" -ForegroundColor Green
} else {
    Write-Host "Error occured during setup." -ForegroundColor Red
}