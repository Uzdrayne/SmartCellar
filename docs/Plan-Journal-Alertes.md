# Plan — Journal des alertes (page Menu)

Ce document prépare l'ajout d'un journal des alertes horodatées, consultable et
effaçable depuis `Menu.html` via le WebSocket. Rien n'est codé tant que les
décisions ci-dessous ne sont pas prises.

## Existant

- `AlertManager` calcule chaque cycle un instantané `AlertSnapshot` : liaison
  Nano perdue, mesures indisponibles, température haute, mode secours du Nano,
  Wi-Fi déconnecté, MQTT déconnecté.
- Seule la dernière alerte « principale » est mémorisée (`LastPrimaryAlertReason`
  et `LastPrimaryAlertRaisedAtMs`, en `millis()`); elle sert à l'écran Warning
  (« Retour à la normale, dernière alerte : … »).
- Aucune heure de début ou de fin n'est conservée par type d'alerte, et rien
  n'est exposé au Web en dehors de l'état courant (`Alerts`).
- L'heure NTP est demandée à la connexion Wi-Fi (fuseau CET/CEST), mais peut
  être indisponible au démarrage ou en mode point d'accès.
- L'écran LVGL n'est pas concerné par cette évolution.

## Questions

Chaque question propose une recommandation; la décision est à compléter.

### Q1 — Que contient le journal ?

**Question :** Un historique des occurrences (une entrée par apparition d'une
alerte, avec début et fin) ou seulement l'état actuel de chaque type d'alerte ?

**Recommandation :** Un historique : une entrée par occurrence, avec le type,
l'heure d'apparition, l'heure de fin (ou « en cours ») et la durée. C'est ce qui
permet de voir « Wi-Fi perdu à 3 h 12 pendant 4 min » au réveil.

**Décision :** Oui, historique des occurrences avec début, fin et durée.

### Q2 — Taille et alertes répétées

**Question :** Combien d'entrées garder, et que faire d'une alerte qui clignote
(par exemple un Wi-Fi qui se coupe dix fois en une heure) ?

**Recommandation :** 20 entrées maximum en anneau (les plus anciennes sont
écrasées), environ 0,5 Ko de RAM. Si une alerte réapparaît moins de 5 minutes
après sa fin, on réutilise la même entrée et on incrémente un compteur
(« ×3 ») au lieu de remplir le journal.

**Décision :** **10 entrées** maximum en anneau; regroupement avec compteur si une alerte réapparaît moins de 5 minutes après sa fin.

### Q3 — Quels événements journaliser ?

**Question :** Uniquement les 6 alertes existantes, ou aussi d'autres
événements utiles ?

**Recommandation :** Les 6 alertes existantes, plus deux événements
ponctuels :
- **Redémarrage** avec sa cause (`esp_reset_reason()`) : alimentation,
  redémarrage demandé, plantage, chien de garde… Un plantage passe sinon
  inaperçu.
- **Échec de sauvegarde NVS** (consignes, réglages), déjà détecté dans le code.

Les erreurs UART ponctuelles (JSON invalide) ne sont pas journalisées une par
une : elles alimentent déjà « mesures indisponibles » si elles durent.

**Décision :** Oui : les 6 alertes, les redémarrages anormaux (plantage, chien de garde, baisse de tension) et les échecs de sauvegarde NVS.

### Q4 — Horodatage : `millis()`, heure NTP, ou les deux ?

**Question :** Comment dater une alerte quand l'heure NTP n'est pas encore
disponible ?

**Recommandation :** Toujours enregistrer `millis()` (fiable, monotone). L'heure
réelle est recalculée au moment de l'affichage : si l'heure NTP est connue
maintenant, heure de l'alerte = heure actuelle − (millis actuel − millis de
l'alerte). Une alerte survenue avant la synchronisation NTP obtient donc quand
même une heure dès que NTP répond. Sans NTP, on n'affiche que la durée écoulée.

**Décision :** Oui : `millis()` enregistré, heure réelle recalculée à l'affichage quand NTP est disponible.

### Q5 — Format d'affichage

**Question :** « Erreur X il y a 12 min » ou « Erreur X à 10 h 24 » ?

**Recommandation :** Les deux, l'heure en premier car elle reste vraie quand
on relit plus tard :
- `Wi-Fi indisponible — 10:24 (il y a 12 min) · durée 4 min`
- `Liaison Nano perdue — 10:41 · en cours depuis 3 min`
- Date ajoutée si ce n'est pas aujourd'hui : `hier 23:58` ou `08/11 23:58`.
- Sans NTP : `il y a 12 min · durée 4 min`.

**Décision :** Oui : heure puis durée écoulée; date si ce n'est pas aujourd'hui; durée seule sans NTP.

