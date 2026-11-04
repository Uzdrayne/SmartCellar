# Checklist de tests matériel — A4 et A5

**Date prévue :** 4 novembre 2026  
**Périmètre :** sécurité de l'interface Web (A5), alertes de température et de liaison (A4), échange C3/Nano.  
**Important :** cette checklist vérifie le comportement logiciel sur les cartes; elle ne valide pas les relais de puissance, le compresseur ni la sûreté de l'installation.

## Avant de commencer

- [ ] Flasher ensemble les versions C3 et Nano compilées pour cette modification.
- [ ] Prévoir l'accès au moniteur série du C3 et du Nano.
- [ ] Prévoir l'accès au broker et à Home Assistant.
- [ ] Noter les mots de passe actuels avant de tester les fonctions qui les modifient.
- [ ] Effectuer les essais de relais avec des sorties déconnectées ou des charges factices. Ne pas faire ces essais avec le compresseur, le chauffage ou les autres charges de puissance en fonctionnement.
- [ ] Si un câblage doit être modifié, mettre l'installation hors tension avant l'intervention.
- [ ] Utiliser un réseau local de confiance : l'authentification Basic passe par HTTP, qui ne chiffre pas les identifiants.

## 1. Démarrage et liaison C3/Nano

- [ ] Démarrer les deux cartes et vérifier qu'elles démarrent sans erreur dans leurs moniteurs série.
- [ ] Au démarrage, vérifier que le Nano envoie `config_request` avec `reason: "startup"`.
- [ ] Vérifier que cette demande normale de démarrage ne déclenche pas à elle seule une alerte « Mode secours ».
- [ ] Vérifier que le C3 envoie la configuration et que le Nano l'acquitte.
- [ ] Confirmer que les mesures valides et les états des sorties continuent d'arriver normalement.

### Perte et retour de liaison

Faire ces essais uniquement avec les sorties de puissance sécurisées.

- [ ] **Coupure C3 → Nano :** vérifier qu'après environ 4 secondes sans commande valide, le Nano passe en fallback.
- [ ] Vérifier que la requête du Nano indique `reason: "outputs_timeout"` et que l'alerte MQTT `DeviceAlert` devient active.
- [ ] Restaurer la liaison et vérifier que le C3 renvoie configuration et sorties.
- [ ] Vérifier que le Nano n'émet `control_restored` qu'après réception de la configuration et d'une commande `outputs` valide, puis que l'alerte de fallback s'efface.
- [ ] **Coupure Nano → C3 :** vérifier qu'après plus de 6 secondes sans télémétrie, le C3 signale les mesures/la liaison indisponibles et active `DeviceAlert`.
- [ ] Restaurer la liaison et vérifier le retour de la télémétrie, la mise à jour de `LastMeasurementAge` et l'effacement de l'alerte de liaison.
- [ ] Si possible, tester ensuite la perte complète de la liaison. Le Nano doit passer en fallback; le C3 doit aussi signaler l'absence de télémétrie.

## 2. Alertes A4 et configuration Web

Ouvrir la page **Alertes** depuis le menu Web.

- [ ] Vérifier qu'elle est protégée par l'authentification Web.
- [ ] Vérifier le seuil initial de **25 °C** et la plage de saisie **0 à 40 °C**.
- [ ] Enregistrer 25 °C, redémarrer le C3, puis vérifier que le seuil est toujours à 25 °C.
- [ ] Vérifier que le serveur refuse une valeur hors plage (par exemple `40.1` ou `-0.1`), une valeur non numérique (`NaN`) et une valeur absente. La configuration enregistrée ne doit pas être remplacée par une valeur invalide.
- [ ] Vérifier l'état normal lorsque les mesures sont fraîches et sous le seuil.
- [ ] Avec une mesure contrôlée et sans charge de puissance en fonctionnement, vérifier que l'alerte ne se déclenche pas avant 5 minutes au-dessus de 25 °C, puis devient active après environ 5 minutes continues.
- [ ] Vérifier le rétablissement après 30 secondes continues sous 25 °C. À exactement 25 °C, l'état ne devrait ni déclencher ni rétablir l'alerte.
- [ ] Si le seuil ne peut pas être dépassé de manière sûre ce soir, différer ce test précis plutôt que de chauffer l'installation ou les denrées.

### Perte de mesure

- [ ] Provoquer une perte de mesures DHT22 uniquement dans des conditions sûres. Vérifier que le Nano arrête la régulation selon son comportement de sécurité.
- [ ] Après plus de 6 secondes sans télémétrie valide, vérifier que le C3 active `DeviceAlert` avec un motif général de mesures/liaison indisponibles.
- [ ] Vérifier que la dernière température valide reste disponible et que `LastMeasurementAge` augmente.
- [ ] Restaurer les mesures et vérifier que le motif d'indisponibilité disparaît.

