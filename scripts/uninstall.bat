@echo off
:: Retire OniMP du dossier du jeu (voir game_dir.bat) : remet le realbink.dll d'origine en place.
setlocal
call "%~dp0game_dir.bat" || exit /b 1

if not exist "%GAME%\realbink_orig.dll" (
    echo realbink_orig.dll introuvable dans "%GAME%" : OniMP n'y est pas installe.
    exit /b 1
)
del "%GAME%\realbink.dll" || exit /b 1
ren "%GAME%\realbink_orig.dll" realbink.dll || exit /b 1
if exist "%GAME%\onimp_alert.mp3" del "%GAME%\onimp_alert.mp3"
if exist "%GAME%\onimp_heartbeat.mp3" del "%GAME%\onimp_heartbeat.mp3"
echo OniMP desinstalle de "%GAME%". onimp.ini et les journaux onimp*.log peuvent etre supprimes a la main.
