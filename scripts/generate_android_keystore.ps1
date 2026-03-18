param(
    [string]$KeystorePath = "",
    [string]$Alias = "",
    [string]$StorePass = "",
    [string]$KeyPass = "",
    [string]$DName = "CN=Qixiangzhan Platform, OU=Platform, O=Qixiangzhan, L=Hangzhou, ST=Zhejiang, C=CN",
    [int]$ValidityDays = 3650
)

$ErrorActionPreference = "Stop"

$storePassInput = $StorePass
$keyPassInput = $KeyPass

function New-RandomPassword {
    $raw = [Convert]::ToBase64String([Guid]::NewGuid().ToByteArray()) +
           [Convert]::ToBase64String([Guid]::NewGuid().ToByteArray())
    return ($raw -replace "[^A-Za-z0-9]", "").Substring(0, 24)
}

$keytool = "C:\Program Files\Java\latest\jdk-25\bin\keytool.exe"
if (-not (Test-Path $keytool)) {
    throw "keytool.exe not found: $keytool"
}

$releaseDir = Join-Path $env:USERPROFILE ".qixiangzhan\android\release"
$defaultKeystorePath = Join-Path $releaseDir "qixiangzhan-release.jks"
$psProfile = Join-Path $releaseDir "release-signing.env.ps1"
$cmdProfile = Join-Path $releaseDir "release-signing.env.cmd"

New-Item -ItemType Directory -Force -Path $releaseDir | Out-Null

if (Test-Path $psProfile) {
    . $psProfile
}

if ([string]::IsNullOrWhiteSpace($KeystorePath)) {
    $KeystorePath = if ([string]::IsNullOrWhiteSpace($env:QXZ_ANDROID_KEYSTORE_PATH)) { $defaultKeystorePath } else { $env:QXZ_ANDROID_KEYSTORE_PATH }
}
if ([string]::IsNullOrWhiteSpace($Alias)) {
    $Alias = if ([string]::IsNullOrWhiteSpace($env:QXZ_ANDROID_KEYSTORE_ALIAS)) { "qixiangzhan-release" } else { $env:QXZ_ANDROID_KEYSTORE_ALIAS }
}
if ([string]::IsNullOrWhiteSpace($StorePass)) {
    $StorePass = if ([string]::IsNullOrWhiteSpace($env:QXZ_ANDROID_KEYSTORE_STORE_PASS)) { New-RandomPassword } else { $env:QXZ_ANDROID_KEYSTORE_STORE_PASS }
}
if ([string]::IsNullOrWhiteSpace($KeyPass)) {
    $KeyPass = if ([string]::IsNullOrWhiteSpace($env:QXZ_ANDROID_KEYSTORE_KEY_PASS)) { $StorePass } else { $env:QXZ_ANDROID_KEYSTORE_KEY_PASS }
}

if ((Test-Path $KeystorePath) -and
    [string]::IsNullOrWhiteSpace($env:QXZ_ANDROID_KEYSTORE_STORE_PASS) -and
    [string]::IsNullOrWhiteSpace($storePassInput) -and
    [string]::IsNullOrWhiteSpace($env:QXZ_ANDROID_KEYSTORE_KEY_PASS) -and
    [string]::IsNullOrWhiteSpace($keyPassInput) -and
    -not (Test-Path $psProfile)) {
    throw "Keystore already exists. Provide the existing signing passwords before regenerating helper profiles."
}

$env:QXZ_ANDROID_KEYSTORE_PATH = $KeystorePath
$env:QXZ_ANDROID_KEYSTORE_ALIAS = $Alias
$env:QXZ_ANDROID_KEYSTORE_STORE_PASS = $StorePass
$env:QXZ_ANDROID_KEYSTORE_KEY_PASS = $KeyPass

$psContent = @(
    '$env:QXZ_ANDROID_KEYSTORE_PATH = "' + $KeystorePath + '"'
    '$env:QXZ_ANDROID_KEYSTORE_ALIAS = "' + $Alias + '"'
    '$env:QXZ_ANDROID_KEYSTORE_STORE_PASS = "' + $StorePass + '"'
    '$env:QXZ_ANDROID_KEYSTORE_KEY_PASS = "' + $KeyPass + '"'
)
[IO.File]::WriteAllLines($psProfile, $psContent)

$cmdContent = @(
    '@echo off'
    'set "QXZ_ANDROID_KEYSTORE_PATH=' + $KeystorePath + '"'
    'set "QXZ_ANDROID_KEYSTORE_ALIAS=' + $Alias + '"'
    'set "QXZ_ANDROID_KEYSTORE_STORE_PASS=' + $StorePass + '"'
    'set "QXZ_ANDROID_KEYSTORE_KEY_PASS=' + $KeyPass + '"'
)
[IO.File]::WriteAllLines($cmdProfile, $cmdContent)

if (-not (Test-Path $KeystorePath)) {
    & $keytool -genkeypair `
        -v `
        -keystore $KeystorePath `
        -alias $Alias `
        -storetype PKCS12 `
        -keyalg RSA `
        -keysize 4096 `
        -validity $ValidityDays `
        -dname $DName `
        -storepass $StorePass `
        -keypass $KeyPass
}

Write-Host ""
Write-Host "Android keystore ready:"
Write-Host "  $KeystorePath"
Write-Host "Alias:"
Write-Host "  $Alias"
Write-Host "Local signing profile:"
Write-Host "  $psProfile"
