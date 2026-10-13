@echo off
:: Hote en coop complet (experimental) : PNJ pilotes par l'hote + scripts de niveau
:: (objectifs, portes, spawns, dialogues) decides par l'hote.
:: A utiliser avec run_client_coop.bat de l'autre cote.
set "ONIMP_QUESTS=1"
call "%~dp0run_host.bat" %*
