param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Debug",
    [string]$ApiBase = "",
    [string]$BuildDirName = "",
    [string]$QtAndroidRoot = ""
)

$ErrorActionPreference = "Stop"

function Get-DefaultApiBase {
    param(
        [string]$Config,
        [string]$RequestedApiBase
    )

    if (-not [string]::IsNullOrWhiteSpace($RequestedApiBase)) {
        return $RequestedApiBase.Trim()
    }

    if (-not [string]::IsNullOrWhiteSpace($env:QXZ_CLIENT_DEFAULT_API_BASE)) {
        return $env:QXZ_CLIENT_DEFAULT_API_BASE.Trim()
    }

    if ($Config -eq "Release") {
        return "https://api.qixiangzhan.online/api/v1"
    }

    return "http://127.0.0.1:8000/api/v1"
}

function Import-LocalSigningProfile {
    $profilePath = Join-Path $env:USERPROFILE ".qixiangzhan\android\release\release-signing.env.ps1"
    if (Test-Path $profilePath) {
        . $profilePath
    }
}

function Ensure-ReleaseSigning {
    param(
        [string]$ScriptRoot
    )

    Import-LocalSigningProfile

    $defaultKeystorePath = Join-Path $env:USERPROFILE ".qixiangzhan\android\release\qixiangzhan-release.jks"
    if ([string]::IsNullOrWhiteSpace($env:QXZ_ANDROID_KEYSTORE_PATH)) {
        $env:QXZ_ANDROID_KEYSTORE_PATH = $defaultKeystorePath
    }
    if ([string]::IsNullOrWhiteSpace($env:QXZ_ANDROID_KEYSTORE_ALIAS)) {
        $env:QXZ_ANDROID_KEYSTORE_ALIAS = "qixiangzhan-release"
    }

    if (-not (Test-Path $env:QXZ_ANDROID_KEYSTORE_PATH)) {
        & (Join-Path $ScriptRoot "generate_android_keystore.ps1")
        Import-LocalSigningProfile
    }

    foreach ($name in @(
        "QXZ_ANDROID_KEYSTORE_PATH",
        "QXZ_ANDROID_KEYSTORE_ALIAS",
        "QXZ_ANDROID_KEYSTORE_STORE_PASS",
        "QXZ_ANDROID_KEYSTORE_KEY_PASS"
    )) {
        if ([string]::IsNullOrWhiteSpace([Environment]::GetEnvironmentVariable($name, "Process"))) {
            throw "Missing Android signing variable: $name"
        }
    }

    if (-not (Test-Path $env:QXZ_ANDROID_KEYSTORE_PATH)) {
        throw "Android keystore not found: $env:QXZ_ANDROID_KEYSTORE_PATH"
    }
}

