#!/usr/bin/env python3
"""Private HD Ukrainian atlas generation from externally supplied vector fonts.

Known 512px Andrulko/SAS TXD only. Preserve normalized cells and companion
metrics; unknown/reserved cells are retained rather than guessed/remapped.
No fonts/assets are bundled in the source repository.
"""
import argparse
import hashlib
from io import BytesIO
import json
import math
from pathlib import Path
import struct
from zipfile import ZipFile, ZIP_DEFLATED
from PIL import Image, ImageFont, ImageDraw
from fontTools.ttLib import TTFont
from fontTools.pens.boundsPen import BoundsPen

TXD_SHA = '0ced366f918f0ecdf3595028283d627dd07c7f88d59cf89649de50d374b9fb3d'
DAT_SHA = 'ee5b0f69372e1c1018a62af9fc4879daf6aacbdc58bc4ca57eafa75701b46d94'
RASTERIZATION_REVISION = 3
VECTOR_FILES = {'Beckett': 'Beckett.ttf', 'Futura': 'FuturaLT.ttf',
                'Bank': 'Bank.otf', 'Pricedown': 'Pricedown.otf'}
# Physical cell identity audited against BOTH original companion textures.
# This is NOT CP1251, nor the different Vice City Andrulko byte table.
CYRILLIC = {96: 'Б', 97: 'Ф', 98: 'Д', 99: 'Ж', 100: 'И', 101: 'Й',
            102: 'Л', 103: 'П', 104: 'У', 105: 'Щ', 106: 'Ц', 107: 'Ч',
            108: 'Ш', 109: 'Г', 110: 'Ї', 112: 'Ь', 113: 'Є', 114: 'Ю',
            115: 'З', 116: 'Ô', 117: 'Ü', 118: 'з', 119: 'б', 120: 'ф',
            121: 'д', 122: 'ж', 123: 'и', 124: 'й', 125: 'л', 126: 'у',
            127: 'м', 128: 'щ', 129: 'ц', 130: 'ч', 131: 'ш', 132: 'г',
            133: 'ї', 135: 'ь', 136: 'є', 137: 'ю', 138: 'я', 139: 'в',
            140: 'н', 141: 'Я', 142: 'п', 143: 'к'}
# Independently indexed subfont: 188 is У and 189 is М in BOTH originals.
# Do not derive these identities from the main uppercase row (103 П, 104 У).
SUBFONT = dict(enumerate('БФДЖИЙЛУМЩЦЧШГЇ', 181))
SUBFONT.update(dict(enumerate('ЬЄЮЯВНЗПК', 197)))
# Shared legacy Latin cells carry Cyrillic in this specific Ukrainian profile.
# In particular Latin t/k are NOT the requested Ukrainian т/к outlines.
PRIMARY_ALIASES = dict(zip('ABCEHIKMOPTXaceikoptx', 'АВСЕНІКМОРТХасеікортх'))
NATIVE_UV_WIDTH = (1 / 16 - 0.001) * 512  # 31.488 texels -> 32 native units
NATIVE_X_PER_TEXEL = 32 / NATIVE_UV_WIDTH
PAD = 1.5
TRACKING = 0.75
MIN_GAP = 1.5


def sha(data): return hashlib.sha256(data).hexdigest()


def chunk(tag, data, version):
    return struct.pack('<III', tag, len(data), version) + data


def children(data):
    offset = 0
    while offset < len(data):
        if offset + 12 > len(data): raise ValueError('Truncated chunk header')
        tag, size, version = struct.unpack_from('<III', data, offset)
        end = offset + 12 + size
        if end > len(data): raise ValueError('Truncated chunk body')
        yield tag, data[offset + 12:end], version
        offset = end


def decode_textures(data):
    top = list(children(data))
    if len(top) != 1 or top[0][0] != 0x16: raise ValueError('Expected one TXD')
    textures = {}
    for tag, payload, version in children(top[0][1]):
        if tag != 0x15: continue
        parts = list(children(payload))
        if not parts or parts[0][0] != 1: raise ValueError('Missing native structure')
        native = parts[0][1]
        platform = struct.unpack_from('<I', native)[0]
        name = native[8:40].split(b'\0')[0].decode('ascii')
        width, height = struct.unpack_from('<HH', native, 80)
        size = struct.unpack_from('<I', native, 88)[0]
        if platform != 9 or (width, height) != (512, 512) or native[85] != 1:
            raise ValueError('Expected 512px single-level D3D9 reference textures')
        if native[76:80] != b'DXT3' or len(native) != 92 + size:
            raise ValueError('Expected the reference DXT3 payload')
        textures[name] = (Image.frombytes('RGBA', (width, height), native[92:], 'bcn', 2), native[:88])
    if set(textures) != {'font1', 'font2'}: raise ValueError('Missing/duplicate font textures')
    return textures, top[0][2]


