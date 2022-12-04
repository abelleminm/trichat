Alexandre BELLEMIN-MAGNINOT, Clément BOUSSEAU, Jean-Baptiste LEPIDI présentent :
# trichat
Un chat par un grand trinôme

## Initialisation 
make depuis la racine du projet, puis lancer le server ("./server")

## Se connecter au serveur
"./connect [address] [pseudonyme]"
Les 2 paramètres sont obligatoires, au vu de se voir refuser la connexion

## Commandes de base
Ecrire directement depuis son terminal envoie un message à toutes les personnes connectées

- @username : permet d'écrire à l'utilisateur spécifié directement. Les autres ne verront pas les messages échangées via cette commande.
- #group : permet d'envoyer d'écrire à un groupe existant. Toute personne hors du groupe ne verra pas le message
- !command : permet d'effectuer une commande directement au serveur.

## Commandes pour le serveur (précédé de "!")
- !create nomDuGroupe : permet de créer un groupe au nom de nomDuGroupe
- !join nomDuGroupe : permet à un utilisateur de rejoindre le groupe "nomDuGroupe"
- !histo nomDuGroupe : liste les 10 derniers messages du groupe "nomDuGroupe"

## Traitement des cas limites :
- Pour l'envoie de messages privés : l'utilisateur est prévenu si 
  * il essaye d'envoyer un message a un utilisateur qui n'existe pas
  * l'utilisateur existe mais n'est pas connecté (et ne recevra donc pas le message)
- Pour l'envoie de messages à un groupe : l'utilisateur est notifié si 
  * il essaye d'envoyer un message à un groupe qui n'existe pas 
  * il essaye d'envoyer un message à un groupe dont il ne fait pas partie
- Pour les commandes relatives aux groupes : l'utilisateur est notifié si il essaye de 
  * créer un groupe qui existe déjà
  * rejoindre un groupe dont il fait déjà partie
  * rejoindre un groupe qui n'existe pas
  * afficher l'historique d'un groupe qui n'existe pas
  * afficher l'historique d'un groupe dont il ne fait pas partie
