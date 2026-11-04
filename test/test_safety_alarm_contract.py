import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
ALARM_SOURCE = ROOT / "src" / "Alarm" / "SafetyAlarm.cpp"
ALARM_HEADER = ROOT / "src" / "Alarm" / "SafetyAlarm.h"
WEB_HANDLERS = ROOT / "src" / "ServerWeb" / "WebHandlers.cpp"
MQTT_SOURCE = ROOT / "src" / "WifiAndMqtt" / "my_MQTT.cpp"
ALARM_PAGE = ROOT / "web" / "Alarm.html"


class SafetyAlarmContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.alarm_source = ALARM_SOURCE.read_text(encoding="utf-8")
        cls.alarm_header = ALARM_HEADER.read_text(encoding="utf-8")
        cls.web_handlers = WEB_HANDLERS.read_text(encoding="utf-8")
        cls.mqtt_source = MQTT_SOURCE.read_text(encoding="utf-8")
        cls.alarm_page = ALARM_PAGE.read_text(encoding="utf-8")

    def test_temperature_alarm_defaults_and_hysteresis_timing(self):
        self.assertIn("SafetyAlarmThresholdDefaultC = 25.0f", self.alarm_header)
        self.assertIn("SafetyAlarmThresholdMinC = 0.0f", self.alarm_header)
        self.assertIn("SafetyAlarmThresholdMaxC = 40.0f", self.alarm_header)
        self.assertIn("SafetyAlarmTriggerDelaySeconds = 300", self.alarm_header)
        self.assertIn("SafetyAlarmRecoveryDelaySeconds = 30", self.alarm_header)
        self.assertIn("correctedTemperature > alarmConfiguration.HighTemperatureThresholdC",
                      self.alarm_source)
        self.assertIn("correctedTemperature < alarmConfiguration.HighTemperatureThresholdC",
                      self.alarm_source)

    def test_alarm_settings_are_persisted_and_web_route_is_validated(self):
        self.assertIn('preferences.putFloat(HighTemperatureThresholdKey, thresholdC)',
                      self.alarm_source)
        self.assertIn('server.on("/AlarmSettings", HTTP_GET', self.web_handlers)
        self.assertIn('server.on("/SaveAlarmSettings", HTTP_POST', self.web_handlers)
        self.assertIn("thresholdC < SafetyAlarmThresholdMinC", self.web_handlers)
        self.assertIn("thresholdC > SafetyAlarmThresholdMaxC", self.web_handlers)
        self.assertIn('id="HighTemperatureThreshold"', self.alarm_page)

    def test_home_assistant_exposes_alert_and_last_valid_measurement(self):
        self.assertIn('"problem", "DeviceAlert"', self.mqtt_source)
        self.assertIn('"AlertStatus"', self.mqtt_source)
        self.assertIn('"LastValidTemperature"', self.mqtt_source)
        self.assertIn('"LastMeasurementAge"', self.mqtt_source)


if __name__ == "__main__":
    unittest.main()
