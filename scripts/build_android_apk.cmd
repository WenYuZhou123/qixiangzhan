@echo off
powershell -ExecutionPolicy Bypass -File "%~dp0build_android_apk.ps1" %*
