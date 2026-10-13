@echo off
:: Comme run_client.bat, avec les PNJ pilotes par l'hote (experimental).
:: A utiliser avec run_host_npcs.bat de l'autre cote.
set "ONIMP_NPCS=1"
call "%~dp0run_client.bat" %*
