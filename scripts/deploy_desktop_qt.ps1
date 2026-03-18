param(
    [string]$ClientExe
)

$ErrorActionPreference = "Stop"

$repoRoot = Split-Path $PSScriptRoot -Parent

if (-not $ClientExe) {
    $clientCandidates = @(
        (Join-Path $repoRoot "qt-client\build\desktop-mingw\relay_qt_client.exe"),
        (Join-Path $repoRoot "qt-client\build\desktop-verify\relay_qt_client.exe"),
        (Join-Path $repoRoot "qt-client\build\desktop\relay_qt_client.exe"),
        (Join-Path $repoRoot "qt-client\build\desktop-check\relay_qt_client.exe"),
        (Join-Path $repoRoot "qt-client\build\desktop-check-1120\relay_qt_client.exe")
    )

    $ClientExe = $clientCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1
}

if (-not $ClientExe) {
    throw "Qt desktop client executable not found under qt-client\build"
}

if (-not (Test-Path $ClientExe)) {
    throw "Qt desktop client executable not found: $ClientExe"
}

$qtDesktopRoot = "D:\Qt\6.7.3\mingw_64"
$qtBinDir = Join-Path $qtDesktopRoot "bin"
$qmlDir = Join-Path $repoRoot "qt-client\qml"
$exeDir = Split-Path $ClientExe -Parent

$windeployqtCandidates = @(
    (Join-Path $qtBinDir "windeployqt.exe"),
    (Join-Path $qtBinDir "windeployqt6.exe")
)

$windeployqt = $windeployqtCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1

if (-not $windeployqt) {
    throw "windeployqt was not found under $qtBinDir"
}

if (-not (Test-Path $qmlDir)) {
    throw "QML source directory not found: $qmlDir"
}

Write-Host "Deploying Qt desktop runtime..."
Write-Host ("Executable: {0}" -f $ClientExe)
Write-Host ("Using: {0}" -f $windeployqt)

& $windeployqt `
    --qmldir $qmlDir `
    --compiler-runtime `
    --verbose 0 `
    $ClientExe

if ($LASTEXITCODE -ne 0) {
    throw "windeployqt failed with exit code $LASTEXITCODE"
}

Write-Host ""
Write-Host "Deployment completed."
Write-Host ("Output directory: {0}" -f $exeDir)
