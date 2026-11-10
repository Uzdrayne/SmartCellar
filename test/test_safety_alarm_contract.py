import unittest
from pathlib import Path

from ws_sources import read_websocket_api


ROOT = Path(__file__).resolve().parents[1]
ALARM_SOURCE = ROOT / "src" / "Alarm" / "SafetyAlarm.cpp"
ALARM_HEADER = ROOT / "src" / "Alarm" / "SafetyAlarm.h"
CONFIGURATION_LIMITS = ROOT / "src" / "Controller" / "ConfigurationLimits.h"
WEBSOCKET_API = ROOT / "src" / "ServerWeb" / "WebSocketApi.cpp"
MQTT_SOURCE = ROOT / "src" / "WifiAndMqtt" / "my_MQTT.cpp"
MQTT_LIBRARY_SOURCE = ROOT / "lib" / "HomeAssistantMQTT" / "HomeAssistantMQTT.cpp"
ALARM_PAGE = ROOT / "web" / "Alarm.html"


class SafetyAlarmContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.alarm_source = ALARM_SOURCE.read_text(encoding="utf-8")
        cls.alarm_header = ALARM_HEADER.read_text(encoding="utf-8")
        cls.configuration_limits = CONFIGURATION_LIMITS.read_text(encoding="utf-8")
        cls.websocket_api = read_websocket_api()
        cls.mqtt_source = MQTT_SOURCE.read_text(encoding="utf-8")
        cls.mqtt_library_source = MQTT_LIBRARY_SOURCE.read_text(encoding="utf-8")
        cls.alarm_page = ALARM_PAGE.read_text(encoding="utf-8")

    def test_temperature_alarm_defaults_and_hysteresis_timing(self):
        self.assertIn("SafetyAlarmThresholdDefaultC = 25.0f", self.alarm_header)
        self.assertIn("SafetyAlarmThresholdMinC = 0.0f", self.configuration_limits)
        self.assertIn("SafetyAlarmThresholdMaxC = 40.0f", self.configuration_limits)
        self.assertIn("SafetyAlarmTriggerDelaySeconds = 300", self.alarm_header)
        self.assertIn("SafetyAlarmRecoveryDelaySeconds = 30", self.alarm_header)
        self.assertIn("correctedTemperature > alarmConfiguration.HighTemperatureThresholdC",
                      self.alarm_source)
        self.assertIn("correctedTemperature < alarmConfiguration.HighTemperatureThresholdC",
                      self.alarm_source)

    def test_alarm_settings_are_persisted_and_websocket_actions_are_validated(self):
        self.assertIn('preferences.putFloat(HighTemperatureThresholdKey, thresholdC)',
                      self.alarm_source)
        self.assertIn('{"getAlarmSettings", handle', self.websocket_api)
        self.assertIn('{"setAlarmThreshold", handle', self.websocket_api)
        self.assertIn("floatValue < SafetyAlarmThresholdMinC", self.websocket_api)
        self.assertIn("floatValue > SafetyAlarmThresholdMaxC", self.websocket_api)
        self.assertIn("SmartCellarWebSocket.onState(", self.alarm_page)
        self.assertNotIn("setInterval", self.alarm_page)
        self.assertIn('request("setAlarmThreshold"', self.alarm_page)
        self.assertIn('id="HighTemperatureThreshold"', self.alarm_page)

    def test_home_assistant_exposes_alert_and_last_valid_measurement(self):
        self.assertIn('{"binary_sensor", "DeviceAlert", nullptr, "problem"', self.mqtt_source)
        self.assertIn('"AlertStatus"', self.mqtt_source)
        self.assertIn('"LastValidTemperature"', self.mqtt_source)
        self.assertIn('"LastMeasurementAge"', self.mqtt_source)

    def test_alert_status_is_discovered_as_a_text_sensor(self):
        # Capteur texte : ni classe, ni unité, ni précision d'affichage.
        self.assertIn(
            '{{"sensor", "AlertStatus", nullptr, nullptr, nullptr, '
            '"mdi:alert-circle-outline", nullptr, false, false}',
            self.mqtt_source,
        )
        self.assertIn('if (entity.displayPrecision) document["suggested_display_precision"] = 2;',
                      self.mqtt_library_source)

    def test_home_assistant_state_is_published_only_on_change(self):
        self.assertIn("if (mqttClient == nullptr || (!valuesDirty && !force)) return false;",
                      self.mqtt_library_source)
        self.assertIn("ForcedPublishIntervalMs = 300000", self.mqtt_source)
        self.assertNotIn("String data =", self.mqtt_library_source)
        self.assertNotIn("new char[", self.mqtt_library_source)

    def test_home_assistant_setpoints_are_persisted(self):
        self.assertIn("temperature ? Save_SetTemp() : Save_SetHum();", self.mqtt_source)


if __name__ == "__main__":
    unittest.main()
