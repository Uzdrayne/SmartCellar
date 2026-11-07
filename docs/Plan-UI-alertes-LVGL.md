# Plan UI — alertes A4 sur l'écran LVGL

**État :** première implémentation LVGL réalisée — validation sur cible restante.

## Objectif et périmètre

Afficher sur l'écran du C3 l'état des alertes A4 déjà calculé par le firmware, en complément de Home Assistant/MQTT.

- L'affichage concerne les alertes de température élevée, de mesures indisponibles et de fallback du Nano.
- Les états Wi-Fi et MQTT sont affichés en complément lorsqu'ils sont indisponibles, sans provoquer seuls l'ouverture automatique de la page.
- Le seuil haut est configurable dans la page Web **Alertes** (25 °C par défaut, plage 0–40 °C). Les temporisations restent fixes : 5 minutes au-dessus du seuil et 30 secondes sous le seuil pour le retour à la normale.
- Il n'y a pas de GPIO disponible pour ajouter une LED ou un buzzer. La solution prévue est uniquement l'écran et MQTT.
- L'écran ne remplace pas une alarme indépendante : il dépend du C3 et de son écran.
- Les changements d'interface ne doivent pas modifier la régulation ni les commandes de relais.

## Comportements à représenter

L'interface devra refléter l'état produit par la logique d'alarme existante, plutôt que recalculer les seuils ou temporisations dans LVGL.

| État du système | Comportement attendu |
|---|---|
| Aucune alerte | Présentation habituelle, sans alerte active. |
| Température au-dessus du seuil pendant la durée requise | Signaler une alerte température; la maintenir jusqu'au retour sous le seuil pendant la durée prévue. |
| Mesures valides indisponibles | Signaler que les mesures ne sont plus disponibles. Ne pas présenter une température périmée comme valeur courante. |
| Fallback Nano | Signaler le mode fallback, distinctement si l'état fourni par le firmware permet de le distinguer des mesures indisponibles. |
| Retour à la normale | Retirer ou actualiser l'indication lorsque l'état fourni par le firmware est revenu à la normale. |
| Plusieurs causes simultanées | Afficher jusqu'à trois motifs, par priorité : liaison/mesures Nano, température, fallback, puis Wi-Fi/MQTT. |

Une alerte principale (Nano/mesures, température ou fallback) ouvre immédiatement la page Warning. L'utilisateur peut ensuite naviguer ailleurs; si une alerte principale est toujours active après 60 secondes sans toucher l'écran, Warning revient au premier plan. La résolution d'une alerte ne force pas de navigation : si Warning est affichée, elle indique le retour à la normale et y reste.

L'icône d'avertissement clignote tant qu'au moins une alerte affichée est active. La page réutilise uniquement les objets générés dans SquareLine; aucun objet ou événement LVGL n'est créé dynamiquement par son code de rafraîchissement.

Si plusieurs motifs sont actifs en même temps, l'interface doit les représenter sans en masquer silencieusement. Réutiliser les libellés et états exposés par le firmware autant que possible; ne pas inférer une cause précise (panne de sonde, Nano ou câble) si le logiciel ne peut pas la distinguer.

## Décisions visuelles à prendre après réception de la maquette

- Emplacement de l'alerte : bandeau, icône persistante, page dédiée ou combinaison de ces éléments.
- Hiérarchie visuelle lorsque plusieurs alertes sont actives.
- Texte exact, icônes, couleurs, taille et contraste, notamment en mode AP ou sans réseau.
- Affichage éventuel de la dernière température valide et de son ancienneté, en la distinguant clairement d'une mesure courante.
- Comportement lors de la navigation entre les pages et lors du retour au démarrage.
- Interaction éventuelle pour acquitter/masquer une indication, sans effacer l'état réel de l'alerte.

Ces décisions ne sont pas figées ici : elles doivent suivre la maquette de l'écran et les contraintes de lisibilité du matériel.

## Principes d'implémentation à préserver

- Lire l'état d'alerte existant; ne pas dupliquer dans LVGL la logique de seuil, les minuteries ou la détection de fraîcheur.
- Garder l'affichage réactif et non bloquant; ne pas ajouter d'attente longue dans la boucle de l'interface.
- Ne pas forcer de changement de page à cause d'une perte Wi-Fi ou MQTT. L'indication d'alerte doit rester disponible sans réseau.
- Conserver le fonctionnement et les contrôles des pages existantes en l'absence d'alerte.
- Ne pas afficher comme actuelle une mesure invalide ou trop ancienne.
- Éviter qu'une mise à jour visuelle réinitialise les commandes ou l'état des relais.

## Critères d'acceptation

- [ ] L'état normal ne change pas l'interface existante hors des éléments explicitement prévus par la maquette.
- [ ] Une alerte température apparaît après le délai firmware et disparaît après son délai de retour à la normale.
- [ ] La perte de mesures et le fallback sont visibles; une valeur périmée n'est pas présentée comme une mesure valide.
- [ ] Jusqu'à trois motifs simultanés sont affichés dans l'ordre de priorité défini.
- [ ] Une alerte principale ouvre Warning immédiatement; après navigation, elle revient après 60 secondes sans toucher si elle est encore active.
- [ ] Wi-Fi/MQTT seuls ne provoquent pas de changement de page.
- [ ] La page reste affichée après résolution et présente le retour à la normale.
- [ ] L'alerte reste visible ou consultable sans Wi-Fi, MQTT ou Home Assistant.
- [ ] Les transitions d'alerte ne bloquent pas l'interface et ne changent pas le comportement des relais.
- [ ] Après acquittement éventuel, l'indication revient tant que la condition d'alerte existe.
- [ ] Les essais sont faits sur cible avec des scénarios sûrs, sans charge de puissance active.

## Prochaine étape

Tester sur le C3 les états normal, température élevée, perte de mesures, fallback, défauts simultanés, navigation hors de Warning, retour après inactivité, résolution de l'alerte et perte réseau.
