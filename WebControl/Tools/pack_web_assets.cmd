@echo off
set SCRIPT_DIR=%~dp0
py -3 "%SCRIPT_DIR%pack_web_assets.py" %*
if errorlevel 1 exit /b %errorlevel%
exit /b 0
