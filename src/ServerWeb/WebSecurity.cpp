#include "WebSecurity.h"

#include <Preferences.h>
#include <WiFi.h>
#include <esp_random.h>

#include "Data/ControllerData.h"

namespace {

constexpr char WebSecurityNamespace[] = "WebSecurity";
constexpr char ApPasswordKey[] = "ApPass";
constexpr char AdminPasswordKey[] = "AdminPass";
constexpr char AdminUsername[] = "admin";
constexpr char AuthenticationRealm[] = "SmartCellar";
constexpr char ApPasswordAlphabet[] =
    "ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz23456789";
constexpr size_t ApPasswordLength = 12;

String adminPassword;

String getAdminPassword() {
    xSemaphoreTake(DataMutex, portMAX_DELAY);
    String password = adminPassword;
    xSemaphoreGive(DataMutex);
    return password;
}

void setAdminPassword(const String& password) {
    xSemaphoreTake(DataMutex, portMAX_DELAY);
    adminPassword = password;
    xSemaphoreGive(DataMutex);
}

String generateApPassword() {
    String password;
    password.reserve(ApPasswordLength);
    constexpr uint8_t acceptedByteLimit =
        (256 / (sizeof(ApPasswordAlphabet) - 1)) *
        (sizeof(ApPasswordAlphabet) - 1);

    while (password.length() < ApPasswordLength) {
        uint8_t randomByte;
        esp_fill_random(&randomByte, sizeof(randomByte));
        if (randomByte < acceptedByteLimit) {
            password += ApPasswordAlphabet[
                randomByte % (sizeof(ApPasswordAlphabet) - 1)];
        }
    }

    return password;
}

bool isAccessPointActive() {
    return (WiFi.getMode() & WIFI_AP) != 0;
}

bool isSetupRequest(const AsyncWebServerRequest* request) {
    return (request->url() == "/AdminSetup.html" &&
            request->method() == HTTP_GET) ||
           (request->url() == "/ConfigureAdmin" &&
            request->method() == HTTP_POST);
}

bool isCaptiveProbePath(const String& path) {
    return path == "/connecttest.txt" ||
           path == "/ncsi.txt" ||
           path == "/generate_204" ||
           path == "/gen_204" ||
           path == "/hotspot-detect.html";
}

bool hasSameOrigin(const AsyncWebServerRequest* request) {
    if (!request->hasHeader("Origin")) {
        return false;
    }

    String host = request->host();
    if (host.endsWith(":80")) {
        host.remove(host.length() - 3);
    }

    const String expectedOrigin = "http://" + host;
    return request->getHeader("Origin")->value().equalsIgnoreCase(expectedOrigin);
}

}

bool InitializeWebSecurity(String& apPassword) {
    Preferences securityPreferences;
    if (!securityPreferences.begin(WebSecurityNamespace, false)) {
        Serial.println("[WebSecurity] Impossible d'ouvrir les paramètres de sécurité NVS");
        return false;
    }

    setAdminPassword(securityPreferences.getString(AdminPasswordKey, ""));
    apPassword = securityPreferences.getString(ApPasswordKey, "");
    if (apPassword.length() != ApPasswordLength) {
        if (!WiFi.mode(WIFI_STA)) {
            securityPreferences.end();
            apPassword = "";
            Serial.println("[WebSecurity] Impossible d'activer le Wi-Fi pour générer le mot de passe AP");
            return false;
        }
        apPassword = generateApPassword();
        const size_t written =
            securityPreferences.putString(ApPasswordKey, apPassword);
        if (written != apPassword.length()) {
            securityPreferences.end();
            apPassword = "";
            Serial.println("[WebSecurity] Échec de sauvegarde du mot de passe AP");
            return false;
        }
    }

    securityPreferences.end();
    return true;
}

bool HasWebAdminCredential() {
    return !getAdminPassword().isEmpty();
}

bool SaveWebAdminCredential(const String& password) {
    if (password.length() < 12) {
        return false;
    }

    Preferences securityPreferences;
    if (!securityPreferences.begin(WebSecurityNamespace, false)) {
        Serial.println("[WebSecurity] Impossible d'ouvrir les paramètres de sécurité NVS");
        return false;
    }

    const size_t written =
        securityPreferences.putString(AdminPasswordKey, password);
    securityPreferences.end();
    if (written != password.length()) {
        Serial.println("[WebSecurity] Échec de sauvegarde du mot de passe administrateur");
        return false;
    }

    setAdminPassword(password);
    return true;
}

bool ResetWebAdminCredential() {
    Preferences securityPreferences;
    if (!securityPreferences.begin(WebSecurityNamespace, false)) {
        Serial.println("[WebSecurity] Impossible d'ouvrir les paramètres de sécurité NVS");
        return false;
    }

    const bool removed = !securityPreferences.isKey(AdminPasswordKey) ||
                         securityPreferences.remove(AdminPasswordKey);
    securityPreferences.end();
    if (!removed) {
        Serial.println("[WebSecurity] Échec de réinitialisation du compte administrateur");
        return false;
    }

    setAdminPassword("");
    return true;
}

bool IsWebAdminSetupRequestAllowed(AsyncWebServerRequest* request) {
    return request != nullptr &&
           !HasWebAdminCredential() &&
           isAccessPointActive() &&
           isSetupRequest(request);
}

void AddWebSecurityMiddleware(AsyncWebServer& webServer) {
    webServer.addMiddleware([](AsyncWebServerRequest* request,
                               ArMiddlewareNext next) {
        const String path = request->url();
        if (isCaptiveProbePath(path)) {
            next();
            return;
        }

        if (IsWebAdminSetupRequestAllowed(request)) {
            if (request->method() == HTTP_POST && !hasSameOrigin(request)) {
                request->send(403, "text/plain", "Origine de requête refusée.");
                return;
            }
            next();
            return;
        }

        const String password = getAdminPassword();
        if (password.isEmpty()) {
            if (isAccessPointActive() &&
                (path == "/style.css" || path == "/favicon.ico")) {
                next();
                return;
            }
            if (isAccessPointActive() && request->method() == HTTP_GET) {
                request->redirect("/AdminSetup.html");
                return;
            }
            request->send(
                503, "text/plain",
                "Compte Web non configuré. Activez le point d'accès depuis l'écran Wi-Fi.");
            return;
        }

        if (!request->authenticate(
                AdminUsername, password.c_str(), AuthenticationRealm, false)) {
            request->requestAuthentication(
                AsyncAuthType::AUTH_BASIC, AuthenticationRealm);
            return;
        }

        if ((request->method() == HTTP_POST || request->isWebSocketUpgrade()) &&
            !hasSameOrigin(request)) {
            request->send(403, "text/plain", "Origine de requête refusée.");
            return;
        }

        next();
    });
}
