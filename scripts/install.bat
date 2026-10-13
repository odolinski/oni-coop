@echo off
:: Installe OniMP dans le dossier du jeu (voir game_dir.bat) : le realbink.dll d'origine devient
:: realbink_orig.dll, la DLL OniMP prend sa place.
setlocal
call "%~dp0game_dir.bat" || exit /b 1
set "BUILT=%~dp0..\build\realbink.dll"

if not exist "%BUILT%" (
    echo "%BUILT%" introuvable : compiler d'abord ^(voir README.md^).
    exit /b 1
)

if not exist "%GAME%\realbink_orig.dll" (
    if not exist "%GAME%\realbink.dll" (
        echo realbink.dll introuvable : Daodan ^(Anniversary Edition^) est requis.
        exit /b 1
    )
    findstr /m /c:"OniMP" "%GAME%\realbink.dll" >nul 2>&1
    if not errorlevel 1 (
        echo realbink.dll est deja la DLL OniMP et realbink_orig.dll a disparu.
        echo Restaurer le realbink.dll d'origine ^(AE Installer^) puis relancer.
        exit /b 1
    )
    ren "%GAME%\realbink.dll" realbink_orig.dll || exit /b 1
    echo realbink.dll d'origine renomme en realbink_orig.dll
)

copy /y "%BUILT%" "%GAME%\realbink.dll" >nul || exit /b 1
if not exist "%GAME%\onimp.ini" copy "%~dp0..\onimp.ini" "%GAME%\onimp.ini" >nul
if not exist "%GAME%\onimp_difficulty.ini" copy "%~dp0..\onimp_difficulty.ini" "%GAME%\onimp_difficulty.ini" >nul
:: Son joue une fois quand un joueur tombe a terre (volume : onglet Coop du menu F1).
if exist "%~dp0..\sounds\AlertFall.mp3" copy /y "%~dp0..\sounds\AlertFall.mp3" "%GAME%\onimp_alert.mp3" >nul
if exist "%GAME%\onimp_heartbeat.mp3" del "%GAME%\onimp_heartbeat.mp3"
echo OniMP installe dans "%GAME%".
