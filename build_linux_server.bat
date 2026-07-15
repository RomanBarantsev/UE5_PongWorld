@echo off
setlocal

powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0Scripts\BuildLinuxServer.ps1" %*
exit /b %ERRORLEVEL%
