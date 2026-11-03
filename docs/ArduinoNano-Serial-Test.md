# Test Arduino Nano - liaison série SmartCellar

Ce guide décrit le protocole attendu par le firmware C3 et donne un sketch de départ pour un **Nano classique ATmega328P**. Il lit un DHT22, reçoit les commandes des quatre sorties de régulation et du ventilateur, puis renvoie périodiquement les mesures et niveaux logiques.

## Raccordement proposé

| Nano classique | ESP32-C3 |
|---|---|
| D3 / TX | RX / GPIO20, **avec adaptation 5 V vers 3,3 V** |
| D2 / RX | TX / GPIO21 |
| GND | GND |
| D4 | DATA du DHT22 |
| D5, D6, D7, D8, D9 | Entrées des relais froid, chaud, déshumidificateur, humidificateur, ventilation |

Ajouter la résistance de tirage du DHT22 si le module/capteur n'en intègre pas. Vérifier le brochage réel des relais et leur polarité avant de brancher une charge. Le sketch ci-dessous est réglé par défaut pour des relais **actifs à l'état bas** : adapter `RelayActiveLow` au module utilisé.

Le connecteur du C3 fournit RX GPIO20 et TX GPIO21. Le firmware utilise `Serial1` à 9600 bauds sur ces broches, tandis que `Serial` reste configuré en USB CDC à 115200 bauds pour le moniteur. Le Nano utilise `SoftwareSerial` sur D2 (RX) et D3 (TX) à 9600 bauds; ses broches D0/D1 restent disponibles pour l'USB-série.

Relier TX du Nano à RX GPIO20 du C3 au travers d'un adaptateur de niveau ou d'un diviseur 5 V → 3,3 V. Relier TX GPIO21 du C3 à RX du Nano et les masses. Ne pas utiliser la broche 3V3 du connecteur pour alimenter le Nano classique.

## Protocole

Vitesse entre le C3 et le Nano : 9600 bauds, 8N1. Chaque message est un objet JSON terminé par un saut de ligne `\n`.

Le C3 envoie environ chaque seconde, ou lors d'un changement :

```json
{"type":"outputs","gpio":[false,false,false,false,false]}
```

Le Nano répond toutes les deux secondes avec une mesure DHT22 valide et les niveaux physiques lus sur les cinq broches de relais :

```json
{"type":"telemetry","temperature":12.5,"humidity":75.0,"gpio":[false,false,false,false,false]}
```

L'ordre des éléments est toujours : froid, chaud, déshumidificateur, humidificateur, ventilation. Dans `outputs`, `true` signifie « demander l'activation ». Dans `telemetry.gpio`, `true` signifie que la broche est au niveau logique HIGH; cela ne prouve pas l'état des contacts du relais.

Le C3 invalide les mesures après 6 secondes sans trame de télémétrie valide. Le Nano coupe ses cinq sorties après 4 secondes sans commande valide du C3, afin qu'une perte C3 → Nano ait aussi un comportement sûr. La minuterie de ventilation est gérée par le C3; le Nano n'applique que l'état reçu.

## Sketch de test

Installer la bibliothèque **DHT sensor library** d'Adafruit (et sa dépendance **Adafruit Unified Sensor**) depuis le gestionnaire de bibliothèques Arduino.

