#!/usr/bin/env python3
import os
import sys
import pathlib
import hashlib
import time
import re
import gzip
from typing import Set, List


# ================================================================
# Paramétrage des logs (simple et sans dépendance)
# ================================================================
def _log(level: str, *args):
    """
    Impression uniforme des messages de log.
    Niveaux possibles : INFO, DEBUG, WARN, ERROR.
    """
    print(f"[{level}]", *args)

try:
    import csscompressor
except ImportError:
    csscompressor = None

try:
    import rjsmin
except ImportError:
    rjsmin = None

try:
    import htmlmin
except ImportError:
    htmlmin = None


# --- Config ---
# Déterminer la racine du projet
# Quand exécuté par PlatformIO, le cwd est la racine du projet
# Quand exécuté manuellement, on utilise le chemin du script
try:
    SCRIPT_DIR = pathlib.Path(__file__).parent.resolve()
    PROJECT_ROOT = SCRIPT_DIR.parent
except NameError:
    # __file__ non défini (exécution via SCons/PlatformIO)
    PROJECT_ROOT = pathlib.Path(os.getcwd()).resolve()

SRC_DIR = PROJECT_ROOT / "web"  # Dossier source contenant les fichiers web à embarquer
OUT_DIR = PROJECT_ROOT / "src/ServerWeb/PageWeb"  # Dossier de sortie pour les .h / .cpp générés
EXTS: Set[str] = {".html", ".css", ".ico", ".js", ".png"}  # Extensions gérées

ASSETS_H = "web_assets.h"      # En-tête généré (déclarations)
ASSETS_CPP = "web_assets.cpp"  # Implémentation + routes

# Extensions à compresser en gzip (les binaires comme .ico et .png sont déjà compressés)
GZIP_EXTS: Set[str] = {".html", ".css", ".js"}


# Table MIME par extension
MIME_BY_EXT = {
    ".html": "text/html",
    ".css": "text/css",
    ".js": "application/javascript",
    ".ico": "image/x-icon",
    ".png": "image/png",
}

# ================================================================
# UTILS
# ================================================================
def to_identifier(name: str) -> str:
    """
    Transforme un nom arbitraire en identifiant C++ valide
    (lettres/chiffres/underscore, pas de chiffre au début).
    """
    s = "".join(c if (c.isalnum() or c == "_") else "_" for c in name)
    if s and s[0].isdigit():
        s = "_" + s
    return s

def pick_delimiter(content: str) -> str:
    """
    Choisit un délimiteur RAW unique pour les littéraux C++ de type R"DELIM(... )DELIM".
    Évite toute collision avec le contenu.
    """
    base = "RAW"
    delim = base
    i = 0
    while delim in content:
        i += 1
        delim = f"{base}_{i}"
    return delim

def sha1_hex(data: bytes) -> str:
    """Empreinte SHA-1 hexadécimale (utile si besoin de cache/versionnage)."""
    return hashlib.sha1(data).hexdigest()

def read_text_safe(p: pathlib.Path) -> str:
    """Lecture texte UTF-8 stricte (erreurs si fichier invalide)."""
    _log("DEBUG", f"Lecture texte: {p.as_posix()}")
    content = p.read_text(encoding="utf-8")
    # Suppression du BOM UTF-8 (U+FEFF) si présent
    if content.startswith('\ufeff'):
        _log("DEBUG", f"BOM UTF-8 détecté et supprimé dans {p.name}")
        content = content[1:]
    # Suppression de tous les caractères BOM/ZWNBSP restants
    content = content.replace('\ufeff', '')
    return content

def read_binary_safe(p: pathlib.Path) -> bytes:
    """Lecture binaire brute (pour .ico)."""
    _log("DEBUG", f"Lecture binaire: {p.as_posix()}")
    return p.read_bytes()

def gzip_compress(data: bytes) -> bytes:
    """Compresse de façon reproductible des données avec gzip."""
    return gzip.compress(data, compresslevel=9, mtime=0)

