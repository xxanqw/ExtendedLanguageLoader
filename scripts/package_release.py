#!/usr/bin/env python3
"""Shared Windows/Linux loader-only release packaging and PE/size checks."""
import hashlib
from pathlib import Path
import struct
from zipfile import ZipFile, ZIP_DEFLATED

ROOT = Path(__file__).resolve().parents[1]
MAX_ASI_BYTES = 2 * 1024 * 1024


def validate_asi(data):
    if len(data) > MAX_ASI_BYTES:
        raise ValueError(f'ASI is {len(data):,} bytes; release limit is {MAX_ASI_BYTES:,}. Remove debug data/use release settings.')
    if len(data) < 64 or data[:2] != b'MZ':
        raise ValueError('Missing PE DOS header')
    pe = struct.unpack_from('<I', data, 0x3c)[0]
    if pe + 24 > len(data) or data[pe:pe + 4] != b'PE\0\0':
        raise ValueError('Invalid PE header')
    machine, count = struct.unpack_from('<HH', data, pe + 4)
    optional_size, characteristics = struct.unpack_from('<HH', data, pe + 20)
    if machine != 0x14c or not characteristics & 0x2000:
        raise ValueError('ASI must be an x86 (i386) DLL, not an x64 DLL/EXE')
    optional = pe + 24
    sections = optional + optional_size
    if optional_size < 96 or sections + count * 40 > len(data):
        raise ValueError('Truncated PE optional/section headers')
    if struct.unpack_from('<H', data, optional)[0] != 0x10b:
        raise ValueError('ASI must use PE32 optional headers')
    for offset in range(sections, sections + count * 40, 40):
        # GNU long section names use a COFF string table and begin with '/'.
        name = data[offset:offset + 8].split(b'\0')[0]
        if name.startswith((b'.debug', b'/')):
            raise ValueError('Release ASI contains debug/long-name sections; strip the MinGW fallback')
    return len(data)


def package():
    binary = ROOT / 'dist/ExtendedLanguageLoader.asi'
    data = binary.read_bytes()
    validate_asi(data)
    archive = ROOT / 'dist/ExtendedLanguageLoader-v0.4.2.zip'
    with ZipFile(archive, 'w', ZIP_DEFLATED) as z:
        z.write(binary, 'ExtendedLanguageLoader.asi')
        for name in ('README.md', 'LICENSE', 'NOTICE.md', 'CHANGELOG.md'):
            z.write(ROOT / name, name)
        for directory in ('config', 'docs'):
            for path in sorted((ROOT / directory).glob('*')):
                if path.is_file(): z.write(path, path.relative_to(ROOT))
    for path in (binary, archive):
        print(f'{hashlib.sha256(path.read_bytes()).hexdigest()}  {path.name} ({path.stat().st_size:,} bytes)')


if __name__ == '__main__': package()
