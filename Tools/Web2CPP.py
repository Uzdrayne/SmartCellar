
#!/usr/bin/env python3
import os
import sys
import subprocess
import pathlib
import hashlib
import argparse
import time
import re
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

# --- Minification libs ---
def ensure_libs():
    for lib, package in [("csscompressor", "csscompressor"), ("rjsmin", "rjsmin"), ("htmlmin", "htmlmin2")]:
        try:
            __import__(lib)
        except ImportError:
            print(f"[INFO] Installation de {package}...")
            subprocess.check_call([sys.executable, "-m", "pip", "install", package])

ensure_libs()

import csscompressor
import rjsmin
import htmlmin


# --- Config ---
SRC_DIR = pathlib.Path("web")  # Dossier source contenant les fichiers web à embarquer
OUT_DIR = pathlib.Path("src/Wifi/PageWeb")  # Dossier de sortie pour les .h / .cpp générés
EXTS: Set[str] = {".html", ".css", ".ico", ".js"}  # Extensions gérées

ASSETS_H = "web_assets.h"      # En-tête généré (déclarations)
ASSETS_CPP = "web_assets.cpp"  # Implémentation + routes




# Table MIME par extension
MIME_BY_EXT = {
    ".html": "text/html",
    ".css": "text/css",
    ".js": "application/javascript",
    ".ico": "image/x-icon",
}

# S'assure que le dossier de sortie existe
OUT_DIR.mkdir(parents=True, exist_ok=True)
_log("DEBUG", f"Dossier de sortie prêt: {OUT_DIR.as_posix()}")

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
    return p.read_text(encoding="utf-8")

def read_binary_safe(p: pathlib.Path) -> bytes:
    """Lecture binaire brute (pour .ico)."""
    _log("DEBUG", f"Lecture binaire: {p.as_posix()}")
    return p.read_bytes()

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
    out.append("// Déclarations extern des ressources web")
    out.append("#include <Arduino.h>\n")


    out.append("class AsyncWebServer;")
    out.append(f"void AddRoutesToServer(AsyncWebServer& server, const char* DefaultPage);\n")

    out.append("namespace WEB {")

    for p in sources:
        ext = p.suffix.lower()[1:]  # 'html', 'css', 'js', 'ico'
        var = to_identifier(p.stem)

        if p.suffix.lower() == ".ico":
            out.append(f"  extern const uint8_t {var}_{ext}[] PROGMEM;")
        else:
            out.append(f"  extern const char {var}_{ext}[] PROGMEM;")

        out.append(f"  extern const size_t {var}_{ext}_len;")

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
    """
    ext = p.suffix.lower()[1:]
    var = to_identifier(p.stem)
    return f"{var}_{ext}", f"{var}_{ext}_len"

def generate_ASSETS_CPP(sources: List[pathlib.Path]) -> str:
    """
    Génère l'implémentation :
    - Définitions des données embarquées (char[] pour texte, uint8_t[] pour binaire)
    - Ajout des routes AsyncWebServer pour servir ces ressources
    """
    _log("INFO", "Génération du fichier d'implémentation (.cpp) des assets et des routes...")
    include_path = OUT_DIR.as_posix()
    if include_path.startswith("src/"):
        include_path = include_path[4:]

    out = []

    # includes
    out.append(f'#include "{include_path}/{ASSETS_H}"')
    out.append("#include <ESPAsyncWebServer.h>")
    out.append("")

    # ------------------------------------------------------------------
    # Définitions des ASSETS (ancien assets.cpp)
    # ------------------------------------------------------------------
    out.append("namespace WEB {")

    for p in sources:
        ext = p.suffix.lower()
        name = f"{to_identifier(p.stem)}_{ext[1:]}"
        _log("DEBUG", f"Traitement asset: {p.name} -> symbole: {name}")

        if ext == ".ico":
            data = read_binary_safe(p)
            out.append(f"const uint8_t {name}[] PROGMEM = {{")
            # dump en hex, 16 octets par ligne
            for i in range(0, len(data), 16):
                out.append("  " + ", ".join(f"0x{b:02X}" for b in data[i:i+16]) + ",")
            out.append("};")
            out.append(f"const size_t {name}_len = sizeof({name});\n")

        else:
            content = read_text_safe(p)
            minified = minify_content(content, ext)
            if len(minified) != len(content):
                _log("INFO", f"Minifié: {p.name} ({len(content)} -> {len(minified)} octets)")
            else:
                _log("DEBUG", f"Pas de minification ou identique: {p.name} ({len(content)} octets)")

            delim = pick_delimiter(minified)
            out.append(f'const char {name}[] PROGMEM = R"{delim}(')
            out.append(minified)
            out.append(f'){delim}";')
            out.append(f"const size_t {name}_len = sizeof({name}) - 1;\n")

    out.append("} // namespace WEB\n")

    # ------------------------------------------------------------------
    # Génération des ROUTES
    # ------------------------------------------------------------------
    out.append(f"void AddRoutesToServer(AsyncWebServer& server, const char* DefaultPage)")

    out.append("{")
    
    
    # Route par défaut et portail captif
    out.append('  // route par défaut')
    out.append('  server.on("/", HTTP_GET, [DefaultPage](AsyncWebServerRequest *request){')
    out.append('      request->redirect(DefaultPage);')
    out.append('  });\n')

    out.append('  // page non trouvée + portail captif')
    out.append('  server.onNotFound([DefaultPage](AsyncWebServerRequest *request){')
    out.append('    String url = request->url();')
    out.append('    if (url.indexOf("generate") != -1 || url.indexOf("gstatic") != -1) {')
    out.append('      Serial.printf("Portail Captif : Redirection de %s vers %s\\n", url.c_str(), DefaultPage);')
    out.append('    } else {')
    out.append('      Serial.printf("Redirection standard de %s vers %s\\n", url.c_str(), DefaultPage);')
    out.append('    }')
    out.append('    request->redirect(DefaultPage);')
    out.append('  });\n')

    

    for p in sources:
        data, length = symbol_names_for(p)
        url = route_path_for(p)
        mime = MIME_BY_EXT.get(p.suffix.lower(), "application/octet-stream")

        out.append(
            f'  server.on("{url}", HTTP_GET, [](AsyncWebServerRequest *request){{\n'
            f'    request->send(200, "{mime}", (const uint8_t*)WEB::{data}, WEB::{length});\n'
                    
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
    t0 = time.perf_counter()

    # Découverte des sources
    if not SRC_DIR.exists():
        _log("ERROR", f"Dossier source introuvable: {SRC_DIR.as_posix()}")
        sys.exit(1)

    sources = [p for p in sorted(SRC_DIR.glob("*")) if p.suffix.lower() in EXTS]
    _log("INFO", f"Fichiers détectés ({len(sources)}): " + ", ".join(p.name for p in sources) if sources else "Aucun fichier détecté.")

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
    """Point d'entrée quand appelé par PlatformIO"""
    try:
        # Importation dynamique de l'environnement SCons
        from SCons.Script import Import
        Import("env")
        _log("INFO", "Exécution automatique via PlatformIO Build System")
        main()
    except Exception as e:
        # Si on n'est pas dans PIO, on ignore l'erreur
        pass

if __name__ == "__main__":
    # Exécution via la tâche VS Code ou ligne de commande
    main()
else:
    # Exécution via le build automatique 'extra_scripts'
    pio_entry()