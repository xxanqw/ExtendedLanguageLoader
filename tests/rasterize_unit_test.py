#!/usr/bin/env python3
"""Optional asset-free rasterizer tests; requires Pillow and fontTools."""
import sys
import unittest
from pathlib import Path
from PIL import Image
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'scripts'))
import rasterize_ukrainian as hd


class RasterizerTest(unittest.TestCase):
    def test_grid_is_not_square(self):
        self.assertEqual(hd.cell_rect(96), (0, 240, 32, 280))
        self.assertEqual(hd.cell_rect(192), (0, 480, 32, 512))
        self.assertEqual(hd.cell_rect(207), (480, 480, 512, 512))

    def test_four_style_regions(self):
        # Primary legacy slots intentionally use audited Cyrillic lookalikes;
        # secondary Latin/menu slots retain their independently indexed letters.
        self.assertEqual(hd.mappings('font1')[33], ('Futura', 'А'))
        self.assertEqual(hd.mappings('font1')[155], ('Pricedown', 'A'))
        self.assertEqual(hd.mappings('font2')[33], ('Beckett', 'А'))
        self.assertEqual(hd.mappings('font2')[155], ('Bank', 'A'))
        for name in ('font1', 'font2'):
            self.assertEqual(hd.mappings(name)[113][1], 'Є')
            self.assertEqual(hd.mappings(name)[133][1], 'ї')
            self.assertEqual(hd.mappings(name)[198][1], 'Є')
            for cell in (0, 60, 111, 134, 196): self.assertNotIn(cell, hd.mappings(name))

    def test_independent_ukrainian_subfont_u_and_m(self):
        # Live trial regression: ЖПРНАЛ/ГРП/НАЛАШТПВАННЯ must not recur.
        # Legacy lower-case У/М bytes select subfont cells 188/189; copying
        # main uppercase cells 103/104 into these slots produces П/У instead.
        for name, style in (('font1', 'Pricedown'), ('font2', 'Bank')):
            table = hd.mappings(name)
            self.assertEqual(table[188], (style, 'У'))
            self.assertEqual(table[189], (style, 'М'))
            for byte, letter in ((0x9E, 'У'), (0x9F, 'М')):
                self.assertEqual(table[byte - 32 + 62][1], letter)
            self.assertEqual(table[103][1], 'П')
            self.assertEqual(table[104][1], 'У')
            self.assertEqual(''.join(table[i][1] for i in range(181,196)), 'БФДЖИЙЛУМЩЦЧШГЇ')

    def test_neighbor_fragment_removed(self):
        image = Image.new('RGBA', (32, 40))
        for x in range(2, 16):
            for y in range(8, 30): image.putpixel((x,y), (255,255,255,255))
        for y in range(11, 29): image.putpixel((31,y), (255,255,255,255))
        self.assertEqual(hd.ink_bbox(image, 'Г'), (2,8,16,30))

    def test_diacritics_retained(self):
        image = Image.new('RGBA', (32,40))
        for x in range(10,16):
            for y in range(12,30): image.putpixel((x,y),(255,255,255,255))
        for x in (3,4,20,21):
            for y in (6,7): image.putpixel((x,y),(255,255,255,255))
        self.assertEqual(hd.ink_bbox(image, 'Ї'), (3,6,22,30))

    def test_dxt5_payload_decode(self):
        import struct
        header = bytearray(88)
        struct.pack_into('<I', header, 0, 9)
        struct.pack_into('<I', header, 4, 0x1102)
        struct.pack_into('<I', header, 72, 0x300)
        images = {name: Image.new('RGBA',(1024,1024),(255,255,255,153)) for name in ('font1','font2')}
        headers = {}
        for name in images:
            h = bytearray(header); h[8:40] = name.encode().ljust(32,b'\0'); headers[name] = h
        encoded = hd.encode_txd(images, headers, 0x1803ffff)
        top = list(hd.children(encoded)); self.assertEqual(len(top),1)
        decoded = {}
        for tag, payload, _ in hd.children(top[0][1]):
            if tag != 0x15: continue
            native = list(hd.children(payload))[0][1]
            name = native[8:40].split(b'\0')[0].decode()
            self.assertEqual(native[76:80], b'DXT5')
            self.assertEqual(native[40:72], (name+'m').encode().ljust(32,b'\0'))
            w,h = struct.unpack_from('<HH',native,80)
            self.assertEqual((w,h),(1024,1024))
            self.assertEqual(len(native)-92,w*h)
            im = Image.frombytes('RGBA',(w,h),native[92:],'bcn',3)
            self.assertEqual(im.getpixel((500,500)), (255,255,255,153))
            decoded[name] = im
        self.assertEqual(set(decoded),set(images))

    def test_output_safety(self):
        import tempfile
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            profile, vectors = root/'source', root/'vectors'
            for unsafe in (profile, profile/'child', root, vectors/'child'):
                with self.assertRaisesRegex(ValueError, 'isolated'):
                    hd.prepare(profile, vectors, unsafe)
            output = root/'game'; output.mkdir()
            sentinel = output/'gta_sa.exe'; sentinel.write_bytes(b'untouched')
            with self.assertRaisesRegex(ValueError, 'empty output'):
                hd.prepare(profile, vectors, output)
            self.assertEqual(sentinel.read_bytes(), b'untouched')

    def test_truncated_chunks_refused(self):
        with self.assertRaises(ValueError): list(hd.children(b'bad'))
        with self.assertRaises(ValueError): list(hd.children(hd.chunk(1,b'123',0)[:-1]))


if __name__ == '__main__': unittest.main()
