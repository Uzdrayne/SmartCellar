#!/usr/bin/env python3
"""
Convertit les assets web (html/css/js/ico/png) en tableaux C++ pour ESP32
et génère les routes ESPAsyncWebServer correspondantes.

Sorties (identiques à la version précédente) :
  - web_assets.h   : déclarations extern (namespace WEB) + AddRoutesToServer()
  - web_assets.cpp : données + routes (portail captif inclus)

Compatibilité : mêmes fichiers, mêmes symboles (WEB::<nom>_<ext>, ..._len),
même signature AddRoutesToServer(server, PortailPage, DefaultPage).

Dépendances optionnelles : csscompressor, rjsmin, htmlmin, zopfli
"""
import gzip
import hashlib
import os
import pathlib
import re
import sys
import time
from dataclasses import dataclass
from typing import Dict, List, Set

# ================================================================
# Config
# ================================================================
# Racine du projet : dossier parent du script, ou cwd sous SCons/PlatformIO
try:
    SCRIPT_DIR = pathlib.Path(__file__).parent.resolve()
    PROJECT_ROOT = SCRIPT_DIR.parent
except NameError:
    PROJECT_ROOT = pathlib.Path(os.getcwd()).resolve()

SRC_DIR = PROJECT_ROOT / "web"                      # Sources web
OUT_DIR = PROJECT_ROOT / "src/ServerWeb/PageWeb"    # Sortie .h / .cpp
EXTS: Set[str] = {".html", ".css", ".ico", ".js", ".png"}
GZIP_EXTS: Set[str] = {".html", ".css", ".js"}      # .ico/.png déjà compressés

ASSETS_H = "web_assets.h"
ASSETS_CPP = "web_assets.cpp"

# --- Options ---
USE_ZOPFLI = True                    # Si le module `zopfli` est installé (gain ~3-8 %)
ZOPFLI_ITERATIONS = 15
ENABLE_ETAG = True                   # ETag + réponses 304 (économise la bande passante)
HTML_REMOVE_ALL_EMPTY_SPACE = True   # Option htmlmin (mettre False si espaces inline perdus)
BYTES_PER_LINE = 20                  # Largeur des tableaux générés
VERBOSE = os.environ.get("WEBGEN_VERBOSE") == "1" or "--verbose" in sys.argv

MIME_BY_EXT = {
    ".html": "text/html; charset=utf-8",
    ".css": "text/css; charset=utf-8",
    ".js": "text/javascript; charset=utf-8",
    ".ico": "image/x-icon",
    ".png": "image/png",
}

# Types <script> considérés comme du JavaScript (les autres ne sont pas minifiés)
JS_SCRIPT_TYPES = {"", "text/javascript", "application/javascript", "module"}


# ================================================================
# Logs
# ================================================================
def _log(level: str, *args):
    """Log uniforme. Les DEBUG ne s'affichent qu'avec WEBGEN_VERBOSE=1 ou --verbose."""
    if level == "DEBUG" and not VERBOSE:
        return
    print(f"[{level}]", *args)


# ================================================================
# Dépendances optionnelles
# ================================================================
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

try:
    import zopfli.gzip as zopfli_gzip
except ImportError:
    zopfli_gzip = None


def warn_missing_tools():
    """Avertit une fois si des outils manquent (la taille de sortie en dépend)."""
    missing = [name for name, mod in (
        ("csscompressor", csscompressor),
        ("rjsmin", rjsmin),
        ("htmlmin", htmlmin),
    ) if mod is None]
    if missing:
        _log("WARN", f"Minifieurs absents: {', '.join(missing)} "
                     f"-> sortie moins optimisée (pip install {' '.join(missing)})")
    if USE_ZOPFLI and zopfli_gzip is None:
        _log("INFO", "zopfli absent: utilisation de gzip -9 (pip install zopfli pour gagner quelques %)")


# ================================================================
# Utilitaires
# ================================================================
def to_identifier(name: str) -> str:
    """Nom arbitraire -> identifiant C++ valide."""
    s = "".join(c if (c.isalnum() or c == "_") else "_" for c in name)
    if s and s[0].isdigit():
        s = "_" + s
    return s


