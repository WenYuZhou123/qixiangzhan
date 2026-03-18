param(
    [switch]$SkipBackend,
    [switch]$SkipClient
)

$ErrorActionPreference = "Stop"

$repoRoot = Split-Path $PSScriptRoot -Parent
$backendDir = Join-Path $repoRoot "backend"
$backendPython = Join-Path $backendDir ".venv\Scripts\python.exe"
$qtDesktopRoot = "D:\Qt\6.7.3\mingw_64"
$qtBinDir = Join-Path $qtDesktopRoot "bin"
$mingwBinDir = "D:\Qt\Tools\mingw1120_64\bin"
$deployScript = Join-Path $PSScriptRoot "deploy_desktop_qt.ps1"

$clientCandidates = @(
    (Join-Path $repoRoot "qt-client\build\desktop-mingw\relay_qt_client.exe"),
    (Join-Path $repoRoot "qt-client\build\desktop-verify\relay_qt_client.exe"),
    (Join-Path $repoRoot "qt-client\build\desktop\relay_qt_client.exe"),
    (Join-Path $repoRoot "qt-client\build\desktop-check\relay_qt_client.exe"),
    (Join-Path $repoRoot "qt-client\build\desktop-check-1120\relay_qt_client.exe")
)

$clientExe = $clientCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1

Write-Host "Repo root: $repoRoot"

if (-not $SkipBackend) {
    if (-not (Test-Path $backendPython)) {
        throw "Backend Python not found: $backendPython"
    }

    Write-Host "Starting backend..."
    $backendProcess = Start-Process `
        -FilePath $backendPython `
        -ArgumentList @("-m", "uvicorn", "app.main:app", "--host", "127.0.0.1", "--port", "8000", "--reload") `
        -WorkingDirectory $backendDir `
        -PassThru

    Write-Host ("Backend started. PID={0}" -f $backendProcess.Id)
}

if (-not $SkipClient) {
    if (-not $clientExe) {
        throw "Qt desktop client executable not found under qt-client\\build"
    }

    $clientDir = Split-Path $clientExe
    $qtCoreDll = Join-Path $clientDir "Qt6Core.dll"
    if (-not (Test-Path $qtCoreDll)) {
        if (-not (Test-Path $deployScript)) {
            throw "Qt runtime is missing and deploy script was not found: $deployScript"
        }

        Write-Host "Qt desktop runtime is missing. Running deployment first..."
        & $deployScript -ClientExe $clientExe
    }

    $env:PATH = (@($qtBinDir, $mingwBinDir, $env:PATH) | Where-Object { $_ }) -join ";"

    Write-Host "Starting Qt desktop client..."
    $clientProcess = Start-Process `
        -FilePath $clientExe `
        -WorkingDirectory $clientDir `
        -PassThru

    Write-Host ("Client started. PID={0}" -f $clientProcess.Id)
    Write-Host ("Client path: {0}" -f $clientExe)
}

Write-Host ""
Write-Host "Default login: admin / admin123"
Write-Host "Default API: http://127.0.0.1:8000/api/v1"
Write-Host "Default MQTT host: rc11adc1.ala.cn-hangzhou.emqxsl.cn"
Write-Host "Default device: relay_h743_001"
