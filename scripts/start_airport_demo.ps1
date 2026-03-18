param(
    [switch]$SkipBackend,
    [switch]$SkipDesktop,
    [switch]$SkipAndroid,
    [switch]$NoInstallApk,
    [int]$EmulatorIndex = 0
)

$ErrorActionPreference = "Stop"

$repoRoot = Split-Path $PSScriptRoot -Parent
$backendDir = Join-Path $repoRoot "backend"
$backendPython = Join-Path $backendDir ".venv\Scripts\python.exe"
$backendHealthUrl = "http://127.0.0.1:8000/healthz"

$qtDesktopRoot = "D:\Qt\6.7.3\mingw_64"
$qtBinDir = Join-Path $qtDesktopRoot "bin"
$mingwBinDir = "D:\Qt\Tools\mingw1120_64\bin"
$deployScript = Join-Path $PSScriptRoot "deploy_desktop_qt.ps1"

$ldRoot = "D:\leidian\LDPlayer9"
$adbExe = Join-Path $ldRoot "adb.exe"
$ldConsole = Join-Path $ldRoot "ldconsole.exe"
$androidPackage = "com.qixiangzhan.platform"
$androidActivity = "org.qtproject.qt.android.bindings.QtActivity"
$androidComponent = "$androidPackage/$androidActivity"

$clientCandidates = @(
    (Join-Path $repoRoot "qt-client\build\desktop-mingw\relay_qt_client.exe"),
    (Join-Path $repoRoot "qt-client\build\desktop-verify\relay_qt_client.exe"),
    (Join-Path $repoRoot "qt-client\build\desktop-check-1120\relay_qt_client.exe"),
    (Join-Path $repoRoot "qt-client\build\desktop-check\relay_qt_client.exe")
)

function Write-Step {
    param([string]$Message)
    Write-Host ""
    Write-Host ("==== {0} ====" -f $Message)
}

function Test-HttpOk {
    param(
        [string]$Url,
        [int]$TimeoutSec = 2
    )

    try {
        $response = Invoke-WebRequest -Uri $Url -UseBasicParsing -TimeoutSec $TimeoutSec
        return $response.StatusCode -ge 200 -and $response.StatusCode -lt 300
    } catch {
        return $false
    }
}

function Wait-HttpOk {
    param(
        [string]$Url,
        [int]$TimeoutSec = 25
    )

    $deadline = (Get-Date).AddSeconds($TimeoutSec)
    while ((Get-Date) -lt $deadline) {
        if (Test-HttpOk -Url $Url -TimeoutSec 2) {
            return $true
        }
        Start-Sleep -Seconds 1
    }
    return $false
}

function Get-QtClientExe {
    $clientExe = $clientCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1
    if (-not $clientExe) {
        throw "Qt desktop client executable not found under qt-client\build"
    }
    return $clientExe
}

function Get-LatestSignedApk {
    $preferred = Join-Path $repoRoot "qt-client\build\android-arm64-release-airport-pad\android-build\build\outputs\apk\release\android-build-release-signed.apk"
    if (Test-Path $preferred) {
        return $preferred
    }

    $candidate = Get-ChildItem (Join-Path $repoRoot "qt-client\build") -Recurse -Filter "android-build-release-signed.apk" |
        Sort-Object LastWriteTime -Descending |
        Select-Object -First 1
    if (-not $candidate) {
        throw "No signed Android APK was found under qt-client\build"
    }
    return $candidate.FullName
}

function Get-ConnectedAndroidSerial {
    param([string]$AdbPath)

    $lines = & $AdbPath devices
    foreach ($line in $lines) {
        if ($line -match "^(?<serial>\S+)\s+device$") {
            return $matches["serial"]
        }
    }
    return $null
}

