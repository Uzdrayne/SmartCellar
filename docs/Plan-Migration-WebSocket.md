# Plan de migration de l'API Web vers WebSocket

## Objectif

Faire évoluer progressivement le serveur pour que les données et les actions de
configuration soient accessibles depuis une API WebSocket réutilisable par
l'interface Web actuelle et, éventuellement, par une autre interface sur le
réseau local.

Ce document sert à recueillir les questions, réponses et décisions avant toute
implémentation de cette migration. L'écran LVGL déporté reste hors périmètre
pour l'instant; il constitue seulement un client futur possible de l'API.

## État actuel vérifié

- Les pages HTML, scripts, styles, l'écran de configuration initiale et les
  routes du portail captif sont servis en HTTP. Ces ressources ne sont pas des
  commandes de l'API et devront rester accessibles en HTTP.
- Le WebSocket `/ws` diffuse le JSON global environ une fois par seconde. Il
  journalise les connexions et déconnexions, mais ne traite pas encore de
  commandes reçues.
- Les données globales et l'état d'alarme sont aussi accessibles par
  `GET /GlobalStatus` et `GET /AlarmSettings`.
- Les actions fonctionnelles passent actuellement par des POST : consignes,
  scan et paramètres Wi-Fi, MQTT, capteur, relais, ventilateur et alarme, ainsi
  que redémarrage et création initiale du compte.
- L'authentification HTTP Basic est appliquée par middleware, y compris à la
  mise à niveau du WebSocket. Les POST et le handshake WebSocket font également
  l'objet d'un contrôle d'origine. Le serveur utilise actuellement HTTP/WS sans
  TLS.
- Le scan Wi-Fi est asynchrone : le client lance le scan, interroge son résultat
  en HTTP, et l'envoi périodique des données WebSocket est suspendu pendant le
  scan. Le mode Wi-Fi précédant le scan est restauré à la fin.
- La sauvegarde des consignes température et humidité est désormais séparée
  dans `Save_SetTemp()` et `Save_SetHum()`.

## Questions et décisions à valider

Les réponses ci-dessous restent ouvertes jusqu'à confirmation explicite.

### Q1 — Périmètre du remplacement HTTP

**Question :** Souhaite-t-on remplacer par WebSocket uniquement les échanges
fonctionnels (lecture des données et commandes), tout en conservant HTTP GET
pour charger les pages, fichiers statiques, portail captif et configuration
initiale du compte administrateur ?

**Décision :** Oui. WebSocket servira l'API; HTTP restera utilisé pour les
pages, fichiers statiques, portail captif et configuration initiale du compte
administrateur. Cette configuration initiale correspond à la création du
premier mot de passe Web, lorsqu'aucun compte n'existe encore.

### Q2 — Clients à prendre en charge

**Question :** Quels clients doivent être considérés dès la première version du
protocole : navigateur Web actuel seulement, ou également des clients externes
futurs (par exemple une application distincte sur le réseau local) ?

**Décision :** Concevoir un protocole réutilisable par différents clients dès
le départ; migrer l'interface Web actuelle en premier.

### Q3 — Format des messages

**Question :** Veut-on un format JSON commun pour les commandes, les réponses,
les erreurs et les événements serveur ? Faut-il inclure un identifiant de
requête et une version de protocole ?

**Décision :** Oui. Base proposée : commandes JSON avec `id`, `action` et
`payload`; réponse corrélée avec le même `id`, un résultat succès/erreur et, au
besoin, des données. Prévoir une version du protocole.

### Q4 — État et mises à jour en direct

**Question :** À la connexion, faut-il envoyer immédiatement un état complet,
puis continuer à publier l'état périodiquement, ou ne publier que les
changements ? Quel niveau de fréquence est souhaité ?

**Éléments vérifiés :** Le JSON global est écrit dans un tampon de 4096 octets;
la génération mesure le JSON et refuse explicitement de le sérialiser si cette
limite est atteinte. La taille réelle n'est pas mesurée dans les logs. Le même
JSON complet est déjà diffusé en WebSocket environ une fois par seconde, donc
conserver ce flux ne devrait pas augmenter le trafic par rapport à aujourd'hui.

**Décision :** Envoyer un état complet dès la connexion, puis environ une fois
par seconde aux clients connectés.

### Q5 — Validation et résultat des commandes

**Question :** Pour chaque commande de modification, quelles informations le
client doit-il recevoir : simple succès/échec, valeurs réellement appliquées,
erreur de validation détaillée, résultat de sauvegarde persistante ?