### Q6 — Que fait le bouton « Effacer » ?

**Question :** Efface-t-il tout, y compris une alerte encore en cours ?

**Recommandation :** Il efface les entrées terminées. Une alerte encore active
reste affichée : on ne peut pas « effacer » une condition qui dure, sinon elle
disparaîtrait alors que le problème persiste. Quand elle se termine, elle reste
dans le journal jusqu'au prochain effacement.

**Décision :** Oui : efface les entrées terminées, les alertes en cours restent.

### Q7 — Visibilité de la carte

**Question :** Quand afficher la carte « Alertes » sur `Menu.html` ?

**Recommandation :** Visible dès qu'il y a au moins une entrée (en cours ou
terminée), masquée sinon. Elle est placée en haut de page, avec une bordure
rouge si une alerte est en cours et orange si toutes sont terminées.

**Décision :** Oui : carte visible s'il y a au moins une entrée, rouge si une alerte est en cours, orange sinon.

### Q8 — Transport WebSocket

**Question :** Envoyer le journal dans l'état diffusé chaque seconde, ou via des
commandes dédiées ?

**Recommandation :** Commandes dédiées, pour ne pas alourdir l'état :
- `getAlertLog` : renvoie les entrées;
- `clearAlertLog` : efface les entrées terminées;
- l'état diffusé ne gagne qu'un petit champ `Alerts.LogRevision`, incrémenté à
  chaque changement du journal. La page relit le journal seulement quand ce
  numéro change. Les durées « il y a … » sont recalculées dans le navigateur à
  partir de `UptimeMilliseconds`, déjà présent dans l'état.

**Décision :** Oui : `getAlertLog`, `clearAlertLog` et `Alerts.LogRevision` dans l'état.

### Q9 — Persistance après redémarrage

**Question :** Le journal doit-il survivre à un redémarrage ou une coupure de
courant ?

**Recommandation :** Non pour cette première version : journal en RAM
seulement. L'entrée « Redémarrage (cause) » de Q3 indique qu'un redémarrage a
eu lieu. Écrire en NVS à chaque alerte userait la flash (une alerte Wi-Fi qui
clignote = beaucoup d'écritures). On pourra ajouter plus tard une sauvegarde
limitée (par exemple uniquement les alertes principales).

**Décision :** Oui : RAM seulement pour cette version.

### Q10 — Plusieurs clients et effacement

**Question :** Si deux navigateurs sont ouverts, l'effacement depuis l'un
s'applique-t-il aux deux ?

**Recommandation :** Oui : le journal est unique dans le contrôleur. Après un
effacement, `LogRevision` change et l'autre page se met à jour automatiquement.

**Décision :** Oui : journal unique, effacement partagé.

### Q11 — Écran LVGL

**Question :** L'effacement Web doit-il modifier l'écran Warning (texte
« Dernière alerte : … ») ?

**Recommandation :** Non : l'écran garde son fonctionnement actuel, indépendant
du journal. On pourra l'y relier plus tard si besoin.

**Décision :** L'écran s'appuie sur les fonctions communes du journal (liste des alertes actives, libellés, dernière alerte) pour éviter les doublons, sans changer ses textes, qui doivent continuer à tenir sur le petit écran. Effacer le journal efface donc aussi « Dernière alerte » à l'écran.

### Q12 — MQTT / Home Assistant

**Question :** Faut-il publier le journal vers Home Assistant ?

**Recommandation :** Non dans cette version : Home Assistant reçoit déjà
`DeviceAlert` et `AlertStatus` et conserve son propre historique.

**Décision :** Oui : rien de nouveau vers Home Assistant.

## Synthèse des décisions

À compléter une fois les questions tranchées.

## Implémentation

- `src/Alarm/AlertLog.h/.cpp` : module commun (types d'alerte, codes Web,
  libellés écran, liste des alertes actives, journal de 10 entrées protégé par
  un mutex). `AlertManager` l'alimente à chaque cycle; l'écran Warning,
  l'état JSON (`ActiveReasons`) et l'API WebSocket l'utilisent tous.
- Délai de grâce de 60 s au démarrage : une alerte n'est journalisée que si
  elle dure au-delà (évite les entrées Wi-Fi/MQTT à chaque boot).
- `LastChangeMs` : dernière activation pour une alerte en cours, heure de fin
  sinon; une alerte regroupée (×N) affiche donc la durée de sa dernière
  occurrence.
- Redémarrages journalisés : plantage, chiens de garde, baisse de tension.
  Mise sous tension, redémarrage logiciel ou téléversement ne sont pas des
  anomalies.
- État : `EpochSeconds` (ou `null` sans NTP) et `Alerts.LogRevision`.