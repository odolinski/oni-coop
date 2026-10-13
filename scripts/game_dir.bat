@echo off
:: Donne dans GAME le dossier du jeu, pour les autres scripts : celui que l'AE Installer tient a
:: jour, ou se trouvent donc les mods choisis.
::   1. la variable ONIMP_GAME, si elle est definie ;
::   2. le sous-dossier AE du dossier qui contient OniMP, s'il a son Oni.exe et son AEInstaller
::      (disposition de l'Anniversary Edition : Oni\AE) ;
::   3. sinon le dossier qui contient OniMP.
set "GAME=%~dp0..\.."
if exist "%GAME%\AE\Oni.exe" if exist "%GAME%\AE\AEInstaller\" set "GAME=%GAME%\AE"
if defined ONIMP_GAME set "GAME=%ONIMP_GAME%"
for %%I in ("%GAME%") do set "GAME=%%~fI"
if exist "%GAME%\Oni.exe" exit /b 0
echo Oni.exe introuvable dans "%GAME%".
exit /b 1
