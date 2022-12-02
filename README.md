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
!create nomDuGroupe : permet de créer un groupe au nom de nomDuGroupe
!join nomDuGroupe : permet à un utilisateur de rejoindre le groupe "nomDuGroupe"