def encode_txd(images, headers, version):
    payload = chunk(1, struct.pack('<HH', 2, 2), version)
    for name in ('font2', 'font1'):
        stream = BytesIO()
        images[name].save(stream, format='DDS', pixel_format='DXT5')
        dds = stream.getvalue()
        if dds[84:88] != b'DXT5' or dds[:4] != b'DDS ': raise ValueError('DDS encoder did not produce BC3')
        pixels = dds[128:]
        size = images[name].width
        if len(pixels) != size * size: raise ValueError('Wrong BC3 level size')
        header = bytearray(headers[name])
        # Clean the original uninitialized mask-name bytes; retain sampler/raster flags.
        header[40:72] = (name + 'm').encode().ljust(32, b'\0')
        header[76:80] = b'DXT5'
        struct.pack_into('<HH', header, 80, size, size)
        header[84:88] = bytes((16, 1, 4, 9))  # compressed alpha, one level, D3D9 texture
        native = bytes(header) + struct.pack('<I', len(pixels)) + pixels
        payload += chunk(0x15, chunk(1, native, version) + chunk(3, b'', version), version)
    return chunk(0x16, payload + chunk(3, b'', version), version)


def cell_rect(cell):
    x, y = cell % 16 * 32, cell // 16 * 40
    return x, y, x + 32, min(y + 40, 512)


def mappings(name):
    primary, secondary = ('Futura', 'Pricedown') if name == 'font1' else ('Beckett', 'Bank')
    result = {ord(char) - 32: (primary, char) for char in
              'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789!\"$%&\'+,-./:;<=>'}
    result.update({cell: (primary, char) for cell, char in CYRILLIC.items()})
    result.update({91: (primary, '?'), 92: (primary, '°')})
    result.update({144 + i: (secondary, char) for i, char in enumerate('0123456789')})
    result[154] = secondary, ':'
    result.update({155 + i: (secondary, char) for i, char in enumerate('ABCDEFGHIJKLMNOPQRSTUVWXYZ')})
    result.update({cell: (secondary, char) for cell, char in SUBFONT.items()})
    if name == 'font1':
        result.update({93: ('Pricedown', '$'), 206: ('Pricedown', "'"), 207: ('Pricedown', '.')})
    return {cell: (style, PRIMARY_ALIASES.get(char, char) if cell < 91 else char)
            for cell, (style, char) in result.items()}


def vector_mask(font, char):
    bounds = font.getbbox(char)
    if bounds is None or bounds[2] <= bounds[0] or bounds[3] <= bounds[1]:
        raise ValueError(f'Empty vector glyph for {char!r}')
    image = Image.new('L', (bounds[2] - bounds[0], bounds[3] - bounds[1]))
    ImageDraw.Draw(image).text((-bounds[0], -bounds[1]), char, font=font, fill=255)
    box = image.getbbox()
    if not box: raise ValueError(f'Empty rendered glyph for {char!r}')
    return image.crop(box)


def ink_bbox(image, char):
    # Reject isolated DXT noise/neighbor fragments, but retain actual diacritics.
    alpha = image.getchannel('A'); pixels = alpha.load()
    remaining = {(x, y) for y in range(image.height) for x in range(image.width) if pixels[x, y] >= 64}
    components = []
    while remaining:
        first = remaining.pop(); group = {first}; stack = [first]
        while stack:
            x, y = stack.pop()
            for other in ((x-1,y), (x+1,y), (x,y-1), (x,y+1)):
                if other in remaining:
                    remaining.remove(other); group.add(other); stack.append(other)
        components.append(group)
    if not components: return None
    main = max(components, key=len)
    left, right = min(x for x,y in main), max(x for x,y in main)
    marked = char in 'ijЙйЇїÔÜ\":;' or not char.isalpha()
    kept = set(main)
    for group in components:
        # Diaeresis dots extend well beyond a narrow I/i stem in the originals.
        margin = 9 if marked else 3
        if min(x for x,y in group) < left - margin or max(x for x,y in group) > right + margin: continue
        if len(group) >= (2 if marked else max(4, len(main) * 0.035)):
            kept.update(group)
    return (min(x for x,y in kept), min(y for x,y in kept),
            max(x for x,y in kept)+1, max(y for x,y in kept)+1)


