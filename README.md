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
Commandes générales :
- !quit permet de quitter le serveur : déconnecte l'utilisateur comme si celui-ci avait fermé la connexion avec un Ctrl+C
- !mailbox permet de lire les messages privés qui ont été envoyés à l'utilisateur pendant son absence (déconnexion)
- !groups permet de lister tous les groupes connus
- !mygroups permet de lister tous les groupes dont l'utilisateur fait partie

Commandes relatives aux groupes :
- !create nomDuGroupe : permet de créer un groupe au nom de nomDuGroupe
- !join nomDuGroupe : permet à un utilisateur de rejoindre le groupe "nomDuGroupe"
- !histo nomDuGroupe : liste les 10 derniers messages du groupe "nomDuGroupe"
- !leave nomDuGroupe : permet à l'utilisateur de quitter le groupe "nomDuGroupe"

## Traitement des cas limites :
- Pour l'envoie de messages privés : l'utilisateur est prévenu si 
  * il essaye d'envoyer un message a un utilisateur qui n'existe pas
  * le destinataire existe mais n'est pas connecté => le message part dans la mailbox de l'utilisateur en question
- Pour l'envoie de messages à un groupe : l'utilisateur est notifié si il essaye d'envoyer un message à un groupe
  * qui n'existe pas 
  * dont il ne fait pas partie
- Pour les commandes relatives aux groupes : l'utilisateur est notifié si il essaye de 
  * créer un groupe qui existe déjà
  * rejoindre un groupe dont il fait déjà partie
  * rejoindre un groupe qui n'existe pas
  * afficher l'historique d'un groupe qui n'existe pas
  * afficher l'historique d'un groupe dont il ne fait pas partie
- Pour la lecture de mailbox : l'utilisateur est averti si
  * il essait de lire sa mailbox alors qu'elle est vide
  