**Décision :** La réponse devra distinguer l'état appliqué en mémoire du
résultat de la sauvegarde durable.

### Q6 — Opérations qui interrompent ou occupent le réseau

#### Scan Wi-Fi

**Question :** Comment le client doit-il vivre un scan Wi-Fi qui peut durer et
éventuellement interrompre la connexion ?

**Décision :** Opération asynchrone : le serveur confirme son acceptation,
notifie le résultat avec la liste des réseaux, et le client tolère une coupure
temporaire; après reconnexion, il récupère un nouvel état complet.

#### Changement de réseau et redémarrage

**Question :** Lorsqu'un client enregistre de nouveaux paramètres Wi-Fi ou
MQTT, faut-il redémarrer automatiquement l'appareil ou garder une commande de
redémarrage distincte ?

**Décision :** Garder deux commandes distinctes : sauvegarder la configuration,
confirmer le résultat et signaler qu'un redémarrage est nécessaire; le client
demande ensuite explicitement le redémarrage.

### Q7 — Authentification et sécurité du transport

#### Authentification des clients

**Question :** Souhaite-t-on conserver l'authentification actuelle au handshake
WebSocket pour tous les clients, ou définir une autre méthode pour faciliter
les clients externes ?

**Décision :** Réutiliser le compte `admin` existant et vérifier ses
identifiants à l'ouverture du WebSocket; ne pas créer un système de compte
distinct pour chaque interface.

#### Chiffrement du transport

**Question :** Le besoin de chiffrement TLS/WSS doit-il faire partie de cette
migration ou d'une étape séparée ?

**Éléments de cadrage :** L'authentification seule ne chiffre pas le trafic :
`ws://` ne protège pas le mot de passe ni les futures commandes sur le réseau.
Le Cyber Resilience Act fixe des exigences essentielles de cybersécurité pour
les produits concernés, mais ne prescrit pas TLS/WSS comme solution universelle
pour chaque produit. Son applicabilité et les mesures nécessaires dépendent
notamment du produit, de son marché et de son analyse de risque. L'application
générale du CRA est prévue à partir du 11 décembre 2027.

Sources :