function Wait-AndroidDevice {
    param(
        [string]$AdbPath,
        [int]$TimeoutSec = 45
    )

    $deadline = (Get-Date).AddSeconds($TimeoutSec)
    while ((Get-Date) -lt $deadline) {
        $serial = Get-ConnectedAndroidSerial -AdbPath $AdbPath
        if ($serial) {
            return $serial
        }
        Start-Sleep -Seconds 2
    }
    return $null
}

if (-not $SkipBackend) {
    Write-Step "Backend"

    if (-not (Test-Path $backendPython)) {
        throw "Backend Python not found: $backendPython"
    }

    if (Test-HttpOk -Url $backendHealthUrl -TimeoutSec 2) {
        Write-Host "Backend is already running at $backendHealthUrl"
    } else {
        $backendProcess = Start-Process `
            -FilePath $backendPython `
            -ArgumentList @("-m", "uvicorn", "app.main:app", "--host", "127.0.0.1", "--port", "8000", "--reload") `
            -WorkingDirectory $backendDir `
            -PassThru

        if (-not (Wait-HttpOk -Url $backendHealthUrl -TimeoutSec 30)) {
            throw "Backend did not become ready: $backendHealthUrl"
        }

        Write-Host ("Backend started. PID={0}" -f $backendProcess.Id)
    }
}

if (-not $SkipDesktop) {
    Write-Step "Desktop Qt"

    $clientExe = Get-QtClientExe
    $clientDir = Split-Path $clientExe
    $qtCoreDll = Join-Path $clientDir "Qt6Core.dll"
    if (-not (Test-Path $qtCoreDll)) {
        if (-not (Test-Path $deployScript)) {
            throw "Qt runtime is missing and deploy script was not found: $deployScript"
        }
        & $deployScript -ClientExe $clientExe
    }

    $env:PATH = (@($qtBinDir, $mingwBinDir, $env:PATH) | Where-Object { $_ }) -join ";"
    $clientProcess = Start-Process -FilePath $clientExe -WorkingDirectory $clientDir -PassThru
    Write-Host ("Desktop client started. PID={0}" -f $clientProcess.Id)
    Write-Host ("Client path: {0}" -f $clientExe)
}

if (-not $SkipAndroid) {
    Write-Step "Android Emulator"

    if (-not (Test-Path $adbExe)) {
        throw "ADB not found: $adbExe"
    }
    if (-not (Test-Path $ldConsole)) {
        throw "LDPlayer console not found: $ldConsole"
    }

    & $adbExe start-server | Out-Null
    $serial = Get-ConnectedAndroidSerial -AdbPath $adbExe
    if (-not $serial) {
        Write-Host ("No emulator device detected. Launching LDPlayer index {0}..." -f $EmulatorIndex)
        & $ldConsole launch --index $EmulatorIndex | Out-Null
        $serial = Wait-AndroidDevice -AdbPath $adbExe -TimeoutSec 60
    }

    if (-not $serial) {
        throw "No Android emulator device became available"
    }

    Write-Host ("Using Android device: {0}" -f $serial)

    & $adbExe -s $serial reverse tcp:8000 tcp:8000
    Write-Host "ADB reverse configured: tcp:8000 -> tcp:8000"

    if (-not $NoInstallApk) {
        $apkPath = Get-LatestSignedApk
        Write-Host ("Installing APK: {0}" -f $apkPath)
        & $adbExe -s $serial install -r $apkPath
    }

    Write-Host ("Launching Android app: {0}" -f $androidComponent)
    & $adbExe -s $serial shell am start -W -n $androidComponent

    $pidLine = & $adbExe -s $serial shell pidof $androidPackage
    if ($pidLine) {
        Write-Host ("Android app running. pid={0}" -f $pidLine.Trim())
    } else {
        Write-Host "Android app launch command sent."
    }
}

Write-Host ""
Write-Host "Demo stack is ready."
Write-Host "Backend: http://127.0.0.1:8000/api/v1"
Write-Host "Login: admin / admin123"
Write-Host "MQTT host: rc11adc1.ala.cn-hangzhou.emqxsl.cn"
Write-Host "Device: relay_h743_001"
