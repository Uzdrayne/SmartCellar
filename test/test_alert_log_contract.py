import re
import unittest
from pathlib import Path

from ws_sources import read_websocket_api


ROOT = Path(__file__).resolve().parents[1]


def read(*parts):
    return (ROOT.joinpath(*parts)).read_text(encoding="utf-8")


class AlertLogContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.log_header = read("src", "Alarm", "AlertLog.h")
        cls.log_source = read("src", "Alarm", "AlertLog.cpp")
        cls.manager_source = read("src", "Alarm", "AlertManager.cpp")
        cls.manager_header = read("src", "Alarm", "AlertManager.h")
        cls.screen_source = read("src", "LVGL", "Screen", "5_ScreenWarning.cpp")
        cls.status_source = read("src", "Data", "ControllerData.cpp")
        cls.websocket_api = read_websocket_api()
        cls.menu_html = read("web", "Menu.html")
        cls.menu_js = read("web", "Menu.js")

    def test_log_is_bounded_and_merges_repeats(self):
        self.assertIn("AlertLogCapacity = 10", self.log_header)
        self.assertIn("RepeatMergeWindowMs = 5UL * 60UL * 1000UL", self.log_source)
        self.assertIn("StartupGraceMs", self.log_source)
        self.assertIn("ClearEndedAlertLogEntries", self.log_header)

    def test_alert_manager_feeds_the_log_without_duplicate_tracking(self):
        self.assertIn("UpdateAlertLog(status, now);", self.manager_source)
        self.assertNotIn("PrimaryAlertReason", self.manager_header)
        self.assertNotIn("LastPrimaryAlert", self.manager_source)

    def test_screen_reuses_shared_labels_and_keeps_its_texts(self):
        self.assertIn("GetActiveAlertTypes(status", self.screen_source)
        self.assertIn("GetAlertTypeLabel(", self.screen_source)
        self.assertIn("GetLastEndedPrimaryAlert(", self.screen_source)
        self.assertNotIn('"Liaison avec le Nano perdue"', self.screen_source)
        self.assertIn('"Retour a la normale\\nDerniere alerte :\\n%s"', self.screen_source)
        for label in ("Liaison avec le Nano perdue", "Mesures indisponibles",
                      "Temperature trop elevee", "Mode secours du Nano",
                      "Wi-Fi indisponible", "MQTT indisponible"):
            self.assertIn(f'"{label}"', self.log_source)

    def test_state_exposes_revision_epoch_and_shared_reason_codes(self):
        self.assertIn('Alerts["LogRevision"] = GetAlertLogRevision();', self.status_source)
        self.assertIn('doc["EpochSeconds"]', self.status_source)
        self.assertIn("GetAlertTypeCode(activeTypes[i])", self.status_source)
        self.assertNotIn('ActiveReasons.add("nano_link_lost")', self.status_source)

    def test_save_failures_are_journaled(self):
        self.assertIn("RecordAlertEvent(AlertType::SaveFailed)", self.status_source)
        self.assertNotIn("if (!saved) Serial.println", self.status_source)
        self.assertGreaterEqual(
            len(re.findall(r"return reportSaveResult\(", self.status_source)), 10)

    def test_websocket_actions_and_menu_card(self):
        self.assertIn('{"getAlertLog", handle', self.websocket_api)
        self.assertIn('{"clearAlertLog", handle', self.websocket_api)
        self.assertIn('id="AlertLogCard" hidden', self.menu_html)
        self.assertIn('request("getAlertLog"', self.menu_js)
        self.assertIn('request("clearAlertLog"', self.menu_js)
        self.assertIn("state.Alerts.LogRevision", self.menu_js)
        self.assertIn("% 4294967296", self.menu_js)


if __name__ == "__main__":
    unittest.main()
