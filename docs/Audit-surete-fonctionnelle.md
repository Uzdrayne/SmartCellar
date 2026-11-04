# Audit logiciel fonctionnel et sûreté — SmartCellar

**Date :** 4 novembre 2026
**Périmètre :** revue statique du firmware ESP32-C3 présent dans ce dépôt et du firmware compagnon Arduino Nano du projet séparé `SmartController_NanoGPIO`, couvrant la régulation, la liaison série, l'interface locale, le Web et MQTT.

> Cet audit n'est ni une certification ni une validation du système frigorifique complet. Le firmware Nano a été relu statiquement et compilé, mais n'a pas été testé sur carte. Le câblage, les relais/contacteurs, les protections électriques, le compresseur, les capteurs et les conditions réelles de conservation ne sont pas validés. Aucun essai de défaut sur matériel n'a été exécuté.

## Avis général

La régulation est une commande tout-ou-rien avec hystérésis, délais minimums et interverrouillage entre froid/chauffage et déshumidification/humidification. La validation logicielle des paramètres et le fallback autonome du Nano sont maintenant implémentés; les deux firmwares compilent. Cela ne démontre toutefois ni la bonne commutation électrique ni le comportement en panne sur le matériel réel. Je ne peux donc pas conclure que le système protège de façon fiable les denrées : les seuils restent à confronter au procédé, le fallback n'a pas été éprouvé sur carte et il n'y a pas d'alarme indépendante de température.

**Je déconseille de laisser de la viande sans surveillance en comptant uniquement sur ce firmware** tant que les points prioritaires ci-dessous n'ont pas été corrigés et les défauts testés avec le matériel complet. Une interface Web, MQTT ou un microcontrôleur ne remplace pas un thermostat de sécurité et une alarme/sonde indépendante adaptés à l'installation et aux exigences sanitaires applicables.

## Comportement observé dans le code

### Algorithmes et valeurs par défaut

Les transitions se font selon les règles suivantes :

- **Froid :** démarrage si la température corrigée dépasse `consigne + 0,8 °C`; arrêt si elle descend sous `consigne + 0,1 °C`.
- **Chauffage :** démarrage sous `consigne - 0,8 °C`; arrêt à `consigne - 0,1 °C` ou au-dessus.
- **Déshumidification :** démarrage au-dessus de `consigne + 1,5 %`; arrêt sous `consigne + 0,5 %`.
- **Humidification :** démarrage sous `consigne - 1,5 %`; arrêt à `consigne - 0,5 %` ou au-dessus.
- Les délais minimums de marche et d'arrêt sont de **60 secondes** par défaut.
- Le ventilateur est activé par défaut et fonctionne par cycles de **5 minutes en marche / 55 minutes à l'arrêt**. Il continue son cycle même si les mesures sont invalides.

Les tests de seuil utilisent des comparaisons strictes pour les démarrages et certaines coupures : à l'égalité, la sortie conserve son état précédent. Les valeurs des délais et seuils doivent être confrontées à la fiche technique du compresseur et aux besoins réels de la cave. Une minute d'arrêt minimum ne peut pas être déclarée suffisante sans cette vérification.

Les limites logicielles des paramètres sont maintenant : consigne **5–30 °C** et **40–100 % HR**, offsets **±5 °C** et **±10 points HR**, hystérésis directionnelles strictement ordonnées et bornées à **5 °C** / **10 points HR**, délais min ON/OFF **5–1800 secondes**. Le minimum de 5 secondes est une borne logicielle demandée pour cette version, pas une recommandation constructeur pour le groupe froid.

### Perte de la liaison série / du Nano

`SerialLink.cpp` valide la forme JSON, les plages physiques des mesures et les cinq booléens de retour GPIO. Au démarrage, les mesures sont invalides, donc la régulation des quatre relais climatiques reste arrêtée jusqu'à la réception d'une télémétrie valide.

Après plus de **6 secondes sans télémétrie valide**, le C3 invalide température et humidité. Au tour de boucle suivant, il met froid, chauffage, déshumidification et humidification à l'arrêt, puis tente d'envoyer les sorties au Nano. Le ventilateur, lui, reste commandé par son cycle.

La régulation vérifie également que les mesures corrigées sont finies et restent dans les plages physiques **-40 à 80 °C** et **0 à 100 % HR**. Une mesure corrigée invalide ou une configuration de régulation invalide entraîne la mise à l'arrêt des quatre relais climatiques; le ventilateur conserve son cycle. Si une sortie était active, le moment de coupure relance aussi son délai minimum d'arrêt.

