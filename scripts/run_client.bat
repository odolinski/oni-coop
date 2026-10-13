@echo off
:: Lance Oni en fenetre, comme client. Argument facultatif : adresse de l'hote (IP ou IP:port).
:: Sans argument : hote sur la meme machine.
:: Avec "auto" : cherche l'hote annonce sur le reseau (Radmin VPN, reseau local) et le rejoint.
:: Le jeu lance est celui que l'AE Installer tient a jour (voir game_dir.bat) : les mods qui y
:: sont installes doivent etre ceux de l'hote.
setlocal
set "TARGET=%~1"
if "%TARGET%"=="" set "TARGET=127.0.0.1:27777"
set "ONIMP=connect:%TARGET%"
call "%~dp0launch.bat"
