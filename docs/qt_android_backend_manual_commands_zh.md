# Qt Desktop / Android / Backend 手动构建与启动命令

本文档整理当前仓库已经验证可用的本地联调命令。

## 1. Backend 启动

```powershell
Set-Location D:\Competition\code_main\qixiangzhan\backend
.venv\Scripts\python.exe -m uvicorn app.main:app --host 127.0.0.1 --port 8000 --reload
```

联调地址：

- API: `http://127.0.0.1:8000/api/v1`
- Health: `http://127.0.0.1:8000/healthz`

## 2. Qt Desktop 构建

优先使用 preset：

```powershell
Set-Location D:\Competition\code_main\qixiangzhan\qt-client
cmake --preset desktop-mingw
cmake --build --preset build-desktop-mingw
```

备用显式命令：

```powershell
Set-Location D:\Competition\code_main\qixiangzhan\qt-client
D:\Qt\6.7.3\mingw_64\bin\qt-cmake.bat -S . -B build\desktop-mingw -G "MinGW Makefiles" `
  -DCMAKE_C_COMPILER=D:/Qt/Tools/mingw1120_64/bin/gcc.exe `
  -DCMAKE_CXX_COMPILER=D:/Qt/Tools/mingw1120_64/bin/g++.exe
cmake --build build\desktop-mingw
```

当前验证通过的桌面端产物：

```text
D:\Competition\code_main\qixiangzhan\qt-client\build\desktop-mingw\relay_qt_client.exe
```

## 3. Qt Desktop 启动

```powershell
Set-Location D:\Competition\code_main\qixiangzhan
.\qt-client\build\desktop-mingw\relay_qt_client.exe
```

如果缺少 Qt 运行库：

```powershell
Set-Location D:\Competition\code_main\qixiangzhan
powershell -ExecutionPolicy Bypass -File .\scripts\deploy_desktop_qt.ps1 -ClientExe .\qt-client\build\desktop-mingw\relay_qt_client.exe
```

## 4. Android Debug 构建

```powershell
Set-Location D:\Competition\code_main\qixiangzhan
powershell -ExecutionPolicy Bypass -File .\scripts\build_android_apk.ps1 -Configuration Debug
```

当前验证通过的 Debug APK 路径：

```text
D:\Competition\code_main\qixiangzhan\qt-client\build\android-arm64\android-build\build\outputs\apk\debug\android-build-debug.apk
```

## 5. Android 本机联调

先做 ADB 反向代理，让模拟器/手机访问本机后端：

```powershell
C:\Users\Lenovo\AppData\Local\Android\Sdk\platform-tools\adb.exe reverse tcp:8000 tcp:8000
```

安装 Debug APK：

```powershell
C:\Users\Lenovo\AppData\Local\Android\Sdk\platform-tools\adb.exe install -r D:\Competition\code_main\qixiangzhan\qt-client\build\android-arm64\android-build\build\outputs\apk\debug\android-build-debug.apk
```

启动 Android App：

```powershell
C:\Users\Lenovo\AppData\Local\Android\Sdk\platform-tools\adb.exe shell am start -W -n com.qixiangzhan.platform/org.qtproject.qt.android.bindings.QtActivity
```

如果使用雷电模拟器，也可以改用：

```text
D:\leidian\LDPlayer9\adb.exe
```

## 6. 默认登录信息

- API Base: `http://127.0.0.1:8000/api/v1`
- Backend login: `admin / admin123`
