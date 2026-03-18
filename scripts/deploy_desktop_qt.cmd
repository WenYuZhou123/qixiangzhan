@echo off
setlocal
powershell -ExecutionPolicy Bypass -File "%~dp0deploy_desktop_qt.ps1" %*
endlocal
