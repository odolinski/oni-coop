@echo off
:: Ouvre l'AE Installer du jeu que lancent les scripts run_*.bat (voir game_dir.bat) : les mods
:: coches ici sont ceux de la partie. Les deux joueurs doivent installer les memes.
setlocal
call "%~dp0game_dir.bat" || goto :failed
if not exist "%GAME%\AEInstaller\bin\AEInstaller2.jar" goto :missing
:: Le Java livre avec l'AE Installer : il ne demarre pas avec un Java recent.
if not exist "%GAME%\AEInstaller\JRE\bin\javaw.exe" goto :missing
cd /d "%GAME%\AEInstaller"
start "" "JRE\bin\javaw.exe" -Dsun.java2d.d3d=false -jar bin\AEInstaller2.jar
exit /b 0

:missing
echo AE Installer introuvable dans "%GAME%\AEInstaller".
:failed
pause
exit /b 1