## 3. Home Assistant / MQTT

- [ ] Vérifier la découverte des entités `DeviceAlert`, `AlertStatus`, `LastValidTemperature` et `LastMeasurementAge`.
- [ ] Vérifier que `DeviceAlert` est OFF en état normal et ON lors d'une alerte température, de mesures indisponibles ou du fallback Nano.
- [ ] Vérifier que `AlertStatus` indique les motifs présents; lors de motifs simultanés, vérifier qu'ils ne se masquent pas entre eux.
- [ ] Vérifier que la température valide et son ancienneté sont cohérentes avec le moniteur série.
- [ ] Redémarrer le broker ou interrompre la connexion MQTT : Home Assistant doit signaler l'appareil indisponible. Au rétablissement, les états doivent être republiés.
- [ ] Garder en tête qu'aucune alerte MQTT ne peut être livrée à Home Assistant lorsque le C3, le Wi-Fi ou le broker est hors service.

## 4. Sécurité A5 de l'interface Web

### Point d'accès et création du compte

- [ ] Vérifier que le mot de passe AP est différent de l'ancien mot de passe fixe `123456789`, comporte 12 caractères et est affiché sur l'écran Wi-Fi.
- [ ] Redémarrer le C3 et vérifier que le mot de passe AP reste identique.
- [ ] Lors de la première configuration Web, vérifier que le compte `admin` refuse un mot de passe de moins de 12 caractères et deux mots de passe différents.
- [ ] Créer le compte puis vérifier que les pages Web, `/GlobalStatus`, `/AlarmSettings` et le WebSocket demandent l'authentification.
- [ ] Vérifier qu'un mot de passe erroné n'ouvre pas les pages ni les API protégées.

### Secrets MQTT et requêtes Web

- [ ] Ouvrir `/GlobalStatus` et vérifier que la réponse JSON ne contient pas `MQTTPASSWORD` ni le mot de passe MQTT.
- [ ] Ouvrir la page MQTT : le champ du mot de passe doit rester vide, même si un mot de passe est enregistré.
- [ ] Avec un broker de test ou une configuration dont le secret peut être rétabli, enregistrer les autres paramètres MQTT avec le champ mot de passe vide; vérifier que le secret existant est conservé.
- [ ] Vérifier l'effacement explicite du mot de passe uniquement lorsque la case d'effacement est cochée. Restaurer ensuite les paramètres requis pour MQTT.
- [ ] Lancer un scan Wi-Fi depuis la page : il doit fonctionner en POST et afficher les erreurs éventuelles.
- [ ] Vérifier qu'un GET direct vers `/Scan` ne lance pas un scan.
- [ ] Vérifier que le redémarrage depuis la page fonctionne en POST et qu'un GET direct vers `/RestartESP` ne redémarre pas l'appareil.
- [ ] Vérifier que le WebSocket continue d'actualiser l'état après authentification.
- [ ] Si possible, vérifier qu'une requête POST provenant d'une autre origine est refusée.

### Récupération du mot de passe Web

- [ ] Depuis l'écran Wi-Fi, tester la double confirmation physique dans le délai prévu.
- [ ] Vérifier qu'une seule activation ou une double confirmation hors délai ne réinitialise pas le compte.
- [ ] Après réinitialisation réussie, vérifier que le compte Web peut être recréé.
- [ ] Vérifier que cette opération ne modifie ni le mot de passe AP ni les paramètres Wi-Fi/MQTT.

## 5. À ne pas considérer comme validé ce soir

- [ ] La nouvelle page d'alerte **LVGL** n'est pas encore intégrée au projet : son affichage n'est pas à tester.
- [ ] La maquette du mode secours, du motif de sonde et de la dernière mesure n'est pas encore validée sur l'écran.
- [ ] Ces alertes C3/écran/MQTT ne constituent pas une alarme indépendante : elles dépendent du fonctionnement du C3 et, pour Home Assistant, du réseau et du broker.
- [ ] Ne pas conclure sur la commutation réelle des contacts, les délais exigés par le fabricant du compresseur ou la sûreté complète de l'installation à partir de ces seuls essais.

## Résultats à noter

| Test | Résultat (OK / échec / reporté) | Observation |
|---|---|---|
| Démarrage C3/Nano et `reason: startup` |  |  |
| Entrée en fallback et `outputs_timeout` |  |  |
| Retour C3 et `control_restored` |  |  |
| Alerte température haute après 5 min |  |  |
| Retour normal après 30 s sous 25 °C |  |  |
| Perte de télémétrie et âge de la dernière mesure |  |  |
| Entités et états Home Assistant |  |  |
| Authentification Web / WebSocket |  |  |
| Secret MQTT non exposé et conservation/effacement |  |  |
| Mot de passe AP et récupération du compte Web |  |  |
