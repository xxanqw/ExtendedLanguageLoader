import struct
import sys
import unittest
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent.parent/'scripts'))
from encode_extended_gxt import byte_map, recode


def gxt(text):
    return struct.pack('<HH4sI8sI4sIII4sI', 4, 8, b'TABL', 12, b'MAIN\0\0\0\0', 24, b'TKEY', 8, 0, 1, b'TDAT', len(text))+text


class EncodingTests(unittest.TestCase):
    def test_canonical_source_to_target_and_native_tags(self):
        source = {f'0x{b:02X}': chr(b) for b in range(32, 127)}
        source['0x41'] = 'А'
        target = [0]*256
        for b in range(32, 127):
            target[b] = b
        target[0xc0] = ord('А')
        old = gxt(b'A ~r~A~w~\0')
        result, report = recode(old, byte_map(source), byte_map({'mapping':target}))
        self.assertEqual(result[:48], old[:48])
        self.assertEqual(result[48:], b'\xc0 ~r~\xc0~w~\0')
        self.assertEqual(report['changedBytes'], 2)
        # Identity conversion does not mutate even a single byte.
        unchanged, report = recode(old, byte_map(source), byte_map(source))
        self.assertEqual(unchanged, old)
        self.assertEqual(report['changedBytes'], 0)

    def test_unmapped_and_invalid_maps_rejected(self):
        mapping = byte_map({'0x20':' ', '0x41':'А'})
        with self.assertRaises(ValueError):
            recode(gxt(b'B\0'), mapping, mapping)
        for value in ({'0x20':' ', '0x7E':'А'}, {'0x20':' ', '0x01':'A'}, {'0x20':' ', '0x41':'AB'}, {'0x41':'A'}):
            with self.assertRaises(ValueError):
                byte_map(value)
        with self.assertRaises(ValueError):
            recode(gxt(b'A~r\0'), mapping, mapping)


if __name__ == '__main__':
    unittest.main()