def read_text_safe(p: pathlib.Path) -> str:
    """Lecture UTF-8 stricte, BOM supprimé."""
    _log("DEBUG", f"Lecture texte: {p.as_posix()}")
    return p.read_text(encoding="utf-8").replace("\ufeff", "")


def compress(data: bytes) -> bytes:
    """Gzip reproductible (mtime=0), via zopfli si disponible."""
    if USE_ZOPFLI and zopfli_gzip is not None:
        return zopfli_gzip.compress(data, numiterations=ZOPFLI_ITERATIONS)
    return gzip.compress(data, compresslevel=9, mtime=0)


def write_if_changed(dest: pathlib.Path, new_text: str) -> bool:
    """Écrit seulement si le contenu change. Retourne True si écrit."""
    new_bytes = new_text.encode("utf-8")
    if dest.exists():
        try:
            if dest.read_bytes() == new_bytes:
                _log("INFO", f"Aucun changement: {dest.as_posix()}")
                return False
        except Exception as e:
            _log("WARN", f"Lecture existant échouée ({dest}): {e}. Réécriture forcée.")
    dest.parent.mkdir(parents=True, exist_ok=True)
    dest.write_bytes(new_bytes)
    _log("INFO", f"Fichier écrit: {dest.as_posix()} ({len(new_bytes)} octets)")
    return True


# ================================================================
# Minification
# ================================================================
_STYLE_RE = re.compile(r"(<style\b[^>]*>)(.*?)(</style\s*>)", re.DOTALL | re.IGNORECASE)
_SCRIPT_RE = re.compile(r"(<script\b([^>]*)>)(.*?)(</script\s*>)", re.DOTALL | re.IGNORECASE)
_TYPE_RE = re.compile(r"""\btype\s*=\s*["']?([^"'\s>]+)""", re.IGNORECASE)


def _css(text: str) -> str:
    if csscompressor and text.strip():
        try:
            return csscompressor.compress(text)
        except Exception as e:
            _log("WARN", f"Erreur csscompressor: {e}")
    return text


def _js(text: str) -> str:
    if rjsmin and text.strip():
        try:
            return rjsmin.jsmin(text)
        except Exception as e:
            _log("WARN", f"Erreur rjsmin: {e}")
    return text


def _minify_inline_script(m: "re.Match") -> str:
    open_tag, attrs, body, close_tag = m.group(1), m.group(2), m.group(3), m.group(4)
    t = _TYPE_RE.search(attrs)
    script_type = t.group(1).lower() if t else ""
    if script_type not in JS_SCRIPT_TYPES:
        return m.group(0)  # JSON, templates... : on ne touche pas
    return f"{open_tag}{_js(body)}{close_tag}"


def minify_content(content: str, ext: str) -> str:
    """Minifie CSS / JS / HTML (avec <style> et <script> inline)."""
    _log("DEBUG", f"Minification ({ext})")

    if ext == ".css":
        if csscompressor:
            return _css(content)
        return re.sub(r"\s+", " ", content)

    if ext == ".js":
        return _js(content)

    if ext == ".html":
        if csscompressor:
            content = _STYLE_RE.sub(lambda m: f"{m.group(1)}{_css(m.group(2))}{m.group(3)}", content)
        if rjsmin:
            content = _SCRIPT_RE.sub(_minify_inline_script, content)

        if htmlmin is None:
            return content.strip()
        try:
            content = htmlmin.minify(
                content,
                remove_comments=True,
                reduce_empty_attributes=True,
                remove_all_empty_space=HTML_REMOVE_ALL_EMPTY_SPACE,
                reduce_boolean_attributes=True,
            )
        except Exception as e:
            _log("WARN", f"Erreur htmlmin: {e}. Nettoyage manuel.")
            content = re.sub(r"[\r\n\t]+", " ", content)
            content = re.sub(r">\s+<", "><", content)
        return content.strip()

    return content


