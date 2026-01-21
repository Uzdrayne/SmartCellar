# SmartCellar_Display

## Présentation Générale

SmartCellar_Display est un projet embarqué basé sur l'ESP32-C3, utilisant le framework Arduino. Il vise à fournir une interface graphique avancée pour la gestion et le contrôle d'une cave intelligente (Smart Cellar). Le projet intègre un écran tactile, une interface web, la gestion du WiFi, et s'appuie sur des bibliothèques puissantes telles que LVGL (pour l'UI) et LovyanGFX (pour l'affichage).

## Architecture du Projet

- **src/** : Contient le code source principal du firmware.
  - **main.cpp** : Point d'entrée du programme. Initialise les modules, la boucle principale, et orchestre l'exécution.
  - **DisplayAndTouch/** : Gestion de l'affichage et du tactile.
    - `ConfigDisplay.cpp/h` : Configuration et initialisation de l'écran et du tactile.
    - `lgfx_conf.hpp` : Configuration spécifique de LovyanGFX.
  - **Global/** : Variables et fonctions globales partagées.
    - `Global.cpp/h` : Déclaration et définition des variables globales.
  - **LVGL/** : Intégration de la bibliothèque LVGL pour l'UI.
    - `Lvgl_Global.cpp/h` : Initialisation et gestion de LVGL.
    - `Screen/` : Gestion des différents écrans/pages de l'interface.
  - **Wifi/** : Gestion du WiFi et du serveur web.
    - `my_WebServer.cpp/h` : Serveur web asynchrone (ESPAsyncWebServer).
    - `my_wifi.cpp/h` : Connexion et gestion du WiFi.
    - `PageWeb/` : Pages web spécifiques servies par le serveur.

- **lib/** : Bibliothèques tierces ou personnalisées.
  - **CST816D/** : Pilote pour le contrôleur tactile CST816D.
  - **LovyanGFX-master/** : Bibliothèque d'affichage avancée.
  - **Lvgl/** : Bibliothèque d'interface graphique.
  - **ui/** : Généré par SquareLine Studio, contient la description de l'UI.

- **SquareLineStudio/** : Fichiers de projet et assets générés par SquareLine Studio.
- **web/** : Fichiers web statiques (HTML, JS, CSS) pour l'interface web.
- **Tools/** : Scripts utilitaires (ex : conversion web vers C++).
- **partitions/** : Fichiers de partitionnement mémoire.
- **debug/** : Fichiers de configuration pour le debug.

## Fonctionnement Global

1. **Démarrage (main.cpp)**
   - Initialisation des variables globales.
   - Configuration de l'écran et du tactile via `ConfigDisplay`.
   - Initialisation de LovyanGFX pour l'affichage.
   - Initialisation de LVGL pour la gestion de l'UI.
   - Connexion au WiFi via `my_wifi`.
   - Lancement du serveur web asynchrone via `my_WebServer`.
   - Boucle principale : gestion de l'UI, du tactile, et des interactions réseau.

2. **Affichage et Tactile**
   - `ConfigDisplay` configure l'écran (via LovyanGFX) et le contrôleur tactile (CST816D).
   - Les événements tactiles sont capturés et transmis à LVGL pour interaction avec l'UI.

3. **Interface Graphique (LVGL)**
   - LVGL gère les widgets, pages, animations et interactions utilisateur.
   - Les écrans sont définis dans `Screen/` et via les fichiers générés par SquareLine Studio.

4. **Gestion du WiFi**
   - `my_wifi` gère la connexion, la reconnexion, et la configuration du WiFi.
   - Les informations de connexion peuvent être modifiées via l'interface web.

5. **Serveur Web**
   - `my_WebServer` utilise ESPAsyncWebServer pour servir des pages web (depuis `web/`).
   - Permet la configuration à distance, la visualisation d'informations, etc.
   - Les pages web interagissent avec le firmware via WebSockets ou requêtes HTTP.

6. **Modules et Utilités**
   - **CST816D** : Pilote I2C pour le tactile.
   - **LovyanGFX** : Gestion avancée de l'affichage (vitesse, rotation, couleurs).
   - **LVGL** : Gestion de l'UI, animations, widgets, thèmes.
   - **SquareLine Studio** : Génération graphique de l'UI, exportée en C++.
   - **Scripts Tools** : Automatisation de la conversion de ressources web en code C++.

## Ordre d'Appel et Interactions

1. `main.cpp`
   - Appelle `Global::init()`
   - Appelle `ConfigDisplay::init()`
   - Initialise LovyanGFX
   - Initialise LVGL (`Lvgl_Global::init()`)
   - Appelle `my_wifi::connect()`
   - Appelle `my_WebServer::start()`
   - Entre dans la boucle principale :
     - LVGL tick (gestion UI)
     - Lecture tactile
     - Traitement des requêtes web

## Détail des Fichiers et Modules

### src/main.cpp
- Point d'entrée, orchestre l'initialisation et la boucle principale.

### src/DisplayAndTouch/ConfigDisplay.cpp/h
- Configure l'écran (type, résolution, orientation).
- Initialise le tactile (CST816D).
- Fournit des fonctions utilitaires pour l'affichage.

### src/Global/Global.cpp/h
- Variables globales (état, configuration, etc.).
- Fonctions utilitaires partagées.

### src/LVGL/Lvgl_Global.cpp/h
- Initialise la bibliothèque LVGL.
- Gère le tick handler, la mémoire, et les callbacks d'événements.

### src/LVGL/Screen/
- Définit les différents écrans/pages de l'UI (accueil, configuration, etc.).
- Chaque écran est un module LVGL.

### src/Wifi/my_wifi.cpp/h
- Gère la connexion WiFi (scan, connexion, reconnexion).
- Stocke les paramètres de connexion.

### src/Wifi/my_WebServer.cpp/h
- Démarre le serveur web asynchrone.
- Sert les fichiers statiques (HTML, JS, CSS).
- Gère les WebSockets et les API REST.

### src/Wifi/PageWeb/
- Contient les handlers pour les pages web spécifiques.

### lib/CST816D/
- Pilote pour le contrôleur tactile CST816D (I2C).
- Fournit les fonctions de lecture des événements tactiles.

### lib/LovyanGFX-master/
- Bibliothèque d'affichage rapide et flexible.
- Supporte de nombreux écrans et contrôleurs.

### lib/Lvgl/
- Bibliothèque d'UI avancée (widgets, animations, thèmes).

### lib/ui/
- Fichiers générés par SquareLine Studio pour l'UI.
- `ui.h` : Déclaration des écrans et widgets.
- `src/` : Code source généré pour l'UI.

### web/
- Fichiers web statiques (HTML, JS, CSS) pour l'interface utilisateur distante.

### Tools/
- Scripts utilitaires (ex : `Web2CPP.py` pour convertir des fichiers web en tableaux C++).

### partitions/
- Fichiers de partitionnement mémoire pour l'ESP32.

### debug/
- Fichiers de configuration pour le debug matériel ou logiciel.

## Dépendances Principales

- **ArduinoJson** : Manipulation JSON (config, API).
- **ESPAsyncWebServer** : Serveur web asynchrone.
- **AsyncTCP** : Support TCP asynchrone.
- **WebSockets** : Communication temps réel avec l'UI web.
- **lvgl** : UI graphique embarquée.
- **LovyanGFX** : Affichage rapide et flexible.

## Schéma de Fonctionnement

```
[Démarrage]
   |
   v
[Initialisation globale]
   |
   v
[Config écran & tactile] --(CST816D)--> [LVGL]
   |
   v
[Connexion WiFi] --(my_wifi)--> [Serveur Web]
   |
   v
[UI locale (LVGL)] <--> [UI web (web/)]
   |
   v
[Boucle principale : gestion UI, tactile, web]
```

## Notes et Conseils

- Pour modifier l'UI, privilégier SquareLine Studio puis exporter dans `lib/ui`.
- Pour ajouter une page web, placer les fichiers dans `web/` et adapter le serveur web.
- Pour ajouter un écran, créer un module dans `src/LVGL/Screen/`.
- Pour toute modification hardware (écran, tactile), adapter `ConfigDisplay` et la config LovyanGFX.

## Auteurs et Contributeurs

- Auteur principal : robin
- Contributions : voir l'historique git et les fichiers README des bibliothèques tierces.

---

Pour toute question ou contribution, merci d'ouvrir une issue ou de contacter l'auteur principal.