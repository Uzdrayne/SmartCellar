import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SCREENS = ROOT / "src" / "LVGL" / "Screen"


def read(*parts):
    return ROOT.joinpath(*parts).read_text(encoding="utf-8")


class AuditContractTests(unittest.TestCase):
    def test_lvgl_screens_share_one_label_helper(self):
        global_source = read("src", "LVGL", "Lvgl_Global.cpp")
        self.assertIn("void SetLabelTextIfChanged(lv_obj_t* label, const char* text)",
                      global_source)
        for screen in SCREENS.glob("*.cpp"):
            source = screen.read_text(encoding="utf-8")
            self.assertNotIn("lv_label_set_text(", source, screen.name)
            self.assertNotRegex(source, r"void setLabelTextIfChanged\(", screen.name)

    def test_home_screen_does_not_touch_the_mqtt_client_from_lvgl(self):
        home = (SCREENS / "0_ScreenHome.cpp").read_text(encoding="utf-8")
        self.assertNotIn("mqtt.connected()", home)
        self.assertIn("Get_MqttConnected()", home)
        self.assertNotRegex(home, r"\bsprintf\(")
        self.assertNotIn("bufftext", read("src", "Global", "Global.h"))

    def test_controller_has_no_dead_debug_buffers(self):
        controller = read("src", "Controller", "CaveController.cpp")
        self.assertNotIn("PrintControllerMsgs", controller)
        self.assertNotIn("N_ColdDebugMsg", controller)
        self.assertEqual(len(re.findall(r"message\[messageSize - 1\] = '\\0';", controller)), 1)

    def test_r0_relays_are_adopted_from_nano_before_first_command(self):
        controller = read("src", "Controller", "CaveController.cpp")
        update = controller.split("void UpdateController() {", 1)[1]
        adopt = update.index("adoptRelayStates(LocalData, now);")
        self.assertLess(adopt, update.index("SendRelayOutputs();"))
        self.assertIn("data.ColdState = data.RelayGpioCold;", controller)
        self.assertIn("RelayAdoptionTimeoutMs = 15000UL", controller)

    def test_r1_debug_messages_are_not_in_controller_snapshot(self):
        header = read("src", "Data", "ControllerData.h")
        snapshot = header.split("struct StrctDataController {", 1)[1].split("};", 1)[0]
        self.assertNotIn("DebugMsg", snapshot)
        self.assertIn("extern ControllerDebugMessages DataDebugMessages;", header)

    def test_r5_measurement_limits_are_centralized(self):
        for path in (("src", "Controller", "CaveController.cpp"),
                     ("src", "SerialLink", "SerialLink.cpp"),
                     ("src", "Alarm", "SafetyAlarm.cpp"),
                     ("src", "LVGL", "Lvgl_Global.cpp")):
            source = read(*path)
            self.assertNotIn("-40.0f", source, path[-1])
            self.assertNotIn("80.0f", source, path[-1])
        self.assertIn("MaxMeasuredTemperatureC = 80.0f", read("src", "Controller", "ConfigurationLimits.h"))

    def test_r6_fan_buttons_are_updated_only_on_change(self):
        fan = (SCREENS / "3_ScreenFan.cpp").read_text(encoding="utf-8")
        self.assertNotIn("lv_imgbtn_set_state(", fan)
        self.assertIn("SetImgButtonEnabled(ui_FanButtonApplyChanges", fan)

    def test_r4_websocket_actions_are_dispatched_by_domain_tables(self):
        api = read("src", "ServerWeb", "WebSocketApi.cpp")
        self.assertNotIn('if (action == "', api)
        self.assertIn("table.actions[i].handler(cmd);", api)
        actions = 0
        for name in ("WsSettingsCommands.cpp", "WsAlertCommands.cpp", "WsNetworkCommands.cpp"):
            actions += len(re.findall(r'^    \{"\w+", handle\w+\},$',
                                      read("src", "ServerWeb", name), re.M))
        self.assertEqual(actions, 15)


if __name__ == "__main__":
    unittest.main()
