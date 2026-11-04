import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
CONTROLLER_DATA = ROOT / "src" / "Data" / "ControllerData.cpp"
WEB_HANDLERS = ROOT / "src" / "ServerWeb" / "WebHandlers.cpp"
WEB_SECURITY = ROOT / "src" / "ServerWeb" / "WebSecurity.cpp"
MQTT_PAGE = ROOT / "web" / "Mqtt.html"


class WebSecurityContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.controller_data = CONTROLLER_DATA.read_text(encoding="utf-8")
        cls.web_handlers = WEB_HANDLERS.read_text(encoding="utf-8")
        cls.web_security = WEB_SECURITY.read_text(encoding="utf-8")
        cls.mqtt_page = MQTT_PAGE.read_text(encoding="utf-8")

    def test_mqtt_password_is_not_in_global_status(self):
        mqtt_json = self.controller_data.split(
            'JsonObject Mqtt = doc["Mqtt"]', 1
        )[1].split("JsonObject ", 1)[0]

        self.assertNotIn("MQTTPASSWORD", mqtt_json)

    def test_mqtt_password_is_write_only_and_can_be_cleared_explicitly(self):
        save_handler = self.web_handlers.split(
            "void handleSaveMqtt", 1
        )[1].split("void handleConfigureAdmin", 1)[0]

        self.assertIn('MqttPassword.value   = "";', self.mqtt_page)
        self.assertIn('ClearMqttPassword.checked = false;', self.mqtt_page)
        self.assertIn('clearPasswordText == "1"', save_handler)
        self.assertIn('DataMqtt.MqttPassword = "";', save_handler)
        self.assertIn("else if (!MqttPassword.isEmpty())", save_handler)

    def test_web_routes_require_authentication_and_mutations_use_post(self):
        setup = self.web_handlers.split("void setupWebHandlers()", 1)[1]

        self.assertIn('server.on("/RestartESP", HTTP_POST', setup)
        self.assertIn('server.on("/Scan", HTTP_POST', setup)
        self.assertIn("AddWebSecurityMiddleware(server);", setup)
        self.assertIn("request->authenticate(", self.web_security)
        self.assertIn("request->requestAuthentication(", self.web_security)
        self.assertIn("request->isWebSocketUpgrade()", self.web_security)

    def test_ap_password_is_generated_and_persisted_without_fixed_default(self):
        self.assertIn("esp_fill_random", self.web_security)
        self.assertIn("securityPreferences.putString(ApPasswordKey, apPassword)", self.web_security)
        self.assertNotIn("123456789", self.web_security)


if __name__ == "__main__":
    unittest.main()
