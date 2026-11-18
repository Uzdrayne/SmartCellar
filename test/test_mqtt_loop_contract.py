import unittest
from pathlib import Path

from ws_sources import read_websocket_api


ROOT = Path(__file__).resolve().parents[1]
MAIN_SOURCE = ROOT / "src" / "main.cpp"
MQTT_SOURCE = ROOT / "src" / "WifiAndMqtt" / "my_MQTT.cpp"
MQTT_LIBRARY_SOURCE = ROOT / "lib" / "HomeAssistantMQTT" / "HomeAssistantMQTT.cpp"
WEBSOCKET_API = ROOT / "src" / "ServerWeb" / "WebSocketApi.cpp"
MQTT_PAGE = ROOT / "web" / "Mqtt.html"


class MqttLoopContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.main_source = MAIN_SOURCE.read_text(encoding="utf-8")
        cls.mqtt_source = MQTT_SOURCE.read_text(encoding="utf-8")
        cls.mqtt_library_source = MQTT_LIBRARY_SOURCE.read_text(encoding="utf-8")
        cls.websocket_api = read_websocket_api()
        cls.mqtt_page = MQTT_PAGE.read_text(encoding="utf-8")

    def test_synchronous_mqtt_work_is_isolated_from_controller_loop(self):
        self.assertNotIn("MyMqttLoop", self.main_source)
        self.assertIn('xTaskCreate(mqttTask, "mqtt", 8192', self.mqtt_source)
        self.assertIn("vTaskDelay(pdMS_TO_TICKS(50))", self.mqtt_source)
        self.assertIn("if (!Get_WifiScanning())", self.mqtt_source)
        self.assertIn("wifiClient.setTimeout(2)", self.mqtt_source)
        self.assertIn("mqttClient->setSocketTimeout(2)", self.mqtt_library_source)
        # PubSubClient garde le pointeur du nom d'hôte : il doit être copié.
        self.assertIn("mqttClient->setServer(serverHost, port);", self.mqtt_library_source)

    def test_mqtt_host_must_be_a_hostname_or_ip_not_a_url(self):
        mqtt_handler = self.websocket_api.split(
            'void handleSaveMqttSettings(const WsCommand& cmd)', 1
        )[1].split('void handleRestartDevice(const WsCommand& cmd)', 1)[0]
        self.assertIn('server.indexOf("://") >= 0', mqtt_handler)
        self.assertIn("sans préfixe mqtt://", self.mqtt_page)

    def test_mqtt_page_subscribes_to_state_and_wraps_password_in_form(self):
        self.assertIn('<form class="card" id="MqttForm"', self.mqtt_page)
        self.assertIn('autocomplete="new-password"', self.mqtt_page)
        undo = self.mqtt_page.split("function UndoMQTT()", 1)[1].split("}", 1)[0]
        self.assertNotIn("onState", undo)
        self.assertIn("SmartCellarWebSocket.onState(LoadMQTT);", self.mqtt_page)

    def test_mqtt_send_failures_report_transport_and_heap_state(self):
        self.assertIn("logMqttPublishFailure", self.mqtt_library_source)
        self.assertIn("client->state()", self.mqtt_library_source)
        self.assertIn("ESP.getFreeHeap()", self.mqtt_library_source)
        self.assertIn(
            "heap_caps_get_largest_free_block(MALLOC_CAP_8BIT)",
            self.mqtt_library_source,
        )

    def test_sensor_states_expire_and_are_not_retained(self):
        self.assertIn("EntityStateExpireAfterSeconds = 600", self.mqtt_library_source)
        self.assertIn("AvailabilityHeartbeatIntervalMs = 30000",
                      self.mqtt_library_source)
        self.assertIn('document["expire_after"] = EntityStateExpireAfterSeconds;',
                      self.mqtt_library_source)
        self.assertIn('mqttClient->publish(stateTopic, "", true)',
                      self.mqtt_library_source)
        self.assertIn('mqttClient->publish(availabilityTopic, "", true)',
                      self.mqtt_library_source)
        self.assertIn(
            'mqttClient->publish(availabilityTopic, OnlineAvailabilityPayload, false)',
            self.mqtt_library_source,
        )
        self.assertIn(
            'publishJson(mqttClient, topic, document, "publication découverte", true);',
            self.mqtt_library_source,
        )
        self.assertIn(
            'publishJson(mqttClient, stateTopic, document, "publication état", false);',
            self.mqtt_library_source,
        )

    def test_temperature_and_humidity_follow_nano_link_availability(self):
        self.assertIn("GetSerialLinkSnapshot(millis()).measurementAvailable",
                      self.mqtt_source)
        self.assertIn("mqtt.setNanoAvailable(snapshot.nanoMeasurementsAvailable);",
                      self.mqtt_source)
        self.assertIn('snprintf(nanoAvailabilityTopic, sizeof(nanoAvailabilityTopic), '
                      '"%s/nano/state", stateTopic);',
                      self.mqtt_library_source)
        self.assertIn('document["availability_mode"] = "all";',
                      self.mqtt_library_source)
        self.assertIn('strcmp(entity.name, "Temperature") == 0', self.mqtt_library_source)
        self.assertIn('strcmp(entity.name, "Humidity") == 0', self.mqtt_library_source)
        self.assertIn('strcmp(entity.component, "binary_sensor") == 0',
                      self.mqtt_library_source)
        self.assertIn('strstr(entity.name, "_Relay") != nullptr',
                      self.mqtt_library_source)


if __name__ == "__main__":
    unittest.main()
