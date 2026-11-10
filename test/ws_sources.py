from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SERVER_WEB = ROOT / "src" / "ServerWeb"


def read_websocket_api():
    """Source complète de l'API WebSocket (enveloppe + commandes par domaine)."""
    files = [SERVER_WEB / "WebSocketApi.cpp", SERVER_WEB / "WsCommands.h"]
    files += sorted(SERVER_WEB.glob("Ws*Commands.cpp"))
    return "\n".join(path.read_text(encoding="utf-8") for path in files)
