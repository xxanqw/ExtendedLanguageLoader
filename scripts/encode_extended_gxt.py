#!/usr/bin/env python3
"""Offline BYTE->Unicode->BYTE GXT conversion. Never claim runtime UTF-8 support.
Consumes the generator's canonical extended_fonts.json mapping, preserving GTA tags.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct
from prepare_languages import normalize, u32


def byte_map(value):
    if isinstance(value, dict) and 'mapping' in value:
        value = value['mapping']
    out = [None] * 256
    if isinstance(value, list) and len(value) == 256:
        for byte, cp in enumerate(value):
            if type(cp) is not int or not 0 <= cp <= 0x10ffff or 0xd800 <= cp <= 0xdfff:
                raise ValueError('Mapping must contain 256 Unicode scalars, with 0 unassigned')
            out[byte] = chr(cp) if cp else None
    elif isinstance(value, dict):
        seen = set()
        for key, char in value.items():
            byte = int(key, 16) if key.lower().startswith('0x') else int(key)
            if not 0 <= byte <= 255 or byte in seen or not isinstance(char, str) or len(char) != 1 or 0xd800 <= ord(char) <= 0xdfff:
                raise ValueError('Expected unique byte keys and Unicode scalars')
            out[byte] = char
            seen.add(byte)
    else:
        raise ValueError('Expected canonical manifest, 256-scalar array or byte-string object')
    if out[32] != ' ':
        raise ValueError('Byte 0x20 must remain space')
    if any(out[:32]) or out[126] not in (None, '~'):
        raise ValueError('Controls and native GTA tilde delimiter cannot be assigned to letters')
    return out


def recode(source, source_map, target_map):
    data, repairs, count = normalize(source)
    data = bytearray(data)
    inverse = {}
    for byte, char in enumerate(target_map):
        if char is not None:
            inverse.setdefault(char, byte)
    changed, regions = 0, set()
    table_size = u32(data, 8)
    for entry in range(12, 12 + table_size, 12):
        table = data[entry:entry+8].split(b'\0')[0]
        p = u32(data, entry + 8) + (0 if table == b'MAIN' else 8)
        while p + 8 <= len(data):
            size = u32(data, p+4)
            if p + 8 + size > len(data):
                raise ValueError('TDAT exceeds file')
            if data[p:p+4] == b'TDAT':
                if p in regions:
                    break
                regions.add(p)
                tag = False
                for q in range(p+8, p+8+size):
                    byte = data[q]
                    if byte == 0:
                        if tag:
                            raise ValueError('Unterminated GTA formatting tag')
                        continue
                    if byte == 126:
                        tag = not tag
                        continue
                    if tag or byte < 32:
                        continue
                    char = source_map[byte]
                    if char is None or char not in inverse:
                        raise ValueError(f'Unrepresentable byte 0x{byte:02X} ({char!r}), file offset {q}; provide the actual source map / complete target map')
                    target = byte if target_map[byte] == char else inverse[char]
                    changed += target != byte
                    data[q] = target
                break
            p += 8 + size
        else:
            raise ValueError('Missing TDAT')
    result, additional_repairs, _ = normalize(bytes(data))
    assert not additional_repairs
    return result, {'entries':count, 'changedBytes':changed, 'headerRepairs':repairs}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('input', type=Path)
    parser.add_argument('manifest', type=Path, help='target ExtendedFonts/extended_fonts.json')
    parser.add_argument('output', type=Path)
    source = parser.add_mutually_exclusive_group(required=True)
    source.add_argument('--source-map', type=Path, help='actual source byte-map JSON, NOT a guessed encoding')
    source.add_argument('--source-encoding', choices=['cp1251'], help='explicitly assert source is already standard CP1251')
    args = parser.parse_args()
    if args.input.resolve() == args.output.resolve() or args.output.exists():
        raise ValueError('Input is never overwritten; output must be a new path')
    manifest = json.loads(args.manifest.read_text(encoding='utf-8'))
    if manifest.get('version') != 1 or manifest.get('slotIsByte') is not True:
        raise ValueError('Expected version-1 ELL manifest')
    target_map = byte_map(manifest)
    if args.source_map:
        source_map = byte_map(json.loads(args.source_map.read_text(encoding='utf-8')))
    else:
        source_map = [None] * 256
        for byte in range(32, 256):
            try:
                source_map[byte] = bytes([byte]).decode('cp1251')
            except UnicodeDecodeError:
                pass
    output, report = recode(args.input.read_bytes(), source_map, target_map)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open('xb') as file:
        file.write(output)
    report.update(encoding=manifest['encoding'], manifestSha256=hashlib.sha256(args.manifest.read_bytes()).hexdigest(), outputSha256=hashlib.sha256(output).hexdigest())
    args.output.with_suffix(args.output.suffix+'.mapping-report.json').write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
