"""Portable synthetic tests: no game, translation, font or plugin assets."""
import configparser
import hashlib
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest
from zipfile import ZipFile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'scripts'))
from prepare_languages import normalize, prepare, LOCALES, DEFERRED


def synthetic_gxt():
    keys = struct.pack('<II', 0, 123)
    text = b'Synthetic test\0'
    return (b'\x04\x00\x08\x00TABL' + struct.pack('<I', 12) +
            b'MAIN\0\0\0\0' + struct.pack('<I', 24) +
            b'TKEY' + struct.pack('<I', len(keys)) + keys +
            b'TDAT' + struct.pack('<I', len(text)) + text)


class PortablePreparationTest(unittest.TestCase):
    def test_valid_synthetic_gxt_roundtrip_and_native_validation(self):
        data = synthetic_gxt()
        output, repairs, count = normalize(data)
        self.assertEqual(output, data)
        self.assertEqual(repairs, [])
        self.assertEqual(count, 1)
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / 'synthetic.gxt'; path.write_bytes(data)
            subprocess.run([str(ROOT / 'tests/bin/core_test'), str(path)], check=True)

    def test_truncated_and_invalid_structures(self):
        data = synthetic_gxt()
        for b in (b'', b'\0' * 24, data[:30], data[:-1]):
            with self.assertRaises(ValueError): normalize(b)

    def test_scope_order_and_no_input_edits(self):
        with tempfile.TemporaryDirectory() as tmp:
            source, output = Path(tmp) / 'inputs', Path(tmp) / 'output/text'
            source.mkdir(); output.mkdir(parents=True)
            for name in LOCALES:
                (source / (name + '.gxt')).write_bytes(synthetic_gxt())
            for name in DEFERRED:
                (source / (name + '.gxt')).write_bytes(b'deferred; do not parse')
                (output / (name + '.gxt')).write_bytes(b'stale')
            before = {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in source.iterdir()}
            report = prepare(source, output)
            self.assertEqual(report['project'], 'ExtendedLanguageLoader')
            self.assertEqual(report['release'], '0.4.0')
            self.assertEqual(report['release_channel'], 'beta')
            self.assertEqual(len(report['languages']), 21)
            self.assertEqual(report['excluded_languages'], ['arabic.gxt', 'japanese.gxt', 'korean.gxt'])
            self.assertEqual(len(list(output.glob('*.gxt'))), 21)
            config = configparser.ConfigParser(); config.read(output / 'languages.ini')
            for slot, file in [('6', 'belarusian.gxt'), ('16', 'russian.gxt'), ('21', 'ukrainian.gxt')]:
                self.assertEqual(config[slot]['GXT_file'], file)
            self.assertEqual(before, {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in source.iterdir()})

    def test_source_output_overlap_and_unknown_locales(self):
        with tempfile.TemporaryDirectory() as tmp:
            source = Path(tmp) / 'inputs'; source.mkdir()
            (source / 'english.gxt').write_bytes(synthetic_gxt())
            for output in (source, source / 'out', source.parent):
                with self.assertRaises(ValueError): prepare(source, output)
            (source / 'greek.gxt').write_bytes(synthetic_gxt())
            with self.assertRaises(ValueError): prepare(source, Path(tmp) / 'out')

    def test_only_deferred_inputs_rejected(self):
        with tempfile.TemporaryDirectory() as tmp:
            source = Path(tmp) / 'inputs'; source.mkdir()
            (source / 'arabic.gxt').write_bytes(b'deferred')
            with self.assertRaises(ValueError): prepare(source, Path(tmp) / 'out')

    def test_example_configs_are_consecutive_and_supported(self):
        for name, count in [('languages.example.ini', 5), ('languages-21.example.ini', 21)]:
            config = configparser.ConfigParser(); config.read(ROOT / 'config' / name)
            self.assertEqual(config.getint('MAIN', 'Number_of_languages'), count)
            self.assertEqual(config.sections(), ['MAIN'] + [str(i) for i in range(1, count + 1)])
            for i in range(1, count + 1):
                self.assertIn(config[str(i)]['Font_profile'], ('keep', 'cyrillic', 'ukrainian', 'belarusian'))
                self.assertNotIn(Path(config[str(i)]['GXT_file']).stem, DEFERRED)

    @unittest.skipUnless((ROOT / 'dist/ExtendedLanguageLoader-v0.4.0.zip').exists(), 'run build.sh first for release checks')
    def test_public_release_is_loader_only(self):
        with ZipFile(ROOT / 'dist/ExtendedLanguageLoader-v0.4.0.zip') as z:
            names = set(z.namelist())
            self.assertIn('ExtendedLanguageLoader.asi', names)
            self.assertIn('LICENSE', names)
            self.assertIn('config/languages-21.example.ini', names)
            for name in names:
                self.assertNotIn('LanguageLoaderTest', name)
                self.assertFalse(name.startswith(('research/', 'language-fonts/', 'text/', 'private/')))
                self.assertNotIn(Path(name).suffix.lower(), ('.gxt', '.txd', '.dat', '.exe', '.dll'))
                if name != 'ExtendedLanguageLoader.asi':
                    self.assertNotIn(b'/home/', z.read(name))
                    self.assertNotIn(b'/mnt/', z.read(name))
            self.assertEqual(z.read('ExtendedLanguageLoader.asi'), (ROOT / 'dist/ExtendedLanguageLoader.asi').read_bytes())


if __name__ == '__main__': unittest.main()
