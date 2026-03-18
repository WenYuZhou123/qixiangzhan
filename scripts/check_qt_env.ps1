$qtRoot = "D:\Qt\6.7.3"
$desktopQt = Join-Path $qtRoot "mingw_64"
$androidQt = Join-Path $qtRoot "android_arm64_v8a"
$sdkRoot = "C:\Users\Lenovo\AppData\Local\Android\Sdk"
$ndkRoot = Join-Path $sdkRoot "ndk\26.1.10909125"

Write-Host "== Qt Kits =="
@(
    (Join-Path $desktopQt "bin\qmake.exe"),
    (Join-Path $desktopQt "bin\qt-cmake.bat"),
    (Join-Path $androidQt "lib\cmake\Qt6\Qt6Config.cmake")
) | ForEach-Object {
    "{0} : {1}" -f $_, (Test-Path $_)
}

Write-Host "`n== Optional Qt Components =="
@(
    (Join-Path $desktopQt "lib\cmake\Qt6Mqtt\Qt6MqttConfig.cmake"),
    (Join-Path $desktopQt "lib\cmake\Qt6SerialPort\Qt6SerialPortConfig.cmake"),
    (Join-Path $androidQt "lib\cmake\Qt6Mqtt\Qt6MqttConfig.cmake")
) | ForEach-Object {
    "{0} : {1}" -f $_, (Test-Path $_)
}

Write-Host "`n== Android Toolchain =="
@($sdkRoot, $ndkRoot, (Join-Path $sdkRoot "platform-tools\adb.exe")) | ForEach-Object {
    "{0} : {1}" -f $_, (Test-Path $_)
}

Write-Host "`n== Configure Smoke Test =="
if (Test-Path (Join-Path $desktopQt "bin\qt-cmake.bat")) {
    & (Join-Path $desktopQt "bin\qt-cmake.bat") `
        -S "$PSScriptRoot\..\qt-client" `
        -B "$PSScriptRoot\..\qt-client\build\desktop-smoke" `
        -G "MinGW Makefiles" `
        -DCMAKE_C_COMPILER="D:/Qt/Tools/mingw1120_64/bin/gcc.exe" `
        -DCMAKE_CXX_COMPILER="D:/Qt/Tools/mingw1120_64/bin/g++.exe"
} else {
    Write-Warning "qt-cmake.bat not found"
}

Write-Host "`n== Android Configure Smoke Test =="
$androidToolchain = Join-Path $ndkRoot "build\cmake\android.toolchain.cmake"
$makeProgram = "D:\Qt\Tools\mingw1120_64\bin\mingw32-make.exe"
if ((Test-Path (Join-Path $desktopQt "bin\qt-cmake.bat")) -and (Test-Path $androidToolchain) -and (Test-Path $makeProgram)) {
    & (Join-Path $desktopQt "bin\qt-cmake.bat") `
        -S "$PSScriptRoot\..\qt-client" `
        -B "$PSScriptRoot\..\qt-client\build\android-smoke" `
        -G "MinGW Makefiles" `
        -DCMAKE_PREFIX_PATH="D:/Qt/6.7.3/android_arm64_v8a" `
        -DQt6_DIR="D:/Qt/6.7.3/android_arm64_v8a/lib/cmake/Qt6" `
        -DQT_HOST_PATH="D:/Qt/6.7.3/mingw_64" `
        -DQT_HOST_PATH_CMAKE_DIR="D:/Qt/6.7.3/mingw_64/lib/cmake" `
        -DCMAKE_TOOLCHAIN_FILE="$androidToolchain" `
        -DCMAKE_MAKE_PROGRAM="$makeProgram" `
        -DCMAKE_FIND_ROOT_PATH_MODE_PACKAGE="BOTH" `
        -DANDROID_SDK_ROOT="$sdkRoot" `
        -DANDROID_NDK_ROOT="$ndkRoot" `
        -DANDROID_ABI="arm64-v8a" `
        -DANDROID_PLATFORM="android-34"
} else {
    Write-Warning "Android smoke test prerequisites are incomplete"
}
