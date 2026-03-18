@echo off
powershell -ExecutionPolicy Bypass -File "%~dp0generate_android_keystore.ps1" %*