def minify_content(content: str, ext: str) -> str:
    """
    Minifie le contenu en fonction de l'extension du fichier.
    Gère le CSS, le JS et le HTML (avec styles et scripts embarqués).
    """
    _log("DEBUG", f"Minification en cours pour l'extension : {ext}")

    # --- Cas du CSS ---
    if ext == ".css":
        if csscompressor:
            try:
                return csscompressor.compress(content)
            except Exception as e:
                _log("WARN", f"Erreur csscompressor: {e}")
        return re.sub(r'\s+', ' ', content) # Fallback : écrase les espaces

    # --- Cas du JS ---
    elif ext == ".js":
        if rjsmin:
            try:
                return rjsmin.jsmin(content)
            except Exception as e:
                _log("WARN", f"Erreur rjsmin: {e}")
        return content

    # --- Cas du HTML (le plus complet) ---
    elif ext == ".html":
        # 1. Minifier le CSS interne
        if csscompressor:
            content = re.sub(
                r"(<style.*?>)(.*?)(</style>)",
                lambda m: f"{m.group(1)}{csscompressor.compress(m.group(2))}{m.group(3)}",
                content, flags=re.DOTALL | re.IGNORECASE
            )

        # 2. Minifier le JS interne
        if rjsmin:
            content = re.sub(
                r"(<script.*?>)(.*?)(</script>)",
                lambda m: f"{m.group(1)}{rjsmin.jsmin(m.group(2))}{m.group(3)}",
                content, flags=re.DOTALL | re.IGNORECASE
            )

        # 3. Minifier le HTML global avec htmlmin
        if htmlmin is None:
            return content.strip()

        try:
            # On utilise les arguments validés pour htmlmin
            content = htmlmin.minify(
                content,
                remove_comments=True,
                reduce_empty_attributes=True,
                remove_all_empty_space=True,
                reduce_boolean_attributes=True
            )
        except Exception as e:
            _log("WARN", f"Erreur htmlmin: {e}. Nettoyage manuel des sauts de ligne.")
            # Fallback manuel : supprime les sauts de ligne si la lib échoue
            content = re.sub(r"[\r\n\t]+", " ", content)
            content = re.sub(r">\s+<", "><", content)
            
        return content.strip()

    # Si extension non gérée (ex: .ico), on retourne tel quel
    return content

def write_if_changed(dest: pathlib.Path, new_text: str) -> bool:
    """
    Écrit 'new_text' dans 'dest' en UTF-8 uniquement si le contenu change.
    Retourne True si écrit, False si inchangé.
    """
    new_bytes = new_text.encode("utf-8")
    if dest.exists():
        try:
            current = dest.read_bytes()
            if current == new_bytes:
                _log("INFO", f"Aucun changement: {dest.as_posix()}")
                return False
        except Exception as e:
            _log("WARN", f"Lecture existant échouée ({dest}): {e}. Réécriture forcée.")
    dest.parent.mkdir(parents=True, exist_ok=True)
    dest.write_bytes(new_bytes)
    _log("INFO", f"Fichier écrit: {dest.as_posix()} ({len(new_bytes)} octets)")
    return True

# ================================================================
# Génération : web_assets.h (extern uniquement)
# ================================================================
def generate_assets_header(sources: List[pathlib.Path]) -> str:
    """
    Génère le header C++ contenant les 'extern' des ressources web
    + la déclaration de AddRoutesToServer.
    """
    _log("INFO", "Génération du header des assets (déclarations extern)...")
    out = []

    out.append("#pragma once")
    out.append("// Déclarations extern des ressources web (gzip-compressées pour HTML/CSS/JS)")
    out.append("#include <Arduino.h>")
    out.append("")
    out.append("// Utilisation de la PSRAM pour les assets web (libère la SRAM)")
    out.append("#ifdef BOARD_HAS_PSRAM")
    out.append("  #define WEB_STORAGE_ATTR")
    out.append("#else")
    out.append("  #define WEB_STORAGE_ATTR PROGMEM")
    out.append("#endif\n")

    out.append("class AsyncWebServer;")
    out.append(f"void AddRoutesToServer(AsyncWebServer& server, const char* PortailPage, const char* DefaultPage);\n")

    out.append("namespace WEB {")

    for p in sources:
        data, length = symbol_names_for(p)
        # Tous les assets sont maintenant stockés en uint8_t[] (gzippés ou binaires bruts)
        out.append(f"  extern const uint8_t {data}[] WEB_STORAGE_ATTR;")
        out.append(f"  extern const size_t {length};")

    out.append("} // namespace WEB\n")
    return "\n".join(out)