# ================================================================
# Modèle d'asset
# ================================================================
@dataclass
class Asset:
    path: pathlib.Path
    symbol: str      # ex: css_style_css
    route: str       # ex: /style.css
    mime: str
    payload: bytes   # octets embarqués (gzip ou bruts)
    gzipped: bool
    etag: str        # "<hash>" avec guillemets, ou ""

    @property
    def length_symbol(self) -> str:
        return f"{self.symbol}_len"


def symbol_for(p: pathlib.Path) -> str:
    """web/css/style.css -> css_style_css (chemin relatif => pas de collision de nom)."""
    try:
        return to_identifier(p.relative_to(SRC_DIR).as_posix())
    except ValueError:
        return to_identifier(p.name)


def route_for(p: pathlib.Path) -> str:
    """Route servie : /<nom du fichier> (dossiers aplatis, comme avant)."""
    return f"/{p.name}"


def build_asset(p: pathlib.Path) -> Asset:
    """Lit, minifie et compresse un fichier source."""
    ext = p.suffix.lower()

    if ext in GZIP_EXTS:
        raw = minify_content(read_text_safe(p), ext).encode("utf-8")
        packed = compress(raw)
        if len(packed) < len(raw):
            payload, gzipped = packed, True
            ratio = round((1 - len(packed) / len(raw)) * 100, 1) if raw else 0
            _log("INFO", f"Gzip: {p.name} ({len(raw)} -> {len(packed)} octets, -{ratio}%)")
        else:
            payload, gzipped = raw, False   # gzip n'apporte rien (fichier minuscule)
            _log("INFO", f"Brut: {p.name} ({len(raw)} octets, gzip non rentable)")
    else:
        payload, gzipped = p.read_bytes(), False
        _log("INFO", f"Binaire: {p.name} ({len(payload)} octets, non compressé)")

    etag = f'"{hashlib.sha1(payload).hexdigest()[:16]}"' if ENABLE_ETAG else ""
    return Asset(
        path=p,
        symbol=symbol_for(p),
        route=route_for(p),
        mime=MIME_BY_EXT.get(ext, "application/octet-stream"),
        payload=payload,
        gzipped=gzipped,
        etag=etag,
    )


def check_collisions(assets: List[Asset]) -> bool:
    """Vérifie routes et symboles uniques. Retourne True si tout va bien."""
    ok = True
    for label, key in (("route", lambda a: a.route), ("symbole", lambda a: a.symbol)):
        seen: Dict[str, Asset] = {}
        for a in assets:
            k = key(a)
            if k in seen:
                ok = False
                _log("ERROR", f"Collision de {label}: {k}")
                _log("ERROR", f"  - {seen[k].path.relative_to(SRC_DIR).as_posix()}")
                _log("ERROR", f"  - {a.path.relative_to(SRC_DIR).as_posix()}")
            else:
                seen[k] = a
    return ok


# ================================================================
# Génération : web_assets.h
# ================================================================
def generate_assets_header(assets: List[Asset]) -> str:
    _log("INFO", "Génération du header des assets...")
    out = [
        "#pragma once",
        "// Déclarations extern des ressources web (gzip pour HTML/CSS/JS, brut pour ico/png)",
        "// Fichier généré automatiquement : ne pas modifier.",
        "#include <Arduino.h>",
        "",
        "// NB: sur ESP32 un `const` global est placé en flash dans tous les cas",
        "// (PROGMEM est sans effet) ; la PSRAM n'est jamais utilisée pour ces tableaux.",
        "#ifdef BOARD_HAS_PSRAM",
        "  #define WEB_STORAGE_ATTR",
        "#else",
        "  #define WEB_STORAGE_ATTR PROGMEM",
        "#endif",
        "",
        "class AsyncWebServer;",
        "void AddRoutesToServer(AsyncWebServer& server, const char* PortailPage, const char* DefaultPage);",
        "",
        "namespace WEB {",
    ]
    for a in assets:
        out.append(f"  extern const uint8_t {a.symbol}[] WEB_STORAGE_ATTR;")
        out.append(f"  extern const size_t {a.length_symbol};")
    out.append("} // namespace WEB")
    out.append("")
    return "\n".join(out)