Le Nano compagnon du projet `SmartController_NanoGPIO` détecte l'absence de commande `outputs` valide après **4 secondes** et passe alors en fallback. Il conserve les états physiques des relais au basculement et réinitialise les minuteries des quatre relais climatiques. En fallback, il utilise une configuration complète, versionnée, validée et sauvegardée en EEPROM avec CRC et vérification de lecture; il la demande au C3 au démarrage et lors du basculement. Sans configuration valide, toutes les sorties restent OFF. Les mesures DHT22 locales sont échantillonnées toutes les 2 secondes et considérées périmées après **6 secondes**; une mesure invalide ou périmée entraîne une demande d'arrêt, en respectant le délai minimum ON des relais climatiques. La LED D10 signale le fallback.

Le retour au contrôle C3 n'a lieu qu'après réception d'une configuration valide et de commandes `outputs` valides. C'est une reprise conservatrice qui permet au Nano d'avoir la configuration courante avant une éventuelle nouvelle perte de liaison. Si la liaison reste coupée, le Nano continue avec sa configuration EEPROM valide; sans configuration valide, il reste en sécurité avec les sorties à l'arrêt. Ce comportement est cohérent à la lecture du code, mais n'a pas été testé sur carte. La configuration des relais Nano est active-HIGH (`RelayActiveLow=false`), valeur confirmée par l'utilisateur; la commutation réelle des contacts et le câblage restent à éprouver.

Le stockage EEPROM utilise un seul enregistrement : son CRC détecte une écriture incomplète/corrompue, mais une coupure d'alimentation pendant une mise à jour peut invalider l'ancienne copie. Dans ce cas, au redémarrage, le Nano reste à l'arrêt jusqu'à une nouvelle synchronisation avec le C3. Le retour `gpio` représente des niveaux logiques lus par le compagnon, pas la position réelle des contacts du relais; il n'est pas utilisé pour arrêter la régulation ni déclencher une alarme.

### Perte du Wi-Fi ou de MQTT

La boucle de régulation est exécutée indépendamment du Wi-Fi et de MQTT. Une coupure réseau n'arrête donc pas directement le contrôle local. Depuis les changements du 3 novembre 2026, `main.cpp` ne force plus de changement d'écran lorsque le Wi-Fi tombe, expire ou revient : la page choisie par l'utilisateur reste affichée. L'écran global distingue maintenant `STA OK`, `Mode AP` et `Sans reseau`. La page Wi-Fi reste accessible par la navigation.

L'état MQTT est remis à faux lors des événements Wi-Fi de déconnexion STA et d'arrêt/démarrage AP. Cela corrige l'état mémorisé, mais ne rend pas le statut consultable si aucun réseau n'est disponible; il redevient visible dès que le Web est accessible en AP ou après reconnexion. La boucle MQTT reste indépendante de la régulation.

### Affichage en cas de perte série ou de mesures invalides

L'écran global affiche **« Capteur indisponible »** et remplace les mesures par `--`; les consignes restent visibles. Il montre désormais les états commandés par le C3 même en défaut, plutôt que de présenter systématiquement les relais à l'arrêt.

Depuis le 3 novembre 2026, Temp/Hum/Fan ont aussi un bandeau rouge **« MESURES INDISPONIBLES »** rafraîchi périodiquement. Temp/Hum masquent leur mesure courante par `--`; leurs arcs sont désactivés et le switch revient verrouillé tant que les mesures corrigées ne sont pas valides. Au retour de mesures valides, le bandeau disparaît, mais l'arc reste verrouillé jusqu'au déverrouillage de l'utilisateur. L'interface distingue toujours une absence de mesures valides d'une panne précise du Nano, du capteur ou du câble; elle n'affiche pas l'âge exact de la dernière télémétrie.

## Constats classés par priorité