$repoRoot = Split-Path $PSScriptRoot -Parent
$sourceDir = Join-Path $repoRoot "qt-client"
$buildDirName = if (-not [string]::IsNullOrWhiteSpace($BuildDirName)) {
    $BuildDirName.Trim()
} elseif ($Configuration -eq "Release") {
    "android-arm64-release"
} else {
    "android-arm64"
}
$buildDir = Join-Path $sourceDir ("build\" + $buildDirName)
$qtHost = "D:\Qt\6.7.3\mingw_64"
$qtAndroid = if (-not [string]::IsNullOrWhiteSpace($QtAndroidRoot)) {
    $QtAndroidRoot.Trim()
} elseif (-not [string]::IsNullOrWhiteSpace($env:QXZ_QT_ANDROID_ROOT)) {
    $env:QXZ_QT_ANDROID_ROOT.Trim()
} else {
    "D:\Qt\6.7.3\android_arm64_v8a"
}
$sdkRoot = "C:\Users\Lenovo\AppData\Local\Android\Sdk"
$ndkRoot = Join-Path $sdkRoot "ndk\26.1.10909125"
$qtCmake = Join-Path $qtHost "bin\qt-cmake.bat"
$androidDeployQt = Join-Path $qtHost "bin\androiddeployqt.exe"
$makeProgram = "D:\Qt\Tools\mingw1120_64\bin\mingw32-make.exe"
$deploymentJson = Join-Path $buildDir "android-relay_qt_client-deployment-settings.json"
$androidBuildDir = Join-Path $buildDir "android-build"
$abiDir = Join-Path $androidBuildDir "libs\arm64-v8a"
$soPath = Join-Path $buildDir "librelay_qt_client_arm64-v8a.so"
$depfilePath = Join-Path $androidBuildDir "relay_qt_client.d"
$jdkRoot = "C:\Program Files\Java\latest\jdk-25"
$apksigner = "C:\Users\Lenovo\AppData\Local\Android\Sdk\build-tools\36.1.0\apksigner.bat"
$defaultApiBase = Get-DefaultApiBase -Config $Configuration -RequestedApiBase $ApiBase

if (-not (Test-Path $qtCmake)) {
    throw "qt-cmake.bat not found: $qtCmake"
}

if (-not (Test-Path $androidDeployQt)) {
    throw "androiddeployqt.exe not found: $androidDeployQt"
}

$qtWebViewConfig = Join-Path $qtAndroid "lib\cmake\Qt6WebView\Qt6WebViewConfig.cmake"
if (-not (Test-Path $qtWebViewConfig)) {
    throw "Qt WebView for Android is required by the video page. Install the Qt 6.7.3 Android arm64-v8a WebView component, then rerun this script. Missing: $qtWebViewConfig"
}

if ($Configuration -eq "Release") {
    Ensure-ReleaseSigning -ScriptRoot $PSScriptRoot
}

$configureArgs = @(
    "-S", $sourceDir,
    "-B", $buildDir,
    "-G", "MinGW Makefiles",
    "-DCMAKE_BUILD_TYPE=$Configuration",
    "-DCMAKE_PREFIX_PATH=$qtAndroid",
    "-DQt6_DIR=$qtAndroid/lib/cmake/Qt6",
    "-DQT_HOST_PATH=$qtHost",
    "-DQT_HOST_PATH_CMAKE_DIR=$qtHost/lib/cmake",
    "-DCMAKE_TOOLCHAIN_FILE=$ndkRoot/build/cmake/android.toolchain.cmake",
    "-DCMAKE_MAKE_PROGRAM=$makeProgram",
    "-DCMAKE_FIND_ROOT_PATH_MODE_PACKAGE=BOTH",
    "-DANDROID_SDK_ROOT=$sdkRoot",
    "-DANDROID_NDK_ROOT=$ndkRoot",
    "-DANDROID_ABI=arm64-v8a",
    "-DANDROID_PLATFORM=android-34",
    "-DQXZ_CLIENT_DEFAULT_API_BASE=$defaultApiBase"
)

& $qtCmake @configureArgs
if ($LASTEXITCODE -ne 0) {
    throw "CMake configuration failed with exit code $LASTEXITCODE"
}

cmake --build $buildDir --target relay_qt_client --parallel 4
if ($LASTEXITCODE -ne 0) {
    throw "Android CMake build failed with exit code $LASTEXITCODE"
}

if (-not (Test-Path $soPath)) {
    throw "Android shared library was not produced: $soPath"
}

New-Item -ItemType Directory -Force -Path $abiDir | Out-Null
Copy-Item $soPath (Join-Path $abiDir (Split-Path $soPath -Leaf)) -Force

$deployArgs = @(
    "--input", $deploymentJson,
    "--output", $androidBuildDir,
    "--android-platform", "android-34",
    "--depfile", $depfilePath,
    "--builddir", $buildDir,
    "--verbose"
)

if ($Configuration -eq "Release") {
    $deployArgs += @(
        "--release",
        "--sign", $env:QXZ_ANDROID_KEYSTORE_PATH, $env:QXZ_ANDROID_KEYSTORE_ALIAS,
        "--storepass", $env:QXZ_ANDROID_KEYSTORE_STORE_PASS,
        "--keypass", $env:QXZ_ANDROID_KEYSTORE_KEY_PASS,
        "--jdk", $jdkRoot
    )
}

& $androidDeployQt @deployArgs
if ($LASTEXITCODE -ne 0) {
    throw "androiddeployqt failed with exit code $LASTEXITCODE"
}

$apkFolder = Join-Path $androidBuildDir ("build\outputs\apk\" + $Configuration.ToLower())
if (-not (Test-Path $apkFolder)) {
    throw "APK output folder not found: $apkFolder"
}

$apkFiles = Get-ChildItem $apkFolder -Filter *.apk | Sort-Object LastWriteTime -Descending
if ($apkFiles.Count -eq 0) {
    throw "No APK produced in $apkFolder"
}

$finalApk = $apkFiles[0].FullName

if ($Configuration -eq "Release") {
    if (-not (Test-Path $apksigner)) {
        throw "apksigner was not found: $apksigner"
    }

    & $apksigner verify --verbose $finalApk
    if ($LASTEXITCODE -ne 0) {
        throw "APK signature verification failed with exit code $LASTEXITCODE"
    }
}

Write-Host ""
Write-Host "$Configuration APK built:"
Write-Host "  $finalApk"
Write-Host "Default API Base:"
Write-Host "  $defaultApiBase"
