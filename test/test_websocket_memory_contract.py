import gzip
import re
import unittest
from pathlib import Path

from ws_sources import read_websocket_api


ROOT = Path(__file__).resolve().parents[1]
WEBSOCKET_API = ROOT / "src" / "ServerWeb" / "WebSocketApi.cpp"
WEBSOCKET_CLIENT = ROOT / "web" / "WebSocketClient.js"
MENU_CLIENT = ROOT / "web" / "Menu.js"
EMBEDDED_ASSETS = ROOT / "src" / "ServerWeb" / "PageWeb" / "web_assets.cpp"
SERIAL_LINK = ROOT / "src" / "SerialLink" / "SerialLink.cpp"
MAIN = ROOT / "src" / "main.cpp"
PLATFORMIO = ROOT / "platformio.ini"


class WebSocketMemoryContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.websocket_api = read_websocket_api()
        cls.websocket_client = WEBSOCKET_CLIENT.read_text(encoding="utf-8")
        cls.menu_client = MENU_CLIENT.read_text(encoding="utf-8")
        cls.embedded_assets = EMBEDDED_ASSETS.read_text(encoding="utf-8")
        cls.serial_link = SERIAL_LINK.read_text(encoding="utf-8")
        cls.main = MAIN.read_text(encoding="utf-8")
        cls.platformio = PLATFORMIO.read_text(encoding="utf-8")

    def test_state_frames_are_serialized_once_into_a_shared_buffer(self):
        self.assertIn("AsyncWebSocketSharedBuffer buildStateFrame()", self.websocket_api)
        self.assertIn("FillGlobalStatus(document[\"payload\"]", self.websocket_api)
        self.assertIn("serializeJson(document, buffer->data()", self.websocket_api)
        self.assertNotIn("JSON_GLOBAL_STATUS", self.websocket_api)
        self.assertNotIn("stateFrame[", self.websocket_api)
        controller_data = (ROOT / "src" / "Data" / "ControllerData.cpp").read_text(encoding="utf-8")
        self.assertNotIn("JSON_GLOBAL_STATUS", controller_data)
        self.assertNotIn("String Ssid = WiFi.SSID()", controller_data)

    def test_state_broadcast_guards_queue_and_heap_allocation(self):
        self.assertIn("client.queueLen() == 0", self.websocket_api)
        self.assertIn(
            "heap_caps_get_largest_free_block(MALLOC_CAP_8BIT)",
            self.websocket_api,
        )
        self.assertIn("catch (const std::bad_alloc&)", self.websocket_api)
        self.assertIn("-DWS_MAX_QUEUED_MESSAGES=4", self.platformio)

    def test_commands_use_frame_opcode_not_unset_message_opcode(self):
        self.assertIn("info->opcode != WS_TEXT", self.websocket_api)
        self.assertNotIn("info->message_opcode", self.websocket_api)

    def test_state_listeners_are_deferred_and_isolated(self):
        on_state = self.websocket_client.split("function onState(listener)", 1)[1]
        on_state = on_state.split("\n  }\n", 1)[0]
        self.assertIn("global.setTimeout(", on_state)
        self.assertIn("callListener(listener, lastState)", self.websocket_client)
        self.assertIn("try {\n      listener(value);", self.websocket_client)

    def test_limits_are_sent_once_and_merged_by_the_client(self):
        self.assertIn('{"getLimits", handle', self.websocket_api)
        status = (ROOT / "src" / "Data" / "ControllerData.cpp").read_text(encoding="utf-8")
        fill_status = status.split("void FillGlobalStatus(JsonObject doc)", 1)[1]
        fill_status = fill_status.split("\n}\n", 1)[0]
        self.assertNotIn('"Limits"', fill_status)
        self.assertIn('request("getLimits"', self.websocket_client)
        self.assertIn("Limits: limits", self.websocket_client)

    def test_wifi_scan_tolerates_framework_six_second_timeout(self):
        self.assertIn("WifiScanTimeoutMs = 15000", self.websocket_api)
        self.assertIn("millis() - scanStartedAt < WifiScanTimeoutMs", self.websocket_api)
        wifi_page = (ROOT / "web" / "Wifi.html").read_text(encoding="utf-8")
        self.assertIn('<form class="card" id="WifiForm"', wifi_page)
        self.assertIn('type="button" class="btn btn-blue" id="ScanBtn"', wifi_page)
        self.assertIn('autocomplete="new-password"', wifi_page)
        self.assertIn('id="SsidUsername" autocomplete="username"', wifi_page)

    def test_uart_json_parsing_uses_mutable_input_and_reports_heap(self):
        self.assertIn("void processTelemetry(char *line", self.serial_link)
        self.assertIn("deserializeJson(message, line)", self.serial_link)
        self.assertIn("heap_caps_get_largest_free_block(MALLOC_CAP_8BIT)",
                      self.serial_link)

    def test_state_is_built_only_for_connected_clients(self):
        self.assertNotIn("Get_Json_GlobalStatus", self.main)
        self.assertIn("ProcessWebSocketApi();", self.main)
        wifi = (ROOT / "src" / "Wifi" / "my_wifi.cpp").read_text(encoding="utf-8")
        send = wifi.split("void SendValueWebSocket()", 1)[1].split("\n}\n", 1)[0]
        self.assertIn("ws.count() == 0", send)
        self.assertIn("TakeWebSocketStateRequest()", send)

    def test_lvgl_pool_leaves_heap_for_network_stack(self):
        lv_conf = (ROOT / "lib" / "Lvgl" / "lv_conf.h").read_text(encoding="utf-8")
        match = re.search(r"^\s*#define LV_MEM_SIZE \((\d+)U\*1024U\)", lv_conf, re.M)
        self.assertIsNotNone(match)
        self.assertLessEqual(int(match.group(1)), 64)
        self.assertIn("[MEM] heap libre", self.main)

    def test_websocket_failures_are_reported_in_the_page(self):
        self.assertIn("websocket-connection-banner", self.websocket_client)
        self.assertIn("aria-live", self.websocket_client)
        self.assertIn("reconnexion automatique en cours", self.websocket_client)
        self.assertIn("Client WebSocket non chargé", self.menu_client)
        self.assertIn("window.SmartCellarWebSocket =", self.menu_client)

    def test_embedded_websocket_client_is_a_valid_gzip_response(self):
        declaration = "const uint8_t WebSocketClient_js[] WEB_STORAGE_ATTR = {"
        start = self.embedded_assets.index(declaration) + len(declaration)
        end = self.embedded_assets.index("};", start)
        compressed = bytes(
            int(value, 16)
            for value in re.findall(
                r"0x([0-9A-Fa-f]{2})", self.embedded_assets[start:end]
            )
        )
        self.assertIn(b"SmartCellarWebSocket", gzip.decompress(compressed))

        route = self.embedded_assets.split(
            'server.on("/WebSocketClient.js"', 1
        )[1].split('server.on("/Wifi.html"', 1)[0]
        self.assertIn("WEB::WebSocketClient_js_len", route)
        self.assertIn('addHeader("Content-Encoding", "gzip")', route)


if __name__ == "__main__":
    unittest.main()
