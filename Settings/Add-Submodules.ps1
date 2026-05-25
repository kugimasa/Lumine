Write-Host "Adding external submodules ..." -ForegroundColor Cyan

git submodule add https://github.com/ocornut/imgui.git externals/imgui
git submodule add https://github.com/microsoft/DirectX-Headers.git externals/DirectX-Headers
git submodule add https://github.com/assimp/assimp.git externals/assimp
git submodule add https://github.com/microsoft/DirectXTex.git externals/DirectXTex
git submodule add https://github.com/richgel999/fpng.git externals/fpng

Write-Host "Updating submodlues ..." -ForegroundColor Cyan
git submodule update --init --recursive

if ($LASTEXITCODE -eq 0) {
    Write-Host "Success!" -ForegroundColor Green
} else {
    Write-Host "Error occured during setup." -ForegroundColor Red
}