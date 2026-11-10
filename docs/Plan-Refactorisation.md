# Plan — Refactorisations issues du mini-audit

Ce document liste les chantiers repérés lors du mini-audit du firmware
(novembre 2026). Les corrections ponctuelles ont déjà été faites; les points
ci-dessous touchent plusieurs modules et doivent être validés avant de coder.

## Déjà corrigé pendant l'audit

- Écran d'accueil : l'état MQTT était lu avec `mqtt.connected()` depuis la
  boucle LVGL, alors que le client MQTT tourne dans sa propre tâche
  (`PubSubClient::connected()` peut fermer le socket). Remplacé par le drapeau
  protégé `Get_MqttConnected()`.
- Écrans LVGL : une seule fonction `SetLabelTextIfChanged()` dans
  `Lvgl_Global` remplace les copies locales; les libellés ne sont plus
  réécrits (réallocation + redessin) toutes les 500 ms quand rien ne change.
- Tampon global `bufftext` et `sprintf` non bornés supprimés (écran d'accueil).
- `CaveController` : code mort supprimé (`PrintControllerMsgs`, huit tampons
  de 200 octets, `applyRelays`, ligne dupliquée).
- Test obsolète retiré : il exigeait un ping `TEST` UART de débogage qui
  n'existe plus et perturberait le protocole JSON avec le Nano.

## Chantiers proposés

Chaque point indique l'intérêt, l'effort estimé et une recommandation.

### R0 — Reprise de contrôle sans couper les relais (prioritaire)

**Constat (mesuré sur l'appareil) :** pendant un redémarrage de l'ESP32, le
Nano passe en mode secours et pilote lui-même les sorties (ici Froid et
Humidification en marche). Au retour du C3, les états commandés repartent à
« arrêt » et le délai minimal OFF est compté depuis le démarrage : le C3 coupe
donc le compresseur vers 45 s, puis le redemande vers 64 s; le Nano, qui
applique aussi son délai OFF de 60 s, le redémarre vers 113 s. Résultat : un
arrêt d'environ 65 s suivi d'un redémarrage du compresseur à chaque reboot de
l'ESP (sauvegarde Wi-Fi/MQTT, téléversement, plantage).

**Proposition :** avant la première commande envoyée au Nano, reprendre l'état
réel des sorties depuis le retour GPIO de la télémétrie (`RelayFeedback`) et
considérer le délai minimal ON comme déjà écoulé pour un relais déjà en marche.
La régulation continue alors sans coupure; elle n'arrête le relais que si les
seuils l'exigent.

**Effort :** faible à moyen (CaveController + SerialLink), mais touche la
logique de régulation : à valider et à tester sur l'appareil. **Intérêt :**
élevé (usure du compresseur).

**Décision :** Fait. Avant sa première commande, le C3 reprend l'état réel des relais remonté par le Nano (attente max. 15 s); un relais en marche garde son état, son délai min. ON est considéré écoulé. Vérifié sur l'appareil : Froid et Humidification restent en marche pendant un redémarrage.

### R1 — Sortir les messages de débogage de `StrctDataController`

**Constat :** la structure contient 4 messages de 200 octets (800 octets sur
environ 1 Ko). Elle est copiée entièrement à chaque instantané : régulation
(4 fois/s), écran LVGL (2 fois/s), état WebSocket (1 fois/s), MQTT, commandes
Web. Chaque copie consomme ~1 Ko de pile et de copie mémoire pour des textes
qui ne servent qu'à la page Menu.

**Proposition :** une structure `ControllerDebugMessages` séparée, avec son
propre accesseur, lue uniquement par l'état WebSocket. Les instantanés de
régulation, LVGL et MQTT ne copient plus que les valeurs utiles.

**Effort :** moyen (ControllerData, CaveController, état JSON). **Intérêt :**
élevé (pile et CPU de toutes les tâches).

**Décision :** Fait. `ControllerDebugMessages DataDebugMessages` séparé, protégé par DataMutex; `StrctDataController` passe d'environ 1 Ko à 200 octets.

### R2 — Bibliothèque `HomeAssistantMQTT`

**Constat :**
- Les messages de découverte et d'état sont construits par concaténation de
  `String` (nombreuses allocations temporaires, fragmentation du heap).
- Les valeurs ne sont pas échappées en JSON (un `"` dans un texte casserait le
  message).
- `setValue()` fait des `new`/`delete` à chaque changement de taille.
- L'état complet est republié toutes les 2 s, même si rien n'a changé.

**Proposition :** construire les messages avec ArduinoJson dans un tampon fixe,
stocker les valeurs dans un tableau de taille fixe, et ne publier l'état que
s'il a changé (ou au plus toutes les 5 minutes, comme aujourd'hui pour chaque
valeur).

**Effort :** moyen à élevé (bibliothèque + `my_MQTT.cpp`, à valider avec Home
Assistant). **Intérêt :** élevé (stabilité mémoire, moins de trafic).

**Décision :** Fait. Valeurs en tableau fixe, messages construits avec ArduinoJson (échappement correct) et envoyés en une écriture, état publié seulement s'il change (ou toutes les 5 min), tampon PubSubClient réduit de 2048 à 512 octets. Topics et unique_id inchangés.

### R3 — Publication MQTT pilotée par une table

**Constat :** `my_MQTT.cpp` gère chaque valeur à la main : 16 horodatages
`T_*`, une copie `LastLocalData`, et un bloc `if` répété par valeur. Ajouter
une entité demande de modifier quatre endroits.

**Proposition :** une table de descripteurs (nom HA, lecture de la valeur,
format) parcourue en boucle pour la découverte et pour les publications. À
faire en même temps que R2.

**Effort :** moyen. **Intérêt :** maintenabilité.

**Décision :** Fait. Table `Entities` dans `my_MQTT.cpp` (configuration de découverte + formatage de la valeur). Corrigé au passage : une consigne reçue de Home Assistant n'était pas sauvegardée en NVS.

### R4 — Découper `WebSocketApi.cpp`

**Constat :** près de 900 lignes; `handleCommand()` est une longue chaîne de
`if (action == ...)` qui mélange consignes, relais, Wi-Fi, MQTT, alertes et
redémarrage.

**Proposition :** une table `action → fonction` et des fichiers par domaine
(`WsSettings.cpp`, `WsNetwork.cpp`, `WsAlerts.cpp`), en gardant l'enveloppe,
la déduplication des identifiants et l'envoi d'état dans `WebSocketApi.cpp`.

**Effort :** moyen, sans changement de protocole. **Intérêt :** lisibilité et
ajout de commandes plus sûr (prépare aussi l'écran déporté).

**Décision :** Fait. `WebSocketApi.cpp` garde l'enveloppe, la déduplication et l'état; commandes dans `WsSettingsCommands.cpp`, `WsAlertCommands.cpp`, `WsNetworkCommands.cpp` (tables action → fonction, protocole inchangé).

### R5 — Limites de mesure centralisées

**Constat :** les plages valides des mesures (−40 à 80 °C, 0 à 100 %) sont
recopiées dans `SerialLink`, `CaveController` et `Lvgl_Global`
(`HasValidControllerMeasurements`), avec des variantes de calcul de la valeur
corrigée.

**Proposition :** constantes dans `ConfigurationLimits.h` et une fonction
unique `GetCorrectedMeasurements()` dans `ControllerValidation`, utilisée par
la régulation, l'écran, le MQTT et le JSON.

**Effort :** faible. **Intérêt :** cohérence (une seule règle de validité).

**Décision :** Fait. Constantes `Min/MaxMeasured*` et `GetCorrectedMeasurements()` utilisées par la régulation, l'alarme, la liaison UART, les écrans et l'état JSON.

### R6 — Rafraîchissement de l'écran Ventilation

**Constat :** `Refresh_ScreenFan()` réapplique l'état des 6 boutons image
toutes les 500 ms; `lv_imgbtn_set_state()` invalide l'objet à chaque appel et
force un redessin même sans changement.

**Proposition :** ne mettre à jour l'état des boutons que s'il a changé (même
principe que `SetLabelTextIfChanged`).

**Effort :** faible. **Intérêt :** CPU et fluidité de l'écran.

**Décision :** Fait. `SetImgButtonEnabled()` ne change que l'état désactivé et seulement s'il change (l'ancien appel effaçait aussi un appui en cours).

## Question hors refactorisation

### Q1 — Puissance d'émission Wi-Fi

`ConnectToWifi()` fixe `WiFi.setTxPower(WIFI_POWER_8_5dBm)`, soit environ un
quart de la puissance par défaut. Le signal mesuré est d'environ −74 dBm.
Est-ce volontaire (alimentation limitée, baisses de tension) ? Sinon, une
valeur plus élevée améliorerait la stabilité du lien. Non modifié, car la
connexion Wi-Fi ne doit pas être changée sans accord.

**Décision :** Inchangé (la connexion Wi-Fi n'est pas modifiée sans demande explicite).

## Ordre recommandé

1. R0 (protection du compresseur).
2. R5 et R6 (rapides, sans risque).
3. R1 (gain mémoire transverse).
4. R2 + R3 ensemble (à tester avec Home Assistant).
5. R4 (purement structurel).