| ID | Priorité / état | Constat et conséquence |
|---|---|---|
| A1 | **Corrigée (à valider sur cible)** | **Validation des paramètres de régulation.** Les chemins Web analysent strictement les nombres, refusent les valeurs non finies, hors plage ou incohérentes et répondent HTTP 400 sans modifier ni sauvegarder les réglages. Les consignes MQTT sont contrôlées avec les mêmes bornes. Au démarrage, les valeurs NVS invalides sont remplacées et sauvegardées avec des valeurs par défaut sûres; les erreurs d'écriture sont journalisées et signalées en HTTP 500 lors d'une sauvegarde Web. Les limites restent des hypothèses logicielles à confirmer pour le procédé et l'équipement réels. |
| A2 | **Corrigée (à valider sur cible)** | **Mesures corrigées invalides.** Le contrôleur exige des valeurs corrigées finies et physiquement plausibles avant toute comparaison. Une mesure corrigée invalide ou une configuration invalide arrête les quatre relais climatiques et identifie la cause dans les messages de diagnostic; les délais minimums d'arrêt repartent à la coupure. |
| A3 | **Élevée — fallback implémenté, matériel non validé** | **Mise en sécurité du Nano.** Le timeout de 4 s, le fallback avec configuration EEPROM, l'arrêt sans configuration valide, le contrôle de fraîcheur du DHT22 et la reprise après configuration + `outputs` valides sont présents dans le firmware Nano. Les sorties sont mises OFF à l'initialisation du Nano; la configuration active-HIGH est confirmée par l'utilisateur. Restent à tester sur carte les resets, la coupure de chaque sens UART, les contacts de relais, la LED D10 et les scénarios de perte/reprise. |
| A4 | **Élevée** | **Aucune alarme indépendante de température élevée / perte capteur.** Une panne de capteur entraîne bien l'arrêt demandé des quatre relais climatiques, mais peut laisser la cave se réchauffer sans alarme locale persistante ni système indépendant de surveillance. Le journal série ou Home Assistant ne suffit pas en cas de coupure réseau ou d'absence de personne. |
| A5 | **Élevée** | **Commandes réseau non authentifiées et secret MQTT exposé.** Les routes de modification/redémarrage n'ont pas de contrôle d'accès visible et utilisent HTTP. Le mot de passe MQTT est inclus dans `JSON_GLOBAL_STATUS`, renvoyé par `/GlobalStatus` et diffusé par WebSocket. En outre, le mot de passe du point d'accès est défini en clair dans le firmware. Un appareil ayant accès au réseau local/à l'AP peut lire des secrets ou modifier des paramètres de contrôle. |
| A6 | **Corrigée (à valider sur cible)** | **Hystérésis et délais bornés.** Les hystérésis froid/chauffage et déshumidification/humidification doivent respecter leur signe, leur ordre, leur amplitude maximale et une zone morte entre fonctions opposées. Les délais min ON/OFF sont bornés à 5–1800 secondes. Ces limites empêchent les configurations contradictoires évidentes, mais leurs valeurs doivent encore être comparées aux exigences du matériel. |
| A7 | **Corrigée en partie** | **Indication réseau et défaut de mesure.** La perte Wi-Fi ne force plus la page de configuration; l'écran global indique STA/AP/absence de réseau. Temp/Hum/Fan affichent le défaut de mesure, masquent les valeurs courantes et désactivent l'arc. Il manque toujours l'âge de la dernière télémétrie et le diagnostic précis de l'origine du défaut. |
| A8 | **Moyenne** | **Retour des sorties uniquement informatif et non exploité.** `RelayFeedbackValid` et les niveaux GPIO sont inclus dans le JSON d'état, mais aucune comparaison commande/retour ne signale un relais qui ne suit pas la consigne. Même une comparaison GPIO ne prouverait pas la commutation des contacts de puissance. |
| A9 | **Moyenne** | **Buffer JSON partagé entre tâches sans verrou du côté rédacteur.** `Get_Json_GlobalStatus()` écrit le buffer global tandis que les handlers HTTP/WebSocket le lisent sous mutex; le rédacteur ne prend pas ce mutex. Une réponse peut donc être lue pendant sa mise à jour. Si le buffer fixe de 2000 octets est trop petit, le code signale une troncature mais le JSON partiel peut tout de même être envoyé. |
| A10 | **Moyenne** | **Aucune suite de tests automatisés de régulation dans `test/`.** La compilation valide le firmware, pas les transitions aux seuils, délais, interverrouillages ni les scénarios de panne. Le dossier `test/` ne contient qu'un README. |
| A11 | **Corrigée** | **État MQTT périmé après perte du Wi-Fi.** `MqttConnected` est maintenant remis à faux sur les événements de déconnexion STA et de changement/arrêt AP. |
| A12 | **Moyenne — marge à mesurer sur cible** | **Marge RAM du Nano.** Après déplacement des clés JSON constantes en mémoire flash, le build utilise 1 254/2 048 octets de RAM statique (61,2 %), avec le tampon JSON porté à 768 octets. Il reste environ 794 octets pour la pile; le build ne mesure pas le pic réel lors du parsing, de l'accès EEPROM et des interruptions. |
| A13 | **Corrigée par agrandissement — à confirmer sur cible** | **Taille insuffisante des trames de configuration.** Les buffers C3/Nano de 512 octets ne pouvaient pas contenir une trame compacte représentative des 25 paramètres et métadonnées; le C3 aurait annulé l'envoi et le Nano aurait rejeté une ligne trop longue. Les deux capacités sont maintenant de 768 octets. Un test de contrat vérifie la correspondance des champs et une borne conservatrice de 721 octets; l'échange réel reste à valider sur UART. |

Les références principales sont `src/Controller/CaveController.cpp`, `src/SerialLink/SerialLink.cpp`, `src/Data/ControllerData.cpp`, `src/ServerWeb/WebHandlers.cpp`, `src/Wifi/my_wifi.cpp`, `src/WifiAndMqtt/my_MQTT.cpp`, `src/main.cpp`, ainsi que `SmartController_NanoGPIO/src/NanoController.cpp` et `SmartController_NanoGPIO/src/NanoSerialLink.cpp`.

