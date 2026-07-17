# Сборка graph_core.exe (JSON CLI, без pybind)
# Usage: powershell -File cpp\build.ps1

$ErrorActionPreference = "Stop"
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $here

$out = Join-Path $here "graph_core.exe"

# 1) MSVC cl, если есть в PATH / через vswhere
$cl = Get-Command cl -ErrorAction SilentlyContinue
if ($cl) {
    Write-Host "Building with MSVC cl..."
    & cl /nologo /O2 /EHsc /std:c++17 /Fe:$out main.cpp algorithms.cpp
    if ($LASTEXITCODE -ne 0) { throw "cl failed" }
    Remove-Item -ErrorAction SilentlyContinue *.obj
    Write-Host "OK: $out"
    exit 0
}

# 2) g++ (MSYS2 / MinGW)
$gpp = Get-Command g++ -ErrorAction SilentlyContinue
if (-not $gpp) {
    $gppPath = "C:\msys64\ucrt64\bin\g++.exe"
    if (Test-Path $gppPath) { $gpp = $gppPath }
}
if ($gpp) {
    Write-Host "Building with g++..."
    & $gpp -O2 -std=c++17 -o $out main.cpp algorithms.cpp
    if ($LASTEXITCODE -ne 0) { throw "g++ failed" }
    Write-Host "OK: $out"
    exit 0
}

throw "Не найден ни cl, ни g++. Установите VS Build Tools или MSYS2 g++."
