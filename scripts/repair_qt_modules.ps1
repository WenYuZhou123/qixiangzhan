$maintenanceTool = "D:\Qt\MaintenanceTool.exe"
$installRoot = "D:\Qt"

if (-not (Test-Path $maintenanceTool)) {
    throw "MaintenanceTool.exe not found at $maintenanceTool"
}

Write-Host "Clearing installer cache..."
& $maintenanceTool cc -t $installRoot --accept-licenses --default-answer

Write-Host "Reinstalling Qt add-ons for MQTT / SerialPort..."
& $maintenanceTool install `
    qt.qt6.673.addons.qtmqtt `
    qt.qt6.673.addons.qtserialport `
    -t $installRoot `
    --accept-licenses `
    --default-answer `
    --confirm-command

Write-Host "Listing installed Qt 6.7.3 packages..."
& $maintenanceTool li qt.qt6.673.* -t $installRoot
