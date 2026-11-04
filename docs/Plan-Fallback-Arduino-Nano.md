# Plan : régulation de secours sur Arduino Nano

## Objectif

Permettre au Nano de continuer la régulation si la liaison série avec l'ESP32-C3 est perdue. En fonctionnement normal, l'ESP32 reste maître. Le Nano devient maître des relais uniquement après avoir détecté une perte de liaison et si ses paramètres et ses mesures sont valides.

Ce document décrit le protocole et le comportement de secours implémentés dans les deux firmwares. Les essais sur matériel restent à effectuer.

## Périmètre et organisation

Le C3 est dans `SmartCellar`; le firmware Nano compagnon est dans `SmartController_NanoGPIO`.

La logique Nano est séparée de `main.cpp` : `NanoController.h/.cpp` gère les relais, la régulation, la configuration et sa persistance; `NanoSerialLink.h/.cpp` gère les trames UART.

## État actuel

- La liaison est bidirectionnelle, en UART 9600 bauds, 8N1, avec des objets JSON terminés par `\n`.
- Le Nano transmet les mesures de température et d'humidité ainsi que les niveaux logiques des relais.
- Le C3 transmet les états souhaités des cinq relais environ chaque seconde, et lors d'un changement.
- En mode normal, le C3 calcule la régulation et envoie les états souhaités.
- Le Nano exécute la régulation locale après quatre secondes sans commande `outputs` valide, avec sa dernière configuration valide et ses mesures locales.
- La sortie D10 `errorLed` indique le mode fallback.

## Paramètres à transmettre

Envoyer les paramètres nécessaires au calcul de la régulation :

- Consignes : `SetTemp`, `SetHum`.
- Corrections de mesure : `TempOffsetEnabled`, `TempOffset`, `HumOffsetEnabled`, `HumOffset`.
- Froid : `ColdHysHigh`, `ColdHysLow`, `ColdDelayMinOn`, `ColdDelayMinOff`.
- Chauffage : `HeatHysHigh`, `HeatHysLow`, `HeatDelayMinOn`, `HeatDelayMinOff`.
- Déshumidification : `DehumHysHigh`, `DehumHysLow`, `DehumDelayMinOn`, `DehumDelayMinOff`.
- Humidification : `HumHysHigh`, `HumHysLow`, `HumDelayMinOn`, `HumDelayMinOff`.
- Ventilation : `FanEnabled`, `FanOnMinutes`, `FanOffMinutes`.

Ne pas transmettre la structure `StrctDataController` brute. Elle contient également des mesures déjà acquises par le Nano, des états d'exécution, des retours GPIO et des messages de debug. Ces éléments ont d'autres rôles et certains dépendent de la carte.

## Quand envoyer la configuration

Envoyer un **instantané complet** des paramètres, plutôt que des messages contenant seulement le paramètre modifié. Cela permet au Nano de remplacer sa configuration d'un seul bloc et évite qu'il conserve un mélange d'anciennes et de nouvelles valeurs.

L'ESP32 envoie cet instantané :

1. Au démarrage, après le chargement et la validation de ses paramètres.
2. À chaque modification acceptée d'un paramètre de régulation, quelle que soit son origine (interface web, MQTT ou écran).
3. À la demande du Nano, avec une trame `{"type":"config_request"}`, notamment après un redémarrage ou une reconnexion. Si les paramètres n'ont pas changé, le C3 renvoie le même numéro de révision.

Les modifications rapprochées peuvent être regroupées en un seul envoi, une fois la mise à jour des paramètres terminée. À 9600 bauds, un instantané JSON occasionnel est raisonnable; il n'est pas nécessaire de retransmettre tous les paramètres chaque seconde.

Chaque instantané porte une version de schéma et un numéro de révision. Le Nano l'applique seulement après analyse, validation et sauvegarde en mémoire non volatile, puis répond par `config_ack`. Il répond par `config_nack` en cas de configuration invalide ou d'échec de persistance. Le C3 réessaie la dernière révision non acquittée à intervalle limité et renvoie immédiatement un instantané à la réception de `config_request`.

