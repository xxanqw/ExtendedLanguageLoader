import configparser
import hashlib
import json
import os
from pathlib import Path
import sys
import tempfile
import unittest
import zlib
from zipfile import ZipFile

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'scripts'))
from prepare_languages import prepare, normalize, u32

# Licensed/reference fixtures are external and never committed.
SOURCE = Path(sys.argv.pop(1)) if len(sys.argv) > 1 else Path(os.environ['LANGUAGE_SOURCE'])
ASSETS = Path(os.environ['ELL_TEST_ASSETS'])
FONTS = Path(os.environ['FONT_SOURCE'])
DIST = Path(__file__).resolve().parents[1] / 'dist/private'


def payloads(b):
    """Compare actual text/keys, ignoring legacy metadata repaired by the build."""
    result = {}
    for i in range(u32(b, 8) // 12):
        p = 12 + i * 12
        name = bytes(b[p:p + 8]).split(b'\0')[0]
        offset = u32(b, p + 8)
        if name != b'MAIN' or b[offset:offset + 4] != b'TKEY':
            offset += 8
        keys = data = None
        for _ in range(32):
            size = u32(b, offset + 4)
            tag = b[offset:offset + 4]
            if tag == b'TKEY': keys = bytes(b[offset + 8:offset + 8 + size])
            if tag == b'TDAT': data = bytes(b[offset + 8:offset + 8 + size])
            offset += size + 8
            if keys is not None and data is not None:
                result[name] = (keys, data)
                break
    return result


class PreparationTest(unittest.TestCase):
    def test_full_bundle_and_no_source_edits(self):
        files = sorted(SOURCE.glob('*.gxt'))
        self.assertEqual(len(files), 24)
        before = {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in files}
        with tempfile.TemporaryDirectory() as tmp:
            output = Path(tmp) / 'text'
            output.mkdir()
            for name in ('arabic.gxt', 'japanese.gxt', 'korean.gxt'):
                (output / name).write_bytes(b'stale excluded output')
            report = prepare(SOURCE, output)
            config = configparser.ConfigParser()
            config.read(output / 'languages.ini')
            self.assertEqual(config.getint('MAIN', 'Number_of_languages'), 21)
            self.assertEqual(config['21']['GXT_file'], 'ukrainian.gxt')
            self.assertEqual(config['21']['Menu_name'], 'Ukrainian')
            self.assertEqual(config.getint('MAIN', 'Auto_fonts'), 1)
            self.assertEqual(config['21']['Font_profile'], 'ukrainian')
            self.assertEqual(config['6']['Font_profile'], 'belarusian')
            self.assertEqual(config['13']['Font_profile'], 'keep')
            self.assertEqual(config['14']['Font_profile'], 'keep')
            self.assertEqual(report['rendering_requirements'], {})
            self.assertEqual(report['release'], '0.4.0')
            self.assertEqual(report['release_channel'], 'beta')
            self.assertEqual(report['script_scope'], ['Latin', 'Cyrillic'])
            self.assertEqual(report['excluded_languages'], ['arabic.gxt', 'japanese.gxt', 'korean.gxt'])
            self.assertEqual(len(list(output.glob('*.gxt'))), 21)
            for name in report['excluded_languages']: self.assertFalse((output / name).exists())
            self.assertEqual(config['16']['Font_profile'], 'cyrillic')
            self.assertEqual(config['1']['Font_profile'], 'keep')
            self.assertTrue(report['font_switching'])
            repaired = set()
            for record in report['languages']:
                source = (SOURCE / record['file']).read_bytes()
                build = (output / record['file']).read_bytes()
                self.assertEqual(payloads(source), payloads(build), record['file'])
                again, changes, _ = normalize(build)
                self.assertEqual(again, build)
                self.assertEqual(changes, [])
                self.assertEqual(record['source_sha256'], before[record['file']])
                self.assertEqual(record['build_sha256'], hashlib.sha256(build).hexdigest())
                self.assertEqual(record['font_profile'], config[str(record['slot'])]['Font_profile'])
                if record['repairs']: repaired.add(record['file'])
            self.assertEqual(repaired, {'belarusian.gxt', 'dutch.gxt', 'mexican.gxt'})
            self.assertEqual(json.loads((Path(tmp) / 'language-build-report.json').read_text())['rendering_verified'], False)
        self.assertEqual(before, {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in files})

    def test_bad_inputs_rejected(self):
        for b in (b'', b'\0' * 24, (SOURCE / 'ukrainian.gxt').read_bytes()[:200]):
            with self.assertRaises(ValueError): normalize(b)
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            with self.assertRaises(ValueError): prepare(root, root / 'out')
            (root / 'unknown.gxt').write_bytes((SOURCE / 'ukrainian.gxt').read_bytes())
            with self.assertRaises(ValueError): prepare(root, root / 'out')

    def test_ukrainian_companion_translation_match(self):
        reference = ASSETS / 'research/ukrainian/andrulko-sas/app/text/american.gxt'
        if not reference.exists(): self.skipTest('optional downloaded research reference is not present')
        def values(b):
            result = {}
            for table, (keys, data) in payloads(b).items():
                for p in range(0, len(keys), 8):
                    offset, key = u32(keys, p), u32(keys, p + 4)
                    result[(table, key)] = data[offset:].split(b'\0')[0]
            return result
        supplied = values((SOURCE / 'ukrainian.gxt').read_bytes())
        companion = values(reference.read_bytes())
        self.assertEqual(len(companion), 16586)
        self.assertTrue(companion.keys() <= supplied.keys())
        def key(label): return (b'MAIN', zlib.crc32(label.encode()) ^ 0xffffffff)
        changed = {k for k in companion if supplied[k] != companion[k]}
        self.assertEqual(changed, {key('FEL_ENG'), key('FEL_SPA')})
        self.assertEqual(supplied.keys() - companion.keys(),
                         {key('FEL_' + suffix) for suffix in ('IND', 'POL', 'POR', 'RUS', 'TUR', 'UKR')})

    def test_font_profile_pairs_and_sources_unchanged(self):
        fonts = FONTS
        cyr = fonts / 'cyrillic'
        source = SOURCE / 'fonts/fonts.txd'
        self.assertEqual((fonts / 'belarusian/models/fonts.txd').read_bytes(), source.read_bytes())
        self.assertEqual(hashlib.sha256(source.read_bytes()).hexdigest(),
                         '7b2a94699d1bc41e009bdec6d4982737e529645705254435c550bc6a67d4067a')
        self.assertEqual(hashlib.sha256((cyr / 'models/fonts.txd').read_bytes()).hexdigest(),
                         'c61d5c746afc7cce85ef19853d252177707efc7e67e49363f372635dc94ccd48')
        self.assertEqual(hashlib.sha256((cyr / 'data/fonts.dat').read_bytes()).hexdigest(),
                         'f915a17b3673023c4c69fddb85d24709365bcdecee8615e5d882ac48721fd538')
        self.assertNotEqual((cyr / 'models/fonts.txd').read_bytes(), (fonts / 'belarusian/models/fonts.txd').read_bytes())
        self.assertNotEqual((cyr / 'data/fonts.dat').read_bytes(), (fonts / 'belarusian/data/fonts.dat').read_bytes())
        stock = ASSETS / 'research/russian/stock-fonts.dat'
        self.assertEqual(hashlib.sha256(stock.read_bytes()).hexdigest(),
                         '0b6ecb4493b0261eece4b025e31aa7b55ae68ca8529c4f7064b5b3b7297c913c')
        self.assertEqual(hashlib.sha256((fonts / 'ukrainian/data/fonts.dat').read_bytes()).hexdigest(),
                         'ee5b0f69372e1c1018a62af9fc4879daf6aacbdc58bc4ca57eafa75701b46d94')

    def test_packaged_font_manifest(self):
        with ZipFile(DIST / 'ExtendedLanguageLoader-v0.4.0-private.zip') as archive:
            report = json.loads(archive.read('language-build-report.json'))
            self.assertEqual(set(report['font_assets']), {
                f'language-fonts/{name}/{path}' for name in ('cyrillic', 'ukrainian', 'belarusian')
                for path in ('models/fonts.txd', 'data/fonts.dat')})
            for path, sha256 in report['font_assets'].items():
                self.assertEqual(hashlib.sha256(archive.read(path)).hexdigest(), sha256)
            self.assertFalse(any(path.startswith(('modloader/', 'optional-fonts/')) for path in archive.namelist()))
            self.assertFalse(report['rendering_verified'])

    def test_beta_archives_exclude_deferred_assets(self):
        root = DIST
        for name in ('ExtendedLanguageLoader-v0.4.0-private.zip', 'ExtendedLanguageLoader-v0.4.0-private-update.zip'):
            with ZipFile(root / name) as archive:
                names = set(archive.namelist())
                self.assertFalse(any(any(word in path for word in ('arabic', 'japanese', 'korean')) for path in names))
                self.assertEqual(len([p for p in names if p.endswith('.gxt')]), 0 if '-update' in name else 21)
                self.assertEqual(len([p for p in names if p.endswith('fonts.txd')]), 3)
                self.assertEqual(len([p for p in names if p.endswith('fonts.dat')]), 3)
                ini = archive.read('text/languages.ini')
                self.assertIn(b'Number_of_languages = 21', ini)
                self.assertNotIn(b'Font_profile = arabic', ini)
                self.assertEqual(archive.read('ExtendedLanguageLoader.asi'), (root / 'ExtendedLanguageLoader.asi').read_bytes())

    def test_belarusian_exact_companion_pair(self):
        root = ASSETS
        reference = root / 'research/belarusian/extracted/BIELARUSIZATAR-LS'
        self.assertEqual((SOURCE / 'belarusian.gxt').read_bytes(), (reference / 'text/american.gxt').read_bytes())
        for path in ('models/fonts.txd', 'data/fonts.dat'):
            self.assertEqual((FONTS / 'belarusian' / path).read_bytes(), (reference / path).read_bytes())
        self.assertNotEqual((FONTS / 'belarusian/data/fonts.dat').read_bytes(),
                            (FONTS / 'cyrillic/data/fonts.dat').read_bytes())
        self.assertEqual(hashlib.sha256((FONTS / 'belarusian/data/fonts.dat').read_bytes()).hexdigest(),
                         '5135c8e20e549db4d3f3998f83f834f54c5abcfd0b214ffaedb17019e62b2769')

    def test_russian_companion_gxt_encoding_evidence(self):
        reference = ASSETS / 'research/russian/sanltd/extracted'
        for path in ('models/fonts.txd', 'data/fonts.dat'):
            self.assertEqual((FONTS / 'cyrillic' / path).read_bytes(), (reference / Path(path).name).read_bytes())
        source = (SOURCE / 'russian.gxt').read_bytes()
        self.assertEqual(hashlib.sha256(source).hexdigest(),
                         '01e126c6d21b80f93530f55a1076de4fb0016a77d09ba710157a5558cab85461')
        companion = (reference / 'american.gxt').read_bytes()
        self.assertEqual(hashlib.sha256(companion).hexdigest(),
                         '08725985cc39bddc8d24b704c940e28c204d8186d8900fe39a7dc9de4b762194')
        def entries(b):
            result = {}
            for table, (keys, data) in payloads(b).items():
                for offset in range(0, len(keys), 8):
                    pos, key = u32(keys, offset), u32(keys, offset + 4)
                    result[table, key] = data[pos:data.index(0, pos)]
            return result
        supplied, original = entries(source), entries(companion)
        common = supplied.keys() & original.keys()
        self.assertEqual(len(common), 16577)
        self.assertEqual(sum(supplied[key] == original[key] for key in common), 14692)
        for name, expected in {'FET_OPT': bytes.fromhex('AD 61 63 A6 70 6F 9D 6B 9C'),
                               'FEH_LAN': bytes.fromhex('95 9F A8 6B'),
                               'FEDS_TB': bytes.fromhex('AD 61 9F 61 9A')}.items():
            key = (b'MAIN', zlib.crc32(name.encode()) ^ 0xffffffff)
            self.assertEqual(supplied[key], expected)
            self.assertEqual(original[key], expected)

    def test_scope_and_source_destination_guards(self):
        with self.assertRaises(ValueError): prepare(SOURCE, SOURCE)
        with tempfile.TemporaryDirectory() as tmp:
            source, output = Path(tmp) / 'source', Path(tmp) / 'out'
            source.mkdir()
            (source / 'arabic.gxt').write_bytes(b'unparsed excluded input')
            with self.assertRaises(ValueError): prepare(source, output)
            (source / 'greek.gxt').write_bytes(b'unknown locale')
            with self.assertRaises(ValueError): prepare(source, output)

    def test_matched_ukrainian_font_asset(self):
        font = FONTS / 'ukrainian/models/fonts.txd'
        self.assertEqual(hashlib.sha256(font.read_bytes()).hexdigest(),
                         '0ced366f918f0ecdf3595028283d627dd07c7f88d59cf89649de50d374b9fb3d')


if __name__ == '__main__': unittest.main()