- [Commission européenne — résumé du Cyber Resilience Act](https://digital-strategy.ec.europa.eu/en/policies/cra-summary)
- [Règlement (UE) 2024/2847 — EUR-Lex](https://eur-lex.europa.eu/eli/reg/2024/2847/oj/eng)

**Décision :** Hors périmètre de la migration actuelle. À réévaluer avant une
éventuelle commercialisation. Pour éviter une refonte du protocole à ce moment,
les messages et la logique métier devront rester indépendants du transport.
L'étude future devra notamment traiter l'écoute WSS, l'émission et le
renouvellement des certificats, ainsi que leur confiance par les clients.

### Q8 — Migration progressive

**Question :** Pendant la transition, faut-il garder temporairement les routes
HTTP fonctionnelles en parallèle avec WebSocket, puis les retirer seulement
après validation de tous les clients ?

**Décision :** Basculer toutes les fonctionnalités API en une seule fois, sans
période de coexistence avec les anciennes routes HTTP fonctionnelles. Les GET
nécessaires aux pages, fichiers statiques, portail captif et configuration
initiale restent conservés comme décidé en Q1.

### Q9 — Secrets dans les états et les réponses

**Question :** Les mots de passe Wi-Fi et MQTT doivent-ils rester absents de
tous les états et réponses, et n'être transmis que lorsqu'un client les change
explicitement ?

**Décision :** Oui. Les mots de passe ne seront pas renvoyés dans les états ou
réponses; ils seront transmis uniquement lors d'un changement explicite, avec
une action explicite pour les effacer si nécessaire.

### Q10 — Commande envoyée sans réponse reçue

**Question :** Si la connexion tombe après l'envoi d'une commande et avant sa
réponse, comment le client doit-il éviter de répéter involontairement une
opération qui a peut-être déjà eu lieu ?

**Décision :** Le serveur doit reconnaître les répétitions récentes portant le
même identifiant et éviter de réexécuter la commande. Après reconnexion, le
client récupère un état complet avant de décider s'il faut envoyer une nouvelle
commande.

### Q11 — Plusieurs clients simultanés

**Question :** Plusieurs clients authentifiés peuvent-ils lire et modifier les
réglages en même temps ? Si oui, comment gérer deux modifications concurrentes ?

**Décision :** Autoriser plusieurs clients. Leurs commandes seront traitées
dans l'ordre; en cas de réglages concurrents, la dernière commande acceptée
prévaudra et l'état actualisé sera diffusé aux clients.

### Q12 — Résultat des sauvegardes persistantes

**Question :** Certaines fonctions de sauvegarde NVS ne renvoient actuellement
aucun résultat. Faut-il les adapter pour signaler précisément un succès ou un
échec de sauvegarde aux commandes WebSocket ?

**Décision :** Oui. Adapter les fonctions NVS concernées pour retourner leur
résultat et vérifier les erreurs d'écriture afin que les réponses distinguent
fiablement l'application en mémoire de la persistance.

### Q13 — Diffusion après une commande

**Question :** Après une commande qui modifie l'état, faut-il diffuser
immédiatement l'état actualisé à tous les clients ou attendre la prochaine
diffusion périodique (environ une seconde) ?

**Décision :** Attendre la prochaine diffusion périodique; la commande elle-même
reçoit son accusé individuel.

### Q14 — Lectures spécialisées

**Question :** Les données spécialisées comme les détails de configuration
d'alarme doivent-elles être incluses dans l'état global complet, ou rester
disponibles par une commande de lecture WebSocket dédiée ?

**Décision :** Conserver une commande de lecture WebSocket dédiée pour les
données spécialisées, notamment les détails de configuration d'alarme.

### Q15 — Accusé avant redémarrage

**Question :** La commande de redémarrage doit-elle d'abord renvoyer un accusé
indiquant que le redémarrage est planifié, puis attendre que l'accusé ait eu
une chance d'être transmis avant de redémarrer l'appareil ?

**Décision :** Oui. Renvoyer un accusé de redémarrage planifié, laisser un
court délai de transmission, puis redémarrer; le client s'attend à perdre la
connexion et se reconnecte quand l'appareil est disponible.

### Q16 — En-tête Origin des clients non navigateurs

**Question :** Le handshake WebSocket doit-il continuer à exiger un en-tête
`Origin` correspondant à l'appareil, ou accepter l'absence de cet en-tête pour
des clients non navigateurs tout en refusant les origines étrangères ?

**Décision :** Accepter l'absence de `Origin` uniquement si le handshake est
authentifié; continuer à refuser un `Origin` présent mais étranger. `Origin`
reste une protection contre les connexions navigateur inter-origines et ne
remplace pas l'authentification.

### Q17 — Granularité des commandes

**Question :** Les commandes doivent-elles refléter les opérations actuelles
des pages Web, ou permettre de modifier séparément les valeurs et réglages pour
rester réutilisables par d'autres interfaces ?

**Décision :** Exposer des commandes indépendantes pour la température et
l'humidité; garder groupés les réglages qui forment un ensemble cohérent, comme
les paramètres d'un relais. La page Web devra elle aussi traiter température
et humidité séparément.

### Q18 — Présentation Web des consignes

**Question :** La page Web garde-t-elle ses boutons actuels, en envoyant une
commande séparée uniquement pour chaque valeur modifiée, ou faut-il aussi
séparer les contrôles de sauvegarde température et humidité ?

**Décision :** Garder les boutons Enregistrer/Annuler communs, mais suivre
séparément les changements et n'envoyer que les commandes correspondant aux
valeurs modifiées.

### Q19 — Échec partiel des consignes

**Question :** Si les deux consignes sont modifiées mais qu'une seule
sauvegarde réussit, faut-il afficher le résultat par consigne et ne garder en
attente que celle qui a échoué ?

**Décision :** Oui. Afficher le résultat par consigne; une consigne sauvegardée
est terminée, et seule celle en échec reste à corriger ou réessayer.

### Q20 — Accès réseau visé

**Question :** Cette API vise-t-elle uniquement les clients sur le réseau
Wi-Fi local ou le point d'accès de l'appareil, sans accès distant via Internet
ni service cloud dans cette migration ?

**Décision :** Oui. Accès local uniquement, sur le Wi-Fi ou le point d'accès
du SmartCellar. L'accès distant et les services cloud sont hors périmètre.

## Synthèse des décisions

- Garder HTTP pour les pages, ressources statiques, portail captif et création
  initiale du compte administrateur; migrer toutes les fonctions de l'API en
  une seule bascule vers WebSocket.
- Concevoir le protocole pour plusieurs types de clients, en migrant d'abord le
  site Web actuel. L'API reste accessible sur le réseau local.
- Utiliser des messages JSON versionnés avec `id`, `action` et `payload`, des
  réponses corrélées, et éviter de réexécuter une commande répétée avec le même
  identifiant.
- Envoyer un état global à la connexion, puis environ une fois par seconde.
  Les limites de configuration, constantes, sont exclues de cet état : le
  client les lit une fois par connexion avec `getLimits` et les fusionne dans
  `state.Limits`. Diffuser les lectures spécialisées, comme les réglages
  d'alarme, avec des commandes dédiées. Après une commande, les autres clients
  attendent la prochaine diffusion périodique.
- Traiter température et humidité séparément dans l'API et dans l'interface
  Web, tout en conservant les boutons Web Enregistrer/Annuler communs. Les
  réglages cohérents, comme ceux d'un relais, restent groupés.
- Renvoyer pour chaque consigne le résultat séparé; distinguer application en
  mémoire et sauvegarde durable. Adapter les fonctions NVS qui ne rapportent
  pas actuellement leur résultat; en cas d'échec partiel, garder seulement la
  consigne en échec à reprendre.
- Ne jamais renvoyer les mots de passe Wi-Fi ou MQTT; les transmettre
  uniquement lors d'un changement explicite.
- Autoriser plusieurs clients authentifiés. Traiter les commandes dans
  l'ordre, avec la dernière valeur acceptée gagnante.
- Gérer le scan de façon asynchrone et tolérer reconnexion et resynchronisation.
  Sauvegarder les nouveaux paramètres avant une commande de redémarrage
  distincte; accuser le redémarrage avant d'attendre brièvement puis de
  redémarrer.
- Réutiliser le compte admin au handshake. Accepter un handshake sans `Origin`
  seulement s'il est authentifié; refuser un `Origin` étranger lorsqu'il est
  présent.
- N'accepter que les clients du réseau local ou du point d'accès; pas d'accès
  distant ni cloud dans cette migration.
- Laisser TLS/WSS hors de la migration actuelle, mais réévaluer la sécurité du
  transport avant une éventuelle commercialisation.

## Journal des décisions

Les décisions prises pendant la séance sont consignées dans les sections
ci-dessus. La migration a été implémentée : les pages fonctionnelles utilisent
le client WebSocket partagé pour lire l'état et envoyer leurs commandes. Les
pages, ressources statiques et la création initiale du compte administrateur
restent servies en HTTP; cette dernière reste le seul POST fonctionnel.

Le scan Wi-Fi est asynchrone, ses résultats sont transmis par événement et
peuvent être récupérés après reconnexion. La sauvegarde des paramètres Wi-Fi ou
MQTT reste distincte de la commande de redémarrage. Les essais sur l'appareil
et la compilation du firmware ont réussi; le téléversement vers l'ESP32-C3 a
également réussi. Le serveur local répond avec le challenge d'authentification
attendu. Un essai authentifié complet a ensuite été mené : toutes les pages
chargent leurs données et leurs limites, et les commandes de lecture répondent
en une dizaine de millisecondes.

État mémoire et diffusion (novembre 2026) :

- Le pool LVGL est réduit de 128 à 64 Ko (l'interface en utilise environ
  40 Ko); il ne restait auparavant qu'environ 19 Ko de heap, ce qui provoquait
  les erreurs UART `NoMemory`, les coupures MQTT et les transferts HTTP
  tronqués dès qu'un navigateur se connectait.
- L'état n'est construit que si un client WebSocket est connecté. Il est
  sérialisé une seule fois, directement dans le tampon partagé envoyé à tous
  les clients; les anciens tampons `JSON_GLOBAL_STATUS` et `stateFrame`
  (hérités de la route HTTP `/GlobalStatus`) sont supprimés.
- Une trame d'état n'est mise en file que pour un client dont la file d'envoi
  est vide : les réponses aux commandes ne sont jamais évincées par des états.
- ESPAsyncWebServer ne renseigne pas `message_opcode` pour une trame reçue en
  un seul paquet TCP; le serveur vérifie donc `opcode` et refuse les messages
  fragmentés en plusieurs trames.
- La page Alertes s'abonne à l'état diffusé au lieu d'interroger
  `getAlarmSettings` toutes les deux secondes; cette commande reste disponible
  pour d'autres clients.

Les appels MQTT potentiellement bloquants sont isolés dans une tâche distincte
de la boucle de régulation et de la liaison UART. Le serveur MQTT doit être
configuré sous forme de nom d'hôte ou d'adresse IP, sans préfixe `mqtt://`.
Les ressources Web embarquées sont renvoyées avec `Cache-Control: no-store`
pour qu'un rechargement après téléversement récupère bien la version courante.