def font_geometry(vectors):
    """One isotropic outline scale and shared baseline per font, never per glyph."""
    geometries, renderers = {}, {}
    all_maps = [mappings(name) for name in ('font1', 'font2')]
    for style, filename in VECTOR_FILES.items():
        path = vectors / filename
        face = TTFont(path); cmap = face.getBestCmap() or {}; glyphs = face.getGlyphSet()
        required = 'АБВГҐДЕЄЖЗИІЇЙКЛМНОПРСТУФХЦЧШЩЬЮЯабвгґдеєжзиіїйклмнопрстуфхцчшщьюя'
        if any(ord(ch) not in cmap for ch in required): raise ValueError(f'{filename} lacks Ukrainian glyphs')
        measurements = {}
        chars = {ch for table in all_maps for s, ch in table.values() if s == style}
        for char in chars | {' '}:
            name = cmap.get(ord(char))
            if name is None: continue
            pen = BoundsPen(glyphs); glyphs[name].draw(pen)
            measurements[char] = {'glyph': name, 'bounds': pen.bounds,
                                  'advance': face['hmtx'].metrics[name][0]}
        units = face['head'].unitsPerEm
        face.close()
        boxes = [m['bounds'] for m in measurements.values() if m['bounds']]
        asc = max(b[3] for b in boxes); desc = max(0, -min(b[1] for b in boxes))
        right = max(b[2] - min(0, b[0]) for b in boxes)
        # Bank/Pricedown continue into the short final physical row. All letters
        # of a style MUST use the same em scale, including accented/descending ones.
        bottom = 30.5 if style in ('Bank', 'Pricedown') else 37.5
        em = min((bottom - 2.5) / (asc + desc), (NATIVE_UV_WIDTH - 3.5) / right)
        baseline = 2.5 + asc * em
        geometries[style] = {'units_per_em': units, 'texels_per_unit': em,
                              'baseline_texels': baseline, 'measurements': measurements}
        renderers[style] = ImageFont.truetype(str(path), 4096, layout_engine=ImageFont.Layout.BASIC)
    return geometries, renderers


def baseline_mask(font, char):
    box = font.getbbox(char, anchor='ls')
    image = Image.new('L', (box[2]-box[0], box[3]-box[1]))
    ImageDraw.Draw(image).text((-box[0], -box[1]), char, font=font, anchor='ls', fill=255)
    ink = image.getbbox()
    if not ink: raise ValueError(f'Empty outline for {char!r}')
    return image.crop(ink), (box[0]+ink[0], box[1]+ink[1])


def glyph_placement(geometry, char):
    metric = geometry['measurements'][char]
    left, bottom, right, top = metric['bounds']
    em = geometry['texels_per_unit']
    origin = PAD + max(0, -left * em)  # preserve positive bearings, contain overhangs
    advance = math.ceil(max(origin + metric['advance'] * em + TRACKING,
                           origin + right * em + MIN_GAP) * NATIVE_X_PER_TEXEL)
    return origin, advance


def rasterize(source, name, fonts, geometries, size):
    scale = size // 512
    output = source.resize((size, size), Image.Resampling.LANCZOS)
    records = []
    for cell, (style, char) in sorted(mappings(name).items()):
        rectangle = cell_rect(cell)
        has_ink = source.crop(rectangle).getchannel('A').point(lambda a: 255 if a >= 64 else 0).getbbox()
        geometry = geometries[style]
        metric = geometry['measurements'].get(char)
        if not metric or not metric['bounds'] or not has_ink:
            records.append({'cell': cell, 'style': style, 'char': char, 'vector': False,
                            'reason': 'missing vector glyph' if not metric else 'blank original cell'})
            continue
        mask, offset = baseline_mask(fonts[style], char)
        # Same factor for X/Y and every character of a style; do NOT fit old boxes.
        factor = geometry['texels_per_unit'] * geometry['units_per_em'] / fonts[style].size * scale
        width, height = max(1, round(mask.width * factor)), max(1, round(mask.height * factor))
        if factor > 1: raise ValueError('Insufficient vector supersampling')
        mask = mask.resize((width, height), Image.Resampling.LANCZOS)
        origin, advance = glyph_placement(geometry, char)
        x = round((rectangle[0] + origin) * scale + offset[0] * factor)
        y = round((rectangle[1] + geometry['baseline_texels']) * scale + offset[1] * factor)
        if not (rectangle[0]*scale <= x and x+width <= (rectangle[0]+NATIVE_UV_WIDTH)*scale and
                rectangle[1]*scale+1.0752*scale <= y and y+height <= rectangle[3]*scale-0.5*scale):
            raise ValueError(f'Glyph exceeds native sampled cell: {name}/{cell}/{char!r}, {(x,y,width,height)}')
        if advance < (x + width - rectangle[0]*scale) / scale * NATIVE_X_PER_TEXEL:
            raise ValueError('Advance clips/overlaps the glyph')
        output.paste((255, 255, 255, 0), tuple(value * scale for value in rectangle))
        glyph = Image.new('RGBA', mask.size, (255, 255, 255, 0)); glyph.putalpha(mask)
        output.paste(glyph, (x, y))
        records.append({'cell': cell, 'byte': cell + 32 if cell < 144 else None,
                        'style': style, 'char': char, 'glyph': metric['glyph'], 'vector': True,
                        'texels_per_font_unit': geometry['texels_per_unit'],
                        'baseline_texels': geometry['baseline_texels'], 'outline_bounds': metric['bounds'],
                        'advance': advance, 'target_bbox': [x, y, x + width, y + height]})
    return output, records


