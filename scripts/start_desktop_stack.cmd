@echo off
setlocal
powershell -ExecutionPolicy Bypass -File "%~dp0start_desktop_stack.ps1" %*
endlocal