# ================================================================
# web_all.cpp = (assets.cpp + routes.cpp)
# ================================================================
def route_path_for(p: pathlib.Path) -> str:

    ext = p.suffix.lower()

    return f"/{p.name}"

def symbol_names_for(p: pathlib.Path):
    """
    Noms des symboles C++ générés pour un fichier donné :
    - data : <nom>_<ext>
    - length : <nom>_<ext>_len
    Inclut le chemin relatif pour éviter les collisions de noms.
    Exemple: web/css/style.css -> css_style_css
    """
    ext = p.suffix.lower()[1:]
    
    # Calculer le chemin relatif depuis SRC_DIR pour éviter les collisions
    try:
        rel_path = p.relative_to(SRC_DIR)
        # Remplacer les séparateurs par des underscores
        path_str = str(rel_path).replace('\\', '/').replace('/', '_').replace('.', '_')
        # Retirer l'extension dupliquée à la fin
        if path_str.endswith(f'_{ext}'):
            path_str = path_str[:-len(ext)-1]
        var = to_identifier(path_str)
    except ValueError:
        # Fallback si le fichier n'est pas dans SRC_DIR
        var = to_identifier(p.stem)
    
    return f"{var}_{ext}", f"{var}_{ext}_len"

def generate_ASSETS_CPP(sources: List[pathlib.Path]) -> str:
    """
    Génère l'implémentation :
    - Définitions des données embarquées (gzippées pour texte, brutes pour binaire)
    - Ajout des routes AsyncWebServer pour servir ces ressources
    """
    _log("INFO", "Génération du fichier d'implémentation (.cpp) des assets et des routes...")
    include_path = OUT_DIR.as_posix()
    if include_path.startswith("src/"):
        include_path = include_path[4:]

    out = []

    # includes
    out.append(f'#include "{ASSETS_H}"')
    out.append("#include <ESPAsyncWebServer.h>")
    out.append("")

    # ------------------------------------------------------------------
    # Définitions des ASSETS (tout en uint8_t[] maintenant)
    # ------------------------------------------------------------------
    out.append("namespace WEB {")

    for p in sources:
        ext = p.suffix.lower()
        data, length = symbol_names_for(p)
        name = data
        try:
            rel_path = p.relative_to(SRC_DIR)
            _log("DEBUG", f"Traitement asset: {rel_path.as_posix()} -> symbole: {name}")
        except ValueError:
            _log("DEBUG", f"Traitement asset: {p.name} -> symbole: {name}")
    
        if ext in GZIP_EXTS:
            # Texte : minifier puis gzip-compresser
            content = read_text_safe(p)
            minified = minify_content(content, ext)
            raw_size = len(minified.encode("utf-8"))
            compressed = gzip_compress(minified.encode("utf-8"))
            gz_size = len(compressed)
            ratio = round((1 - gz_size / raw_size) * 100, 1) if raw_size > 0 else 0
            _log("INFO", f"Gzip: {p.name} ({raw_size} -> {gz_size} octets, -{ratio}%)")
            
            out.append(f"const uint8_t {name}[] WEB_STORAGE_ATTR = {{")
            for i in range(0, len(compressed), 16):
                out.append("  " + ", ".join(f"0x{b:02X}" for b in compressed[i:i+16]) + ",")
            out.append("};")
            out.append(f"const size_t {name}_len = sizeof({name});\n")

        elif ext in [".ico", ".png"]:
            # Binaire : garder tel quel (déjà compressé)
            binary_data = read_binary_safe(p)
            _log("INFO", f"Binaire: {p.name} ({len(binary_data)} octets, non compressé)")
            out.append(f"const uint8_t {name}[] WEB_STORAGE_ATTR = {{")
            for i in range(0, len(binary_data), 16):
                out.append("  " + ", ".join(f"0x{b:02X}" for b in binary_data[i:i+16]) + ",")
            out.append("};")
            out.append(f"const size_t {name}_len = sizeof({name});\n")

        else:
            # Autre type non géré : stocker tel quel en binaire
            content = read_text_safe(p)
            raw_bytes = content.encode("utf-8")
            out.append(f"const uint8_t {name}[] WEB_STORAGE_ATTR = {{")
            for i in range(0, len(raw_bytes), 16):
                out.append("  " + ", ".join(f"0x{b:02X}" for b in raw_bytes[i:i+16]) + ",")
            out.append("};")
            out.append(f"const size_t {name}_len = sizeof({name});\n")

    out.append("} // namespace WEB\n")

    # ------------------------------------------------------------------
    # Génération des ROUTES
    # ------------------------------------------------------------------
    out.append(f"void AddRoutesToServer(AsyncWebServer& server, const char* PortailPage, const char* DefaultPage)")

    out.append("{")
    
    
    # Route par défaut
    out.append('  // ========== Route par défaut ==========')
    out.append('  server.on("/", HTTP_GET, [DefaultPage](AsyncWebServerRequest *request){')
    out.append('    request->redirect(DefaultPage);')
    out.append('  });\n')

    # Portail captif — Windows
    out.append('  // ========== Portail captif — Windows ==========')
    out.append('  server.on("/connecttest.txt", HTTP_GET, [](AsyncWebServerRequest *request){')
    out.append('    Serial.println("[Captive] Windows connecttest.txt");')
    out.append('    request->send(200, "text/plain", "captive");')
    out.append('  });\n')
    out.append('  server.on("/redirect", HTTP_GET, [PortailPage](AsyncWebServerRequest *request){')
    out.append('    Serial.println("[Captive] Windows /redirect -> Portail");')
    out.append('    request->redirect(PortailPage);')
    out.append('  });\n')
    out.append('  server.on("/ncsi.txt", HTTP_GET, [](AsyncWebServerRequest *request){')
    out.append('    Serial.println("[Captive] Windows ncsi.txt");')
    out.append('    request->send(200, "text/plain", "captive");')
    out.append('  });\n')

    # Portail captif — Android
    out.append('  // ========== Portail captif — Android ==========')
    out.append('  server.on("/generate_204", HTTP_GET, [PortailPage](AsyncWebServerRequest *request){')
    out.append('    Serial.println("[Captive] Android generate_204");')
    out.append('    request->redirect(PortailPage);')
    out.append('  });\n')
    out.append('  server.on("/gen_204", HTTP_GET, [PortailPage](AsyncWebServerRequest *request){')
    out.append('    Serial.println("[Captive] Android gen_204");')
    out.append('    request->redirect(PortailPage);')
    out.append('  });\n')

    # Portail captif — Apple
    out.append('  // ========== Portail captif — Apple ==========')
    out.append('  server.on("/hotspot-detect.html", HTTP_GET, [PortailPage](AsyncWebServerRequest *request){')
    out.append('    Serial.println("[Captive] Apple hotspot-detect");')
    out.append('    request->redirect(PortailPage);')
    out.append('  });\n')

    # 404 — fallback captif
    out.append('  // ========== 404 — fallback captif ==========')
    out.append('  server.onNotFound([DefaultPage, PortailPage](AsyncWebServerRequest *request){')
    out.append('    String url = request->url();')
    out.append('    if (url.indexOf("generate") != -1 ||')
    out.append('        url.indexOf("gstatic") != -1 ||')
    out.append('        url.indexOf("hotspot-detect") != -1 ||')
    out.append('        url.indexOf("connecttest") != -1 ||')
    out.append('        url.indexOf("ncsi") != -1 ||')
    out.append('        url.indexOf("captive") != -1) {')
    out.append('      Serial.printf("[Captive] Fallback: %s\\n", url.c_str());')
    out.append('      request->redirect(PortailPage);')
    out.append('    } else {')
    out.append('      Serial.printf("[404] %s\\n", url.c_str());')
    out.append('      request->redirect(DefaultPage);')
    out.append('    }')
    out.append('  });\n')

    for p in sources:
        data, length = symbol_names_for(p)
        url = route_path_for(p)
        mime = MIME_BY_EXT.get(p.suffix.lower(), "application/octet-stream")
        ext = p.suffix.lower()
        
        if ext in GZIP_EXTS:
            # Assets gzippés : utiliser AsyncWebServerResponse avec Content-Encoding: gzip
            out.append(
                f'  server.on("{url}", HTTP_GET, [](AsyncWebServerRequest *request){{\n'
                f'    AsyncWebServerResponse *response = request->beginResponse(200, "{mime}", WEB::{data}, WEB::{length});\n'
                f'    response->addHeader("Content-Encoding", "gzip");\n'
                f'    request->send(response);\n'
                f'  }});\n'
            )
        else:
            # Assets non compressés (binaires : ico, png)
            out.append(
                f'  server.on("{url}", HTTP_GET, [](AsyncWebServerRequest *request){{\n'
                f'    request->send(200, "{mime}", WEB::{data}, WEB::{length});\n'
                f'  }});\n'
            )

    out.append("}")   
    return "\n".join(out)

    