def read_metrics(dat):
    tables = {}; font = None; section = None
    for line in dat.decode('latin1').splitlines():
        content = line.split('#',1)[0].strip()
        if not content: continue
        if content.startswith('['): section = content; continue
        values = [int(v) for v in content.split()]
        if section == '[FONT_ID]': font = values[0]; tables[font] = []
        elif section == '[PROP]': tables[font].extend(values)
    if set(tables) != {0,1} or any(len(v) != 208 for v in tables.values()):
        raise ValueError('Expected two 208-entry metric tables')
    return tables


def make_metrics(dat, records, geometries):
    tables = read_metrics(dat)
    before = {k:list(v) for k,v in tables.items()}
    for texture, rows in records.items():
        font = 0 if texture == 'font2' else 1
        for row in rows:
            if row['vector']: tables[font][row['cell']] = row['advance']
        styles = ('Beckett','Bank') if font == 0 else ('Futura','Pricedown')
        # The native ABI shares one space entry across the two styles on each atlas.
        spaces = [geometries[s]['measurements'][' ']['advance'] * geometries[s]['texels_per_unit']
                  for s in styles]
        tables[font][0] = math.ceil(sum(spaces)/len(spaces) * NATIVE_X_PER_TEXEL)
    lines = []; font = None; section = None; row_index = 0
    for line in dat.decode('latin1').splitlines(keepends=True):
        content = line.split('#',1)[0].strip()
        if content.startswith('['): section = content; row_index = 0
        elif content and section == '[FONT_ID]': font = int(content)
        elif content and section == '[PROP]':
            comment = ('#'+line.split('#',1)[1].rstrip('\r\n')) if '#' in line else ''
            values = tables[font][row_index:row_index+8]; row_index += 8
            line = ' '.join(f'{v:2}' for v in values) + ('  '+comment if comment else '') + '\r\n'
        lines.append(line)
    result = ''.join(lines).encode('latin1')
    if read_metrics(result) != tables: raise ValueError('Metric output mismatch')
    changes = {str(font): [{'cell':i, 'old':before[font][i], 'new':v}
                           for i,v in enumerate(table) if v != before[font][i]]
               for font,table in tables.items()}
    return result, changes