Le C3 détecte automatiquement les changements en comparant l'instantané courant à celui envoyé précédemment à chaque traitement de la liaison série. Il n'est donc pas nécessaire d'ajouter un signal spécifique dans chaque route Web ou chemin MQTT; toute modification effective des champs de `DataController` est détectée par cette comparaison.

La capacité des buffers de configuration est de 768 octets des deux côtés. Le test de contrat du protocole vérifie une borne conservatrice de 721 octets pour les clés et types actuels; toute modification du schéma doit réévaluer cette borne avant de réduire ces buffers.

## Heartbeat et détection de perte de liaison

Séparer la configuration, envoyée à l'événement, du signal de vie, envoyé régulièrement :

- Les messages `outputs` actuels sont rafraîchis environ chaque seconde; ils peuvent servir de heartbeat du C3 tant qu'ils restent périodiques et valides.
- Le Nano mémorise l'heure du dernier message valide du C3.
- Après expiration du délai de communication, le Nano passe en mode autonome. Sans configuration valide, toutes les sorties restent OFF; avec une configuration valide mais une mesure invalide ou périmée, il demande l'arrêt des relais en respectant le délai minimum ON.
- Le délai de perte de liaison retenu est de quatre secondes sans commande valide du C3.

Le Nano doit ignorer les anciennes commandes de relais une fois qu'il a pris la main. Le retour de la liaison déclenche une reprise automatique du C3 uniquement après réception d'une configuration valide et de commandes `outputs` valides.

La sortie LED `errorLed` du Nano, sur D10, doit être allumée en mode fallback et éteinte lorsque le C3 reprend le contrôle.

## Fonctionnement autonome du Nano

Le Nano applique les mêmes règles de régulation que le C3 pour :

- les consignes, offsets et hystérésis;
- les temporisations minimales ON/OFF et les interverrouillages froid/chauffage et humidification/déshumidification;
- le cycle de ventilation.

Au basculement, le Nano conserve les états physiques actuels des relais, mais initialise les minuteries de régulation à zéro, comme au démarrage du C3. Chaque changement d'état demandé ensuite doit respecter le délai minimum ON ou OFF correspondant.

Les mesures du Nano ont leur propre contrôle de validité et de fraîcheur. Si une mesure devient invalide ou trop ancienne, le Nano demande l'arrêt des relais tout en respectant le délai minimum ON avant d'éteindre un relais actuellement actif.

## Persistance

La dernière configuration valide est sauvegardée en mémoire non volatile du Nano pour permettre le fallback après un redémarrage ou une panne du C3. Elle n'est mise à jour qu'à la réception d'une nouvelle configuration valide, jamais à chaque heartbeat. Si aucune configuration valide n'est disponible, toutes les sorties restent OFF.

## Décisions retenues

- Timeout de perte de liaison : 4 s sans commande `outputs` valide.
- Reprise automatique du C3 après réception d'une configuration et de commandes valides.
- Sortie `errorLed` allumée pendant le fallback.
- Configuration conservée en mémoire non volatile; si aucune copie valide n'existe, toutes les sorties sont OFF.
- Mesure invalide/périmée : demander l'arrêt, en respectant le délai minimum ON du relais actif.
- Au début du fallback, conserver les états actuels des relais et repartir avec les minuteries à zéro; toute transition respecte ensuite les délais minimum.

## Vérifications matérielles restantes

- Le seuil de péremption des mesures locales est de 6 s depuis la dernière mesure DHT22 valide.
- Commutation physique des relais/contacteurs et signification des niveaux GPIO retournés par le Nano. Le firmware est configuré en actif-HIGH, réglage confirmé par l'utilisateur; la validation sur banc reste à faire.
- Câblage de la LED D10 avec résistance et polarité active.

## Validation restante

- Les builds PlatformIO des deux firmwares réussissent. Le test logiciel de contrat du protocole est disponible dans `test/test_nano_protocol_contract.py`.
- Sur matériel, tester la synchronisation initiale, les changements de configuration, la perte et le retour UART, les redémarrages, les trames invalides, la péremption DHT22 et les temporisations relais.
- Ne connecter les charges réelles qu'après vérification de la polarité des relais et des états sûrs.
