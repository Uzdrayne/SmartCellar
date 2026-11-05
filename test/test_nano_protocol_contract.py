import json
import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
NANO_ROOT = ROOT.parent / "SmartController_NanoGPIO"
C3_LINK = ROOT / "src" / "SerialLink" / "SerialLink.cpp"
NANO_LINK = NANO_ROOT / "src" / "NanoSerialLink.cpp"


class NanoProtocolContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.c3_source = C3_LINK.read_text(encoding="utf-8")
        cls.nano_source = NANO_LINK.read_text(encoding="utf-8")

    def test_configuration_fields_match_between_firmwares(self):
        c3_block = self.c3_source.split(
            "void sendControllerConfigurationIfChanged", 1
        )[1].split("void setSensorDataStale", 1)[0]
        c3_keys = re.findall(r'message\["([^"]+)"\]\s*=', c3_block)

        nano_block = self.nano_source.split(
            "bool parseConfiguration(", 1
        )[1].split("void sendConfigurationAck", 1)[0]
        nano_keys = re.findall(r'PSTR\("([^"]+)"\)', nano_block)
        nano_keys += re.findall(
            r'READ_(?:FLOAT|BOOL|UINT16)_FIELD\("([^"]+)"', nano_block
        )

        self.assertEqual(sorted(key for key in c3_keys if key != "type"),
                         sorted(nano_keys))
        self.assertEqual(len(c3_keys), 28)
        self.assertEqual(len(nano_keys), 27)

    def test_buffers_fit_conservative_configuration_bound(self):
        c3_block = self.c3_source.split(
            "void sendControllerConfigurationIfChanged", 1
        )[1].split("void setSensorDataStale", 1)[0]
        keys = re.findall(r'message\["([^"]+)"\]\s*=', c3_block)
        values = {
            key: "controller_config" if key == "type"
            else False if key.endswith("_enabled")
            else 0
            for key in keys
        }
        compact_json = json.dumps(values, separators=(",", ":"))
        encoded_values = sum(
            len(json.dumps(value, separators=(",", ":")))
            for value in values.values()
        )
        fixed_overhead = len(compact_json) - encoded_values

        float_fields = re.findall(
            r'READ_FLOAT_FIELD\("([^"]+)"',
            self.nano_source.split("bool parseConfiguration(", 1)[1]
            .split("void sendConfigurationAck", 1)[0],
        )
        bool_fields = re.findall(
            r'READ_BOOL_FIELD\("([^"]+)"',
            self.nano_source.split("bool parseConfiguration(", 1)[1]
            .split("void sendConfigurationAck", 1)[0],
        )
        uint_fields = re.findall(
            r'READ_UINT16_FIELD\("([^"]+)"',
            self.nano_source.split("bool parseConfiguration(", 1)[1]
            .split("void sendConfigurationAck", 1)[0],
        )
        self.assertEqual((len(float_fields), len(bool_fields), len(uint_fields)),
                         (12, 3, 10))

        maximum_value_bytes = (
            len(json.dumps("controller_config"))
            + 1
            + 10
            + len(float_fields) * 12
            + len(uint_fields) * 4
            + len(bool_fields) * 5
        )
        conservative_bound = fixed_overhead + maximum_value_bytes

        c3_capacity = int(re.search(
            r"MaxConfigurationMessageLength\s*=\s*(\d+)",
            self.c3_source,
        ).group(1))
        nano_capacity = int(re.search(
            r"MaxLineLength\s*=\s*(\d+)",
            self.nano_source,
        ).group(1))

        self.assertEqual(conservative_bound, 721)
        self.assertGreaterEqual(c3_capacity - 1, conservative_bound)
        self.assertGreaterEqual(nano_capacity - 1, conservative_bound)

    def test_fallback_request_and_recovery_events_are_explicit(self):
        self.assertIn("reason", self.nano_source)
        self.assertIn("outputs_timeout", self.nano_source)
        self.assertIn("startup", self.nano_source)
        self.assertIn("control_restored", self.nano_source)
        self.assertIn('strcmp(reason, "outputs_timeout")', self.c3_source)
        self.assertIn('strcmp(type, "control_restored")', self.c3_source)

    def test_c3_emits_periodic_uart_test_marker(self):
        self.assertIn("TestMessagePeriodMs = 1000", self.c3_source)
        self.assertIn('Serial1.println("TEST")', self.c3_source)


if __name__ == "__main__":
    unittest.main()