# ================================================================
# Génération : web_assets.cpp
# ================================================================
def emit_array(name: str, data: bytes) -> List[str]:
    """Tableau C++ uint8_t + constante de longueur."""
    lines = [f"const uint8_t {name}[] WEB_STORAGE_ATTR = {{"]
    for i in range(0, len(data), BYTES_PER_LINE):
        lines.append("  " + ",".join(str(b) for b in data[i:i + BYTES_PER_LINE]) + ",")
    lines.append("};")
    lines.append(f"const size_t {name}_len = sizeof({name});")
    lines.append("")
    return lines


# Portail captif + redirections (code C++ inchangé par rapport à l'ancienne version)
CAPTIVE_PORTAL_CPP = r'''
  // ========== Route par défaut ==========
  server.on("/", HTTP_GET, [DefaultPage](AsyncWebServerRequest *request){
    request->redirect(DefaultPage);
  });

  // ========== Portail captif — Windows ==========
  server.on("/connecttest.txt", HTTP_GET, [](AsyncWebServerRequest *request){
    Serial.println("[Captive] Windows connecttest.txt");
    request->send(200, "text/plain", "captive");
  });
  server.on("/redirect", HTTP_GET, [PortailPage](AsyncWebServerRequest *request){
    Serial.println("[Captive] Windows /redirect -> Portail");
    request->redirect(PortailPage);
  });
  server.on("/ncsi.txt", HTTP_GET, [](AsyncWebServerRequest *request){
    Serial.println("[Captive] Windows ncsi.txt");
    request->send(200, "text/plain", "captive");
  });

  // ========== Portail captif — Android ==========
  server.on("/generate_204", HTTP_GET, [PortailPage](AsyncWebServerRequest *request){
    Serial.println("[Captive] Android generate_204");
    request->redirect(PortailPage);
  });
  server.on("/gen_204", HTTP_GET, [PortailPage](AsyncWebServerRequest *request){
    Serial.println("[Captive] Android gen_204");
    request->redirect(PortailPage);
  });

  // ========== Portail captif — Apple ==========
  server.on("/hotspot-detect.html", HTTP_GET, [PortailPage](AsyncWebServerRequest *request){
    Serial.println("[Captive] Apple hotspot-detect");
    request->redirect(PortailPage);
  });

  // ========== 404 — fallback captif ==========
  server.onNotFound([DefaultPage, PortailPage](AsyncWebServerRequest *request){
    String url = request->url();
    if (url.indexOf("generate") != -1 ||
        url.indexOf("gstatic") != -1 ||
        url.indexOf("hotspot-detect") != -1 ||
        url.indexOf("connecttest") != -1 ||
        url.indexOf("ncsi") != -1 ||
        url.indexOf("captive") != -1) {
      Serial.printf("[Captive] Fallback: %s\n", url.c_str());
      request->redirect(PortailPage);
    } else {
      Serial.printf("[404] %s\n", url.c_str());
      request->redirect(DefaultPage);
    }
  });
'''

ASSET_STRUCT_CPP = '''namespace {
struct WebAsset {
  const char*    path;
  const char*    mime;
  const uint8_t* data;
  size_t         len;
  bool           gzip;
  const char*    etag;   // "" si désactivé
};
}
'''

ASSET_ROUTES_CPP = '''
  // ========== Assets web (table unique -> une seule lambda en flash) ==========
  for (const WebAsset& a : WEB_ASSETS) {
    const WebAsset* asset = &a;
    server.on(asset->path, HTTP_GET, [asset](AsyncWebServerRequest *request){
%s      AsyncWebServerResponse *response = request->beginResponse(200, asset->mime, asset->data, asset->len);
      if (asset->gzip) response->addHeader("Content-Encoding", "gzip");
%s      request->send(response);
    });
  }
'''

ETAG_CHECK_CPP = '''      if (asset->etag[0] && request->hasHeader("If-None-Match") &&
          request->header("If-None-Match") == asset->etag) {
        request->send(304);
        return;
      }
'''

