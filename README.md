# SmartCellar Controller + Display

Firmware PlatformIO pour ESP32-C3, combinant l'écran tactile LVGL du projet Display avec la régulation, l'interface web et MQTT de SmartCellar Controller.

## Architecture

- `src/DisplayAndTouch/`, `src/LVGL/` et `lib/ui/` : écran, tactile et tableau de bord local.
- `src/Controller/` et `src/Data/` : régulation, paramètres persistants, état des mesures et des sorties.
- `src/SerialLink/` : liaison JSON ligne par ligne avec un Arduino compagnon.
- `src/Wifi/` et `src/ServerWeb/` : Wi-Fi STA/point d'accès, DNS, interface web et WebSocket navigateur.
- `src/WifiAndMqtt/` et `lib/HomeAssistantMQTT/` : MQTT et découverte Home Assistant.
- `web/` : sources HTML, CSS et JavaScript; le script `Tools/WebtoCPP_Gzip.py` les embarque compressées dans le firmware.

Le C3 conserve les consignes, calcule les quatre états de régulation et les affiche localement. L'Arduino compagnon lit le DHT22 et pilote les sorties physiques. Le capteur et les relais ne sont donc pas raccordés aux GPIO de l'écran.

## Liaison série avec l'Arduino compagnon

Configuration C3 par défaut : UART `Serial1`, 9600 bauds, 8N1, RX GPIO20 et TX GPIO21, correspondant aux broches RX/TX du connecteur de la carte. Les broches sont définies par `CELLAR_LINK_RX_PIN` et `CELLAR_LINK_TX_PIN` dans `src/SerialLink/SerialLink.h`.

Chaque message est un objet JSON terminé par `\n`. Ordre des sorties : froid, chaud, déshumidificateur, humidificateur, ventilateur.

Mesures et retour logique des cinq sorties rapportées par le compagnon :

```json
{"type":"telemetry","temperature":12.5,"humidity":75.0,"gpio":[false,false,false,false,false]}
```

Le champ `gpio` est facultatif; sans lui, les mesures restent utilisables mais le retour des sorties est signalé indisponible.

États demandés par le C3 :

```json
{"type":"outputs","gpio":[false,false,false,false,false]}
```

Le C3 transmet une nouvelle trame à chaque changement et répète l'état au moins une fois par seconde. Si aucune mesure valide n'arrive pendant 6 secondes, les mesures sont invalidées et les quatre relais de régulation sont arrêtés; le ventilateur reste géré selon sa configuration cyclique. Le retour `gpio` indique les niveaux logiques lus par le compagnon, pas l'état électrique des contacts des relais.

Raccorder TX du C3 (GPIO21) à RX du Nano, RX du C3 (GPIO20) à TX du Nano, et relier les masses. Le Nano étant généralement en logique 5 V, adapter son signal TX avant l'entrée RX 3,3 V du C3. Ne pas utiliser la broche 3V3 du connecteur pour alimenter un Nano 5 V.

GPIO20/21 sont aussi les broches UART0 par défaut de l'ESP32-C3. Le projet configure `Serial` en USB CDC et utilise `Serial1` pour le Nano; vérifier sur la carte assemblée que le moniteur USB et le téléversement restent fonctionnels avec le connecteur branché.

## Interface et réseau

Les pages web du contrôleur sont embarquées et accessibles depuis le réseau local ou le point d'accès `SmartCellar`. Elles permettent de régler les consignes, les hystérésis, le Wi-Fi et MQTT. La page capteur indique le DHT22 du compagnon et permet de régler ses offsets.

Le tableau de bord web affiche les états calculés par le C3 et, lorsqu'ils sont transmis, les niveaux GPIO rapportés par l'Arduino. MQTT/Home Assistant reste optionnel et se configure depuis l'interface web.

## Compilation

Depuis la racine du projet :

```powershell
pio run -e esp32c3
```

La génération des pages web est exécutée automatiquement avant la compilation. L'écran et le tactile conservent leur brochage existant; ne pas flasher le brochage relais de l'ancien contrôleur V2 sur le C3.
