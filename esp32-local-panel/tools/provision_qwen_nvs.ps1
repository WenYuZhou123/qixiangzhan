[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidatePattern('^COM\d+$')]
    [string]$Port,

    [string]$ApiConfigFile = 'D:\OneDrive\Desktop\千问API Key.txt',

    [Parameter(Mandatory = $true)]
    [ValidateLength(1, 32)]
    [string]$WifiSsid,

    [Parameter(Mandatory = $true)]
    [ValidateLength(8, 63)]
    [string]$WifiPassword,

    [ValidateLength(1, 64)]
    [string]$DeviceId = 'relay_h743_001',

    [ValidateLength(1, 64)]
    [string]$Model = 'qwen3.7-plus',

    [ValidateRange(30, 3600)]
    [int]$AutoAnalyzeSeconds = 60
)

$ErrorActionPreference = 'Stop'

function ConvertTo-NvsCsvValue {
    param([Parameter(Mandatory = $true)][string]$Value)
    return '"' + $Value.Replace('"', '""') + '"'
}

function Find-IdfPython {
    $venvPython = if ($env:IDF_PYTHON_ENV_PATH) {
        Join-Path $env:IDF_PYTHON_ENV_PATH 'Scripts\python.exe'
    }
    if ($venvPython -and (Test-Path -LiteralPath $venvPython)) {
        return $venvPython
    }
    $defaultPython = 'C:\Espressif\tools\python\v6.0.2\venv\Scripts\python.exe'
    if (Test-Path -LiteralPath $defaultPython) {
        return $defaultPython
    }
    throw 'ESP-IDF Python virtual environment was not found. Open ESP-IDF PowerShell first.'
}

if (-not (Test-Path -LiteralPath $ApiConfigFile)) {
    throw "Qwen configuration file not found: $ApiConfigFile"
}

$configText = Get-Content -LiteralPath $ApiConfigFile -Encoding UTF8 -Raw
$apiKeyMatch = [regex]::Match($configText, '\bsk-[A-Za-z0-9._-]+\b')
$endpointMatch = [regex]::Match($configText, 'https://[^\s"''，,]+/compatible-mode/v1')
if (-not $apiKeyMatch.Success -or -not $endpointMatch.Success) {
    throw 'The configuration file must contain an API key and an OpenAI-compatible endpoint.'
}

$apiUrl = $endpointMatch.Value.TrimEnd('/') + '/chat/completions'
$idfPath = if ($env:IDF_PATH) { $env:IDF_PATH } else { 'D:\esp\v6.0.2\esp-idf' }
$generator = Join-Path $idfPath 'components\nvs_flash\nvs_partition_generator\nvs_partition_gen.py'
if (-not (Test-Path -LiteralPath $generator)) {
    throw "NVS generator was not found under IDF_PATH: $idfPath"
}

$python = Find-IdfPython
$token = [guid]::NewGuid().ToString('N')
$csvPath = Join-Path $env:TEMP "panel-qwen-$token.csv"
$binPath = Join-Path $env:TEMP "panel-qwen-$token.bin"

try {
    # This replaces the complete 0x6000 NVS partition. Include Wi-Fi credentials
    # here so the device can reconnect immediately after provisioning.
    $csv = @(
        'key,type,encoding,value',
        'panel_cfg,namespace,,',
        ('wifi_ssid,data,string,' + (ConvertTo-NvsCsvValue $WifiSsid)),
        ('wifi_pass,data,string,' + (ConvertTo-NvsCsvValue $WifiPassword)),
        ('edge_url,data,string,' + (ConvertTo-NvsCsvValue $apiUrl)),
        ('edge_key,data,string,' + (ConvertTo-NvsCsvValue $apiKeyMatch.Value)),
        ('llm_model,data,string,' + (ConvertTo-NvsCsvValue $Model)),
        ('device_id,data,string,' + (ConvertTo-NvsCsvValue $DeviceId)),
        "ai_period,data,u32,$AutoAnalyzeSeconds"
    )
    [System.IO.File]::WriteAllLines($csvPath, $csv, [System.Text.UTF8Encoding]::new($false))

    & $python $generator generate $csvPath $binPath 0x6000
    if ($LASTEXITCODE -ne 0) {
        throw 'NVS partition generation failed.'
    }

    & $python -m esptool --chip esp32s3 --port $Port write_flash 0x9000 $binPath
    if ($LASTEXITCODE -ne 0) {
        throw 'Failed to write the NVS partition. Check the COM port and boot mode.'
    }

    Write-Host 'Qwen and Wi-Fi configuration provisioned to NVS successfully.'
    Write-Host 'Restart the panel, wait for Wi-Fi and SNTP synchronization, then use AI analysis.'
}
finally {
    Remove-Item -LiteralPath $csvPath, $binPath -Force -ErrorAction SilentlyContinue
}