ETAG_HEADERS_CPP = '''      if (asset->etag[0]) {
        response->addHeader("ETag", asset->etag);
        response->addHeader("Cache-Control", "no-cache");
      }
'''


def generate_assets_cpp(assets: List[Asset]) -> str:
    _log("INFO", "Génération de l'implémentation (.cpp)...")
    out = [
        "// Fichier généré automatiquement : ne pas modifier.",
        f'#include "{ASSETS_H}"',
        "#include <ESPAsyncWebServer.h>",
        "",
        "namespace WEB {",
        "",
    ]
    for a in assets:
        _log("DEBUG", f"Asset: {a.path.name} -> {a.symbol}")
        out.extend(emit_array(a.symbol, a.payload))
    out.append("} // namespace WEB")
    out.append("")

    if assets:
        out.append(ASSET_STRUCT_CPP)
        out.append("static const WebAsset WEB_ASSETS[] = {")
        for a in assets:
            out.append(
                f'  {{"{a.route}", "{a.mime}", WEB::{a.symbol}, WEB::{a.length_symbol}, '
                f'{"true" if a.gzipped else "false"}, "{a.etag.replace(chr(34), chr(92) + chr(34))}"}},'
            )
        out.append("};")
        out.append("")

    out.append("void AddRoutesToServer(AsyncWebServer& server, const char* PortailPage, const char* DefaultPage)")
    out.append("{")
    out.append(CAPTIVE_PORTAL_CPP.rstrip("\n"))
    if assets:
        etag_check = ETAG_CHECK_CPP if ENABLE_ETAG else ""
        etag_headers = ETAG_HEADERS_CPP if ENABLE_ETAG else ""
        out.append(ASSET_ROUTES_CPP % (etag_check, etag_headers))
    out.append("}")
    out.append("")
    return "\n".join(out)


# ================================================================
# MAIN
# ================================================================
def main():
    _log("INFO", "Démarrage de la génération des assets web...")
    _log("DEBUG", f"Racine du projet: {PROJECT_ROOT}")
    _log("DEBUG", f"Dossier source: {SRC_DIR}")
    _log("DEBUG", f"Dossier de sortie: {OUT_DIR}")
    t0 = time.perf_counter()

    if not SRC_DIR.exists():
        _log("ERROR", f"Dossier source introuvable: {SRC_DIR}")
        sys.exit(1)
    OUT_DIR.mkdir(parents=True, exist_ok=True)

    warn_missing_tools()

    sources = [p for p in sorted(SRC_DIR.rglob("*"))
               if p.is_file() and p.suffix.lower() in EXTS]
    if sources:
        _log("INFO", f"Fichiers détectés ({len(sources)}):")
        for p in sources:
            _log("INFO", f"  - {p.relative_to(SRC_DIR).as_posix()}")
    else:
        _log("INFO", "Aucun fichier détecté.")

    assets = [build_asset(p) for p in sources]

    if not check_collisions(assets):
        _log("ERROR", "Génération annulée (renomme les fichiers en conflit).")
        sys.exit(1)

    total = sum(len(a.payload) for a in assets)
    _log("INFO", f"Total embarqué: {total} octets ({round(total / 1024, 1)} Ko) pour {len(assets)} fichiers")

    wrote_h = write_if_changed(OUT_DIR / ASSETS_H, generate_assets_header(assets))
    wrote_cpp = write_if_changed(OUT_DIR / ASSETS_CPP, generate_assets_cpp(assets))

    dt = round(time.perf_counter() - t0, 2)
    if wrote_h or wrote_cpp:
        _log("OK", f"Génération terminée en {dt} s")
    else:
        _log("OK", f"Aucun changement. Terminé en {dt} s")


def pio_entry():
    """Génère les ressources lorsque PlatformIO charge ce script (extra_scripts)."""
    _log("INFO", "Exécution automatique via PlatformIO Build System")
    main()


if __name__ == "__main__":
    main()
else:
    pio_entry()