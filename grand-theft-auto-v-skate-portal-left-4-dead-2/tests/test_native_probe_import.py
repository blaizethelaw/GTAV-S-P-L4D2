import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from import_native_probe import parse_capture


class NativeCaptureValidation(unittest.TestCase):
    sections = [{"executable": True, "rva": 0x1000, "virtualSize": 0x2000000},
                {"executable": False, "rva": 0x3000000, "virtualSize": 0x1000}]
    valid = ("name,hash,rva,status\n"
             "WAIT,4EDE34FBADD967A6,1678AC8,ok\n"
             "PLAYER_PED_ID,D80958FC74E988A6,CE6BE8,ok\n")

    def test_supported_sentinels(self):
        self.assertEqual(len(parse_capture(self.valid, self.sections)), 2)

    def test_wrong_version_addresses_rejected(self):
        with self.assertRaisesRegex(ValueError, "sentinel"):
            parse_capture(self.valid.replace("CE6BE8", "CE6BE0"), self.sections)

    def test_non_executable_memory_rejected(self):
        with self.assertRaisesRegex(ValueError, "outside executable"):
            parse_capture(self.valid.replace("CE6BE8", "3000001"), self.sections)

    def test_duplicate_hash_and_missing_native_rejected(self):
        for value in (self.valid + "FAKE,D80958FC74E988A6,CE6BE8,ok\n",
                      self.valid.replace(",ok", ",missing", 1),
                      self.valid.split("PLAYER_PED_ID")[0]):
            with self.subTest(value=value), self.assertRaises(ValueError):
                parse_capture(value, self.sections)


if __name__ == "__main__":
    unittest.main()