```cpp
#include <DHT.h>
#include <SoftwareSerial.h>
#include <ctype.h>
#include <math.h>
#include <string.h>

constexpr uint8_t C3RxPin = 2;
constexpr uint8_t C3TxPin = 3;
constexpr unsigned long C3BaudRate = 9600;
constexpr uint8_t DhtPin = 4;
constexpr uint8_t RelayPins[] = {5, 6, 7, 8, 9};
constexpr size_t RelayCount = sizeof(RelayPins) / sizeof(RelayPins[0]);
constexpr bool RelayActiveLow = true;  // Mettre false si le module est actif à HIGH.
constexpr unsigned long TelemetryPeriodMs = 2000;
constexpr unsigned long CommandTimeoutMs = 4000;
constexpr size_t MaxLineLength = 96;

DHT dht(DhtPin, DHT22);
SoftwareSerial C3Serial(C3RxPin, C3TxPin);
char inputLine[MaxLineLength];
size_t inputLength = 0;
bool discardLine = false;
bool relayRequested[RelayCount] = {false, false, false, false, false};
unsigned long lastCommandAt = 0;
unsigned long lastTelemetryAt = 0;

void applyRelayOutputs() {
  for (size_t i = 0; i < RelayCount; ++i) {
    const bool pinHigh = relayRequested[i] != RelayActiveLow;
    digitalWrite(RelayPins[i], pinHigh ? HIGH : LOW);
  }
}

void forceRelaysOff() {
  for (size_t i = 0; i < RelayCount; ++i) {
    relayRequested[i] = false;
  }
  applyRelayOutputs();
}

char *skipWhitespace(char *p) {
  while (*p && isspace(static_cast<unsigned char>(*p))) {
    ++p;
  }
  return p;
}

bool parseOutputs(char *line, bool states[RelayCount]) {
  if (!strstr(line, "\"type\":\"outputs\"")) {
    return false;
  }

  char *p = strstr(line, "\"gpio\"");
  if (!p || !(p = strchr(p, '['))) {
    return false;
  }
  ++p;

  bool parsed[RelayCount];
  for (size_t i = 0; i < RelayCount; ++i) {
    p = skipWhitespace(p);
    if (strncmp(p, "true", 4) == 0) {
      parsed[i] = true;
      p += 4;
    } else if (strncmp(p, "false", 5) == 0) {
      parsed[i] = false;
      p += 5;
    } else {
      return false;
    }

    p = skipWhitespace(p);
    if (i + 1 < RelayCount) {
      if (*p++ != ',') {
        return false;
      }
    } else if (*p != ']') {
      return false;
    }
  }

  memcpy(states, parsed, sizeof(parsed));
  return true;
}

void processLine(char *line) {
  bool states[RelayCount];
  if (!parseOutputs(line, states)) {
    return;
  }

  memcpy(relayRequested, states, sizeof(relayRequested));
  applyRelayOutputs();
  lastCommandAt = millis();
}

void readCommands() {
  while (C3Serial.available() > 0) {
    const char c = static_cast<char>(C3Serial.read());

    if (c == '\n') {
      if (!discardLine && inputLength > 0) {
        inputLine[inputLength] = '\0';
        processLine(inputLine);
      }
      inputLength = 0;
      discardLine = false;
    } else if (c != '\r' && !discardLine) {
      if (inputLength < sizeof(inputLine) - 1) {
        inputLine[inputLength++] = c;
      } else {
        inputLength = 0;
        discardLine = true;
      }
    }
  }
}

void sendTelemetry() {
  const float temperature = dht.readTemperature();
  const float humidity = dht.readHumidity();
  if (!isfinite(temperature) || !isfinite(humidity)) {
    return;
  }

  C3Serial.print(F("{\"type\":\"telemetry\",\"temperature\":"));
  C3Serial.print(temperature, 1);
  C3Serial.print(F(",\"humidity\":"));
  C3Serial.print(humidity, 1);
  C3Serial.print(F(",\"gpio\":["));

  for (size_t i = 0; i < RelayCount; ++i) {
    if (i > 0) {
      C3Serial.write(',');
    }
    C3Serial.print(digitalRead(RelayPins[i]) == HIGH ? F("true") : F("false"));
  }

  C3Serial.println(F("]}"));
}

void setup() {
  for (size_t i = 0; i < RelayCount; ++i) {
    const uint8_t offLevel = RelayActiveLow ? HIGH : LOW;
    digitalWrite(RelayPins[i], offLevel);
    pinMode(RelayPins[i], OUTPUT);
  }
  forceRelaysOff();

  Serial.begin(115200); // Moniteur USB
  C3Serial.begin(C3BaudRate);
  dht.begin();
  lastCommandAt = millis();
  lastTelemetryAt = millis();
}

void loop() {
  readCommands();

  if (millis() - lastCommandAt >= CommandTimeoutMs) {
    forceRelaysOff();
  }

  if (millis() - lastTelemetryAt >= TelemetryPeriodMs) {
    lastTelemetryAt = millis();
    sendTelemetry();
  }
}
```

## Vérifications rapides

1. Téléverser le sketch avec le C3 débranché des broches D0/D1, puis reconnecter la liaison et les masses communes.
2. Vérifier sur le C3 que la température et l'humidité apparaissent et restent actualisées. Une mesure DHT22 invalide n'est pas envoyée; le timeout C3 doit alors invalider les mesures.
3. Vérifier dans l'interface web les quatre niveaux GPIO existants; le cinquième retour correspond au relais de ventilation. Tester les relais sans équipement dangereux connecté et confirmer la polarité avant tout raccordement de charge.
4. Couper uniquement la ligne Nano TX → C3 RX : après environ 6 secondes, le C3 doit invalider les mesures et demander les quatre sorties de régulation à OFF; la ventilation reste indépendante de la validité DHT.
5. Couper uniquement la ligne C3 TX → Nano RX : le Nano doit couper ses cinq sorties au plus tard 4 secondes après sa dernière commande valide.
6. Rétablir les fils et vérifier le retour à un fonctionnement normal.

Le protocole JSON passe exclusivement par `C3Serial`; `Serial` reste réservé au moniteur USB et peut recevoir des diagnostics sans polluer les trames.
