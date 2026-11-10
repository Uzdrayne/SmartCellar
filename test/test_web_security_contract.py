import unittest
from pathlib import Path

from ws_sources import read_websocket_api


ROOT = Path(__file__).resolve().parents[1]
CONTROLLER_DATA = ROOT / "src" / "Data" / "ControllerData.cpp"
WEB_HANDLERS = ROOT / "src" / "ServerWeb" / "WebHandlers.cpp"
WEB_SECURITY = ROOT / "src" / "ServerWeb" / "WebSecurity.cpp"
WEBSOCKET_API = ROOT / "src" / "ServerWeb" / "WebSocketApi.cpp"
WEB_DIR = ROOT / "web"
WEB_ASSET_GENERATOR = ROOT / "Tools" / "WebtoCPP_Gzip.py"
MQTT_PAGE = WEB_DIR / "Mqtt.html"
WEBSOCKET_CLIENT = WEB_DIR / "WebSocketClient.js"


class WebSecurityContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.controller_data = CONTROLLER_DATA.read_text(encoding="utf-8")
        cls.web_handlers = WEB_HANDLERS.read_text(encoding="utf-8")
        cls.web_security = WEB_SECURITY.read_text(encoding="utf-8")
        cls.websocket_api = read_websocket_api()
        cls.web_asset_generator = WEB_ASSET_GENERATOR.read_text(encoding="utf-8")
        cls.mqtt_page = MQTT_PAGE.read_text(encoding="utf-8")
        cls.websocket_client = WEBSOCKET_CLIENT.read_text(encoding="utf-8")

    def test_mqtt_password_is_not_in_global_status(self):
        mqtt_json = self.controller_data.split(
            'JsonObject Mqtt = doc["Mqtt"]', 1
        )[1].split("JsonObject ", 1)[0]

        self.assertNotIn("MQTTPASSWORD", mqtt_json)

    def test_mqtt_password_is_write_only_and_can_be_cleared_explicitly(self):
        save_handler = self.websocket_api.split(
            'void handleSaveMqttSettings(const WsCommand& cmd)', 1
        )[1].split('void handleRestartDevice(const WsCommand& cmd)', 1)[0]

        self.assertIn('MqttPassword.value   = "";', self.mqtt_page)
        self.assertIn('ClearMqttPassword.checked = false;', self.mqtt_page)
        self.assertIn("if (MqttPassword.value) payload.password", self.mqtt_page)
        self.assertIn('readBoolean(cmd.payload, "clearPassword", clearPassword)', save_handler)
        self.assertIn('DataMqtt.MqttPassword = "";', save_handler)
        self.assertIn("else if (!password.isEmpty())", save_handler)

    def test_only_initial_admin_setup_remains_an_http_post(self):
        setup = self.web_handlers.split("bool setupWebHandlers()", 1)[1]

        self.assertIn('server.on("/ConfigureAdmin", HTTP_POST', setup)
        for route in (
            "/GlobalStatus", "/AlarmSettings", "/SaveConsignes", "/SaveWifi",
            "/SaveMqtt", "/SaveFanParameters", "/SaveAlarmSettings",
            "/RestartESP", "/Scan", "/GetScanResults",
        ):
            self.assertNotIn(route, self.web_handlers)
        self.assertIn("InitializeWebSocketApi()", setup)
        self.assertIn("AddWebSecurityMiddleware(server);", setup)
        self.assertIn("request->authenticate(", self.web_security)
        self.assertIn("request->requestAuthentication(", self.web_security)
        self.assertIn("request->isWebSocketUpgrade()", self.web_security)

    def test_operational_pages_use_the_shared_websocket_client(self):
        operational_pages = [
            path for path in WEB_DIR.glob("*.html")
            if path.name != "AdminSetup.html"
        ]
        self.assertTrue(operational_pages)
        for page in operational_pages:
            contents = page.read_text(encoding="utf-8")
            self.assertIn("WebSocketClient.js", contents, page.name)
            self.assertNotIn("fetch(", contents, page.name)

        self.assertIn("new WebSocket(", self.websocket_client)
        for action in (
            "setTemperature", "setHumidity", "getAlarmSettings",
            "getWifiScanResults", "scanWifi", "saveWifiSettings",
            "saveMqttSettings", "saveSensorCalibration", "setRelaySettings",
            "saveFanSettings", "setAlarmThreshold", "restartDevice",
            "getLimits", "getAlertLog", "clearAlertLog",
        ):
            self.assertIn(f'"{action}"', self.websocket_api)

    def test_embedded_web_assets_are_not_cached_across_firmware_updates(self):
        self.assertIn(
            'response->addHeader("Cache-Control", "no-store");',
            self.web_asset_generator,
        )

    def test_ap_password_is_generated_and_persisted_without_fixed_default(self):
        self.assertIn("esp_fill_random", self.web_security)
        self.assertIn(
            "securityPreferences.putString(ApPasswordKey, apPassword)",
            self.web_security,
        )
        self.assertNotIn("123456789", self.web_security)


if __name__ == "__main__":
    unittest.main()
