@echo off
:: Lance Oni en fenetre, comme hote, sur le port UDP 27777 (ou celui passe en argument).
:: Le jeu lance est celui que l'AE Installer tient a jour (voir game_dir.bat) : les mods qui y
:: sont installes sont ceux de la partie.
setlocal
set "PORT=%~1"
if "%PORT%"=="" set "PORT=27777"
set "ONIMP=host:%PORT%"
call "%~dp0launch.bat"
