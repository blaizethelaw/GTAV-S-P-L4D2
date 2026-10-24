import importlib.util
from pathlib import Path
import struct
import unittest

spec = importlib.util.spec_from_file_location("runtime_audit", Path(__file__).parents[1] / "tools/runtime_audit.py")
audit = importlib.util.module_from_spec(spec)
spec.loader.exec_module(audit)


class RuntimeAuditTests(unittest.TestCase):
    def test_wildcards_binary_literals_and_overlapping_matches(self):
        self.assertEqual(audit.find_pattern(b"\x48\x00\x8b\x48\x0a\x8b", "48 ? 8B"), [0, 3])
        self.assertEqual(audit.find_pattern(b"\xaa\xaa\xaa", "AA AA"), [0, 1])
        self.assertEqual(audit.find_pattern(b".+*", "2E 2B 2A"), [0])

    def test_invalid_signatures_fail_closed(self):
        for bad in ("", "? ??", "GG", "123", "[0]"):
            with self.subTest(signature=bad), self.assertRaises(ValueError):
                audit.find_pattern(b"", bad)

    def test_pe_offsets_are_not_assumed_equal_to_rvas(self):
        data = bytearray(0x220)
        data[:2] = b"MZ"
        struct.pack_into("<I", data, 0x3C, 0x80)
        data[0x80:0x84] = b"PE\0\0"
        struct.pack_into("<HH", data, 0x84, 0x8664, 1)
        struct.pack_into("<H", data, 0x94, 0x70)
        header = 0x80 + 24 + 0x70
        data[header:header + 8] = b".text\0\0\0"
        struct.pack_into("<IIII", data, header + 8, 0x20, 0x1000, 0x20, 0x200)
        struct.pack_into("<I", data, header + 36, 0x60000020)
        sections = audit.pe_sections(bytes(data))
        self.assertEqual(sections[0]["rawOffset"], 0x200)
        self.assertEqual(sections[0]["rva"], 0x1000)
        self.assertTrue(sections[0]["executable"])
        with self.assertRaises(ValueError):
            audit.pe_sections(bytes(data[:-1]))


if __name__ == "__main__":
    unittest.main()
