@echo off
:: Client en coop complet par Radmin VPN : cherche tout seul l'hote annonce sur le reseau Radmin
:: (26.x.x.x) et le rejoint. A utiliser avec run_host_radmin.bat de l'autre cote.
set "ONIMP_QUESTS=1"
set "ONIMP_INTERFACE=radmin"
:: Titre de la fenetre du jeu, pose par OniMP des qu'elle existe.
set "ONIMP_TITLE=Client - Oni"
call "%~dp0run_client.bat" auto
