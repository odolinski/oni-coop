@echo off
:: Comme run_host.bat, avec les PNJ pilotes par l'hote (experimental).
:: A utiliser avec run_client_npcs.bat de l'autre cote.
set "ONIMP_NPCS=1"
call "%~dp0run_host.bat" %*