def prepare(profile, vectors, output, size=2048):
    profile, vectors, output = (Path(p).resolve() for p in (profile, vectors, output))
    if size not in (1024, 2048): raise ValueError('Supported target sizes: 1024 or 2048')
    for source in (profile, vectors):
        if output == source or output in source.parents or source in output.parents:
            raise ValueError('Output must be isolated from source assets')
    if output.exists() and any(output.iterdir()):
        marker = output / 'rasterization-report.json'
        if not marker.is_file(): raise ValueError('Use an empty output directory, not an installed game/profile')
        previous = json.loads(marker.read_text())
        if previous.get('profile') != 'ukrainian' or previous.get('source_txd_sha256') != TXD_SHA:
            raise ValueError('Output directory is not owned by this Ukrainian rasterizer')
    txd = (profile / 'models/fonts.txd').read_bytes()
    dat = (profile / 'data/fonts.dat').read_bytes()
    if sha(txd) != TXD_SHA or sha(dat) != DAT_SHA:
        raise ValueError('Use the pinned original Ukrainian companion pair, not another profile/previous HD output')
    geometries, fonts = font_geometry(vectors)
    hashes = {name: sha((vectors / name).read_bytes()) for name in VECTOR_FILES.values()}
    textures, version = decode_textures(txd)
    images, records = {}, {}
    for name, (image, _) in textures.items():
        images[name], records[name] = rasterize(image, name, fonts, geometries, size)
    updated_dat, metric_changes = make_metrics(dat, records, geometries)
    encoded = encode_txd(images, {n: v[1] for n, v in textures.items()}, version)
    base = output / 'language-fonts/ukrainian'
    (base / 'models').mkdir(parents=True, exist_ok=True); (base / 'data').mkdir(exist_ok=True)
    (base / 'models/fonts.txd').write_bytes(encoded)
    (base / 'data/fonts.dat').write_bytes(updated_dat)
    report = {'profile': 'ukrainian', 'rasterization_revision': RASTERIZATION_REVISION,
              'size': size, 'format': 'DXT5/BC3, single level',
              'source_txd_sha256': TXD_SHA, 'source_dat_sha256': DAT_SHA,
              'output_txd_sha256': sha(encoded), 'output_dat_sha256': sha(updated_dat),
              'vector_fonts': hashes, 'font_geometry': geometries, 'metric_changes': metric_changes,
              'cells': records, 'live_rendering_verified': False,
              'policy': 'Common isotropic em scale/baseline per style; legacy Ukrainian aliases; paired recalculated proportional advances; native monospace/replacement-space settings retained; private test only'}
    (output / 'rasterization-report.json').write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n')
    preview = output / 'previews'; preview.mkdir(exist_ok=True)
    for name, image in images.items(): image.save(preview / f'{name}-{size}.png')
    instructions = f'''# Ukrainian HD font trial r{RASTERIZATION_REVISION} ({size}x{size})

Revision 3 supersedes BOTH earlier trials. Those trials stretched glyphs into
individual old bitmap boxes and reused incompatible widths. This revision uses
ONE isotropic em scale and a shared baseline per style, retains glyph bearings,
and recalculates proportional widths from the outlines/hmtx. It also uses the
Ukrainian т/к/і outlines for their shared Latin-byte aliases, not Latin t/k/i.
The previous Bank/Pricedown У/М cell correction remains. GXT bytes are unchanged.

Only the Ukrainian profile changes. Both font1/font2 contain vector-rasterized
Futura/Pricedown and Beckett/Bank regions. Reserved, unknown, blank or missing
vector cells retain scaled original content; this is not a generic Unicode font.
The NEW matched fonts.dat MUST be installed with the new TXD. Mixing in the
original or trial-1/trial-2 DAT breaks spacing. GXT bytes, ASI, INI, Russian
and Belarusian assets are unchanged. No runtime EasyFont/CLEO dependency is added.

Close GTA. Back up your entire language-fonts/ukrainian folder OUTSIDE the game.
Extract this ZIP into the game root, replacing BOTH Ukrainian fonts.txd/fonts.dat.
Keep Auto_fonts=1 and Font_profile=ukrainian. Select Ukrainian; test menu rows,
headings, subtitles, mission names, money/HUD and Ukrainian Є/Ї/І/Ґ as encoded by
this legacy translation. Switching back to Russian/Belarusian should restore
their unchanged profiles. Report exact words/screenshots if a glyph is wrong.

This is a private candidate, NOT live RenderWare/rasterization validation.
TTF coverage alone does not prove every legacy character has a verified slot;
unknown mappings are not invented. Restore the backed-up pair to roll back.
Fonts retain their original authors' rights; public redistribution permission
is unresolved. No TTF/OTF, translation, ASI, config or other profile is bundled.
'''
    (output / 'UKRAINIAN-HD-UPDATE.md').write_text(instructions)
    (base / 'README.md').write_text(instructions)
    archive = output / f'ExtendedLanguageLoader-ukrainian-hd-update-r{RASTERIZATION_REVISION}.zip'
    with ZipFile(archive, 'w', ZIP_DEFLATED) as z:
        for relative in ('UKRAINIAN-HD-UPDATE.md', 'rasterization-report.json',
                         'language-fonts/ukrainian/README.md',
                         'language-fonts/ukrainian/models/fonts.txd',
                         'language-fonts/ukrainian/data/fonts.dat'):
            z.write(output / relative, relative)
    print(archive)
    print(f'{sha(encoded)}  fonts.txd ({len(encoded):,} bytes)')
    for name, rows in records.items():
        print(name, 'vector cells:', sum(row['vector'] for row in rows),
              'retained mapped cells:', sum(not row['vector'] for row in rows))
    return report


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('profile', type=Path)
    parser.add_argument('vectors', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('--size', type=int, default=2048)
    args = parser.parse_args()
    prepare(args.profile, args.vectors, args.output, args.size)
