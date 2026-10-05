#!/usr/bin/env python3
"""Prepare private test-build copies of user-supplied GXTs. Never edit inputs."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import struct

STOCK = ['american', 'french', 'german', 'italian', 'spanish']
LOCALES = {
    'american': ('English', 'ENG', 0, 0),
    'french': ('French', 'FRE', 1, 1),
    'german': ('German', 'GER', 2, 1),
    'italian': ('Italian', 'ITA', 3, 1),
    'spanish': ('Spanish', 'SPA', 4, 1),
    'belarusian': ('Belarusian', 'BEL', 2, 1),
    'bosnian': ('Bosnian', 'BOS', 2, 1),
    'czech': ('Czech', 'CZE', 2, 1),
    'dutch': ('Dutch', 'DUT', 2, 1),
    'hungarian': ('Hungarian', 'HUN', 2, 1),
    'indonesian': ('Indonesian', 'IND', 0, 1),
    'mexican': ('Spanish (Mexico)', 'MEX', 4, 1),
    'polish': ('Polish', 'POL', 2, 1),
    'portuguese': ('Portuguese', 'POR', 0, 1),
    'romanian': ('Romanian', 'ROM', 2, 1),
    'russian': ('Russian', 'RUS', 2, 1),
    'serbian': ('Serbian', 'SRB', 2, 1),
    'slovak': ('Slovak', 'SVK', 2, 1),
    'slovenian': ('Slovenian', 'SLO', 2, 1),
    'turkish': ('Turkish', 'TUR', 2, 1),
    'ukrainian': ('Ukrainian', 'UKR', 2, 1),
}

DEFERRED = {'arabic', 'japanese', 'korean'}


def u32(b, p):
    if p < 0 or p + 4 > len(b):
        raise ValueError('truncated GXT integer')
    return struct.unpack_from('<I', b, p)[0]


def chunks(b, p):
    keys = data = None
    for _ in range(32):
        if p + 8 > len(b):
            raise ValueError('truncated GXT chunk')
        size = u32(b, p + 4)
        if p + 8 + size > len(b):
            raise ValueError('chunk exceeds file')
        tag = b[p:p + 4]
        if tag == b'TKEY':
            if not size or size % 8:
                raise ValueError('invalid TKEY')
            keys = b[p + 8:p + 8 + size]
        if tag == b'TDAT':
            if not size:
                raise ValueError('empty TDAT')
            data = b[p + 8:p + 8 + size]
        p += 8 + size
        if keys is not None and data is not None:
            for q in range(0, len(keys), 8):
                offset = u32(keys, q)
                if offset >= len(data) or data.find(b'\0', offset) < 0:
                    raise ValueError('text offset/terminator invalid')
            return len(keys) // 8
    raise ValueError('missing TKEY/TDAT')


def normalize(original):
    b = bytearray(original)
    repairs = []
    if len(b) < 24 or b[4:8] != b'TABL':
        raise ValueError('expected PC GXT with TABL')
    if b[:4] == b'\0\0\0\0':
        b[:4] = b'\x04\0\x08\0'
        repairs.append('Canonicalized legacy zero version/encoding header to version 4 / 8-bit')
    if b[:4] != b'\x04\0\x08\0':
        raise ValueError('not a version-4 8-bit GXT')
    size = u32(b, 8)
    main_offset = u32(b, 20)
    if b[12:20] != b'MAIN\0\0\0\0':
        raise ValueError('first table is not MAIN')
    # Two supplied files overdeclare TABL by exactly the four-byte TKEY tag.
    if size % 12 == 4 and main_offset == 12 + size - 4 and b[main_offset:main_offset + 4] == b'TKEY':
        size -= 4
        struct.pack_into('<I', b, 8, size)
        repairs.append('Corrected TABL size overdeclared by four bytes; no offsets or text changed')
    if not size or size % 12 or size // 12 > 200 or 12 + size > len(b):
        raise ValueError('invalid TABL size')
    if main_offset != 12 + size:
        raise ValueError('unexpected MAIN position')
    if b[main_offset:main_offset + 8] == b'MAIN\0\0\0\0' and b[main_offset + 8:main_offset + 12] == b'TKEY':
        del b[main_offset:main_offset + 8]
        for p in range(12, 12 + size, 12):
            offset = u32(b, p + 8)
            if offset > main_offset:
                struct.pack_into('<I', b, p + 8, offset - 8)
        repairs.append('Removed redundant eight-byte MAIN prefix; shifted mission offsets by eight bytes')
    entries = 0
    fixed_names = 0
    tables = set()
    for p in range(12, 12 + size, 12):
        raw = bytes(b[p:p + 8])
        if b'\0' not in raw:
            raise ValueError('unterminated table name')
        name = raw.split(b'\0')[0]
        if not name or name in tables:
            raise ValueError('empty/duplicate table name')
        tables.add(name)
        offset = u32(b, p + 8)
        if offset < main_offset or offset >= len(b):
            raise ValueError('invalid table offset')
        if name != b'MAIN':
            if offset + 8 > len(b):
                raise ValueError('truncated mission name')
            # Check the complete payload BEFORE repairing a wrong eight-byte title.
            entries += chunks(b, offset + 8)
            if bytes(b[offset:offset + 8]).split(b'\0')[0] != name:
                b[offset:offset + 8] = raw
                fixed_names += 1
        else:
            entries += chunks(b, offset)
    if fixed_names:
        repairs.append(f'Restored {fixed_names} mission-table title prefixes from TABL; payloads and offsets unchanged')
    return bytes(b), repairs, entries


def prepare(source, destination):
    source, destination = Path(source), Path(destination)
    files = {p.stem: p for p in source.glob('*.gxt') if p.is_file()}
    if not files or len(files) > 64:
        raise ValueError('expected 1..64 GXT files')
    if (source.resolve() == destination.resolve() or source.resolve() in destination.resolve().parents or
            destination.resolve() in source.resolve().parents):
        raise ValueError('Build output must be isolated from source language inputs')
    unknown = set(files) - LOCALES.keys() - DEFERRED
    if unknown:
        raise ValueError(f'Add explicit Latin/Cyrillic labels/layouts for unknown locales: {sorted(unknown)}')
    excluded = sorted(set(files) & DEFERRED)
    files = {name: path for name, path in files.items() if name in LOCALES}
    if not files:
        raise ValueError('No Latin/Cyrillic languages available for this beta')
    ordered = [name for name in STOCK if name in files] + sorted(set(files) - set(STOCK))
    destination.mkdir(parents=True, exist_ok=True)
    records = []
    ini = ['; Generated from external user-supplied language files. Inputs are never modified.',
           '; First beta: Latin/Cyrillic only; Arabic, Japanese and Korean are excluded.',
           '; Automatic font profiles: Russian, Belarusian and Ukrainian.',
           '[MAIN]', f'Number_of_languages = {len(ordered)}', 'Auto_fonts = 1', '']
    for index, stem in enumerate(ordered, 1):
        f = files[stem]
        original = f.read_bytes()
        output, repairs, entries = normalize(original)
        (destination / f.name).write_bytes(output)
        label, key, script, metric = LOCALES[stem]
        profile = {'ukrainian': 'ukrainian', 'russian': 'cyrillic',
                   'belarusian': 'belarusian'}.get(stem, 'keep')
        ini.extend([f'[{index}]', f'GXT_language_key = FEL_{key}', f'GXT_file = {f.name}',
                    f'Menu_name = {label}', f'Script_language = {script}', f'Is_system_metric = {metric}',
                    f'Font_profile = {profile}', ''])
        records.append({'slot': index, 'file': f.name, 'label': label, 'script_language': script,
                        'font_profile': profile, 'metric': metric, 'text_entries': entries, 'repairs': repairs,
                        'source_sha256': hashlib.sha256(original).hexdigest(),
                        'build_sha256': hashlib.sha256(output).hexdigest()})
    # Remove stale build-output GXTs only, never source files.
    for f in destination.glob('*.gxt'):
        if f.stem not in files:
            f.unlink()
    (destination / 'languages.ini').write_text('\n'.join(ini), encoding='ascii')
    report = {'source': str(source.resolve()), 'languages': records,
              'project': 'ExtendedLanguageLoader', 'release': '0.4.0', 'release_channel': 'beta', 'script_scope': ['Latin', 'Cyrillic'],
              'excluded_languages': [name + '.gxt' for name in excluded],
              'font_switching': True, 'rendering_verified': False,
              'font_notes': {'ukrainian': 'Companion GXT byte mapping matched; user reports working in live setup, not universal validation',
                             'cyrillic': 'Original SanLtd Russian atlas and companion metrics; 14692/16577 common GXT values and relevant menu bytes match. Belarusian replacements removed; native rendering retest pending',
                             'belarusian': 'Exact original GXT and TXD match; companion Pyats metrics, user reports working in live setup, not universal validation',
                             'keep': 'Preserve the existing original/Mod Loader font setup'},
              'font_repairs': {}, 'rendering_requirements': {}}
    (destination.parent / 'language-build-report.json').write_text(json.dumps(report, indent=2) + '\n')
    return report


if __name__ == '__main__':
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('source', type=Path)
    ap.add_argument('destination', type=Path)
    args = ap.parse_args()
    result = prepare(args.source, args.destination)
    print(f'Prepared {len(result["languages"])} languages in {args.destination}')
    for record in result['languages']:
        if record['repairs']:
            print(record['file'] + ': ' + '; '.join(record['repairs']))
