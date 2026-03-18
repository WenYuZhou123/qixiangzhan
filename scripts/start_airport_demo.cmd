@echo off
setlocal
powershell -ExecutionPolicy Bypass -File "%~dp0start_airport_demo.ps1" %*
endlocal
