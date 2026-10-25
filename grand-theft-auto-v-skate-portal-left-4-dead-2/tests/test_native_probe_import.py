import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from import_native_probe import parse_capture, validate_capture_counts


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

    def test_count_includes_independent_sentinel_lookups(self):
        # v2 inventories 52 rows; v3 adds the frame and script-name natives.
        for native_count in (52, 54):
            with self.subTest(native_count=native_count):
                validate_capture_counts({"nativeCount": native_count,
                                         "resolvedCount": native_count,
                                         "executableMemoryChecks": native_count + 2}, native_count)

    def test_missing_extra_and_partial_checks_rejected(self):
        for checks in (0, 54, 55, 57):
            with self.subTest(checks=checks), self.assertRaisesRegex(ValueError, "preliminary sentinel"):
                validate_capture_counts({"nativeCount": 54, "resolvedCount": 54,
                                         "executableMemoryChecks": checks}, 54)
        for field in ("nativeCount", "resolvedCount"):
            manifest = {"nativeCount": 54, "resolvedCount": 54, "executableMemoryChecks": 56}
            manifest[field] = 53
            with self.subTest(field=field), self.assertRaisesRegex(ValueError, "Native counts"):
                validate_capture_counts(manifest, 54)


if __name__ == "__main__":
    unittest.main()
