@echo off
:: Hote en coop complet par Radmin VPN : la partie n'est annoncee que sur le reseau Radmin
:: (26.x.x.x). L'autre joueur, membre du meme reseau Radmin, lance run_client_radmin.bat et la
:: rejoint sans saisir d'adresse.
set "ONIMP_QUESTS=1"
set "ONIMP_INTERFACE=radmin"
:: Titre de la fenetre du jeu, pose par OniMP des qu'elle existe.
set "ONIMP_TITLE=Host - Oni"
call "%~dp0run_host.bat" %*
