#include "WebHandlers.h"

#include "Data/ControllerData.h"
#include "ServerWeb/PageWeb/web_assets.h"
#include "ServerWeb/WebSecurity.h"
#include "ServerWeb/WebSocketApi.h"

namespace {

void handleConfigureAdmin(AsyncWebServerRequest* request) {
    if (HasWebAdminCredential()) {
        request->send(409, "text/plain",
                      "Le compte administrateur est déjà configuré.");
        return;
    }

    if (!request->hasArg("password") ||
        !request->hasArg("password_confirm")) {
        request->send(400, "text/plain", "Mot de passe manquant.");
        return;
    }

    const String password = request->arg("password");
    if (password.length() < 12 ||
        password != request->arg("password_confirm")) {
        request->send(
            400, "text/plain",
            "Les mots de passe doivent correspondre et contenir au moins 12 caractères.");
        return;
    }

    if (!SaveWebAdminCredential(password)) {
        request->send(500, "text/plain",
                      "Impossible d'enregistrer le compte administrateur.");
        return;
    }

    request->send(200, "text/plain", "Compte administrateur créé.");
}

}

bool setupWebHandlers() {
    if (!InitializeWebSocketApi()) {
        return false;
    }

    AddRoutesToServer(server, "/Wifi.html", "/Menu.html");
    server.on("/ConfigureAdmin", HTTP_POST, handleConfigureAdmin);
    AddWebSecurityMiddleware(server);

    server.begin();
    return true;
}