## Points positifs observés

- Les trames série ont une longueur maximale, les JSON invalides sont rejetés et les températures/humidités brutes sont contrôlées dans des plages plausibles.
- Les données sont invalidées après un timeout et la perte des mesures conduit, lorsque le C3 fonctionne, à demander l'arrêt des quatre relais climatiques.
- Les réglages de durée du ventilateur ont des limites et sont validés dans les chemins Web et MQTT.
- Les paramètres climatiques sont maintenant validés sur le serveur Web, à la réception MQTT des consignes et au chargement depuis NVS; le navigateur affiche également les bornes attendues.
- Les régulations froid/chauffage et déshumidification/humidification disposent d'hystérésis, de délais minimums et d'un interverrouillage vérifié par la validation.
- Le Nano dispose maintenant de modules séparés pour le contrôleur et la liaison série; les clés des 25 paramètres envoyés par le C3 sont prises en charge côté Nano.
- Les configurations Nano sont validées, acquittées/refusées explicitement et sauvegardées avec version et CRC; les deux projets compilent.
- Un test logiciel vérifie la correspondance des clés C3/Nano et la capacité des buffers pour une borne conservatrice du message.
- Le code de contrôle local continue à tourner sans MQTT et sans accès Web.

Ces points réduisent certains risques mais ne prouvent pas le comportement de la chaîne électrique complète.

## Plan recommandé avant toute mise en service sans surveillance

1. **Valider A1/A2 sur cible.** Tester les bornes exactes, les valeurs hors plage, `NaN`/`Inf`, le texte avec suffixe, les hystérésis contradictoires et une valeur NVS invalide. Confirmer le HTTP 400 sans mutation, le rejet MQTT journalisé, la réparation NVS, l'arrêt des quatre relais climatiques sur mesure corrigée invalide et le délai minimum d'arrêt après retour des mesures.
2. **Valider le fallback du Nano sur banc.** Tester la synchronisation initiale, l'EEPROM valide/invalide, une coupure UART dans chaque sens, la perte et le retour du C3, les resets des deux cartes et une interruption pendant l'écriture EEPROM. Vérifier les délais, la LED D10 et les contacts réels des relais, pas seulement leur GPIO. Évaluer le pic de pile; réduire l'usage SRAM ou mesurer une marge suffisante avant déploiement.
3. **Vérifier les délais avec le fabricant du compresseur.** Les délais logiciels doivent correspondre à ses exigences de démarrage/arrêt. Conserver une protection matérielle indépendante contre les conditions dangereuses.
4. **Ajouter une alarme indépendante.** Une sonde/alarme de température avec alimentation et notification adaptées, et une indication locale persistante pour capteur ou liaison en défaut. Définir les seuils et délais selon le procédé et les exigences sanitaires applicables.
5. **Sécuriser le réseau.** Retirer le mot de passe MQTT du JSON public, ajouter authentification et protection contre les requêtes non autorisées/CSRF aux commandes, et revoir le mot de passe AP et les secrets stockés/transmis.
6. **Automatiser les tests de la machine d'état.** Couvrir les seuils exacts, hystérésis, min ON/OFF, interverrouillages, capteur invalide, timeout série, valeurs NaN/Inf, valeurs hors plage, reset et débordement de `millis()`. Faire ensuite un essai sur banc avec charges factices avant le groupe froid.

## Scénarios de validation à consigner

- Démarrage sans Nano, puis connexion tardive; télémétrie JSON malformée, hors plage, partielle ou interrompue.
- Démarrage Nano sans configuration EEPROM, puis synchronisation avec le C3; configuration invalide, NACK, échec de sauvegarde et vérification du comportement sans réponse.
- Perte de RX, perte de TX, arrêt/reset du Nano, reset/blocage du C3, reprise avec configuration et `outputs`, et vérification physique de chaque sortie.
- Perte puis retour du Wi-Fi et du broker MQTT; confirmer que la régulation continue, que les états affichés redeviennent exacts et qu'aucune consigne ne change de façon inattendue.
- Valeurs manquantes, texte invalide, `NaN`, `Inf`, valeurs extrêmes et hystérésis contradictoires via Web, MQTT et NVS.
- Vérification sur banc des seuils de démarrage/arrêt, de la durée minimale d'arrêt du compresseur et de l'interverrouillage entre charges.

**Conclusion :** les deux firmwares compilent et les fonctions logicielles de validation et de fallback sont présentes, mais la sûreté globale n'est pas démontrée sans essais sur matériel. Les priorités restent la validation du fallback et de la marge RAM du Nano, la confirmation des délais avec le fabricant, et une surveillance/alarme indépendante de la température.