# ================================================================
# MAIN
# ================================================================
def main():
    """
    Étapes :
    1) Recherche des fichiers sources dans SRC_DIR filtrés par EXTS
    2) Génération du header (.h) et de l'implémentation (.cpp)
    3) Écriture conditionnelle (uniquement si contenus modifiés)
    """
    _log("INFO", "Démarrage de la génération des assets web...")
    _log("DEBUG", f"Racine du projet: {PROJECT_ROOT}")
    _log("DEBUG", f"Dossier source: {SRC_DIR}")
    _log("DEBUG", f"Dossier de sortie: {OUT_DIR}")
    t0 = time.perf_counter()

    # S'assure que le dossier de sortie existe
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    
    # Vérifier que le dossier source existe
    if not SRC_DIR.exists():
        _log("ERROR", f"Dossier source introuvable: {SRC_DIR}")
        sys.exit(1)

    # Recherche récursive avec rglob au lieu de glob
    sources = [p for p in sorted(SRC_DIR.rglob("*")) if p.is_file() and p.suffix.lower() in EXTS]
    
    if sources:
        _log("INFO", f"Fichiers détectés ({len(sources)}):")
        for p in sources:
            rel = p.relative_to(SRC_DIR)
            _log("INFO", f"  - {rel.as_posix()}")
    else:
        _log("INFO", "Aucun fichier détecté.")
    
    # Détecter les collisions de routes (même nom de fichier dans des dossiers différents)
    routes = {}
    for p in sources:
        route = route_path_for(p)
        if route in routes:
            _log("WARN", f"Collision de route détectée: {route}")
            _log("WARN", f"  - {routes[route].relative_to(SRC_DIR).as_posix()}")
            _log("WARN", f"  - {p.relative_to(SRC_DIR).as_posix()}")
        else:
            routes[route] = p

    # web_assets.h
    header_text = generate_assets_header(sources)
    wrote_h = write_if_changed(OUT_DIR / ASSETS_H, header_text)

    # web_all.cpp (ici fusionné sous ASSETS_CPP)
    cpp_text = generate_ASSETS_CPP(sources)
    wrote_cpp = write_if_changed(OUT_DIR / ASSETS_CPP, cpp_text)

    dt = round(time.perf_counter() - t0, 2)
    if wrote_h or wrote_cpp:
        _log("OK", f"Génération terminée en {dt} s")
    else:
        _log("OK", f"Aucun changement. Terminé en {dt} s")

def pio_entry():
    """Génère les ressources lorsque PlatformIO charge ce script."""
    _log("INFO", "Exécution automatique via PlatformIO Build System")
    main()

if __name__ == "__main__":
    # Exécution via la tâche VS Code ou ligne de commande
    main()
else:
    # Exécution via le build automatique 'extra_scripts'
    pio_entry()