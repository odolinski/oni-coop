@echo off
:: Lance Oni en fenetre depuis le dossier du jeu (voir game_dir.bat), avec le role donne par la
:: variable ONIMP. Appele par run_host.bat et run_client.bat.
call "%~dp0game_dir.bat" || goto :failed
findstr /m /c:"OniMP" "%GAME%\realbink.dll" >nul 2>&1
if errorlevel 1 goto :not_installed
cd /d "%GAME%"
start "" Oni.exe -noswitch -debugfiles
exit /b 0

:not_installed
echo OniMP n'est pas installe dans "%GAME%" : lancer install.bat.
echo L'AE Installer reecrit realbink.dll quand il reinstalle Daodan.
:failed
pause
exit /b 1
