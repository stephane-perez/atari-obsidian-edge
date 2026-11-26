#!/usr/bin/env python3
"""Small editor for GEM resource files (.RSC), used when no Atari resource editor is at hand.

  tools/rsc_patch.py EDGE.RSC --info
  tools/rsc_patch.py EDGE.RSC --free N 'text'          replace free string N (alert or string)
  tools/rsc_patch.py EDGE.RSC --object TREE OBJ 'text' replace the text of a G_STRING/G_BUTTON/
                                                      G_TITLE object, or the te_ptext of a
                                                      G_TEXT/G_BOXTEXT/G_FTEXT/G_FBOXTEXT one
                                                      (the object is widened if needed)
  tools/rsc_patch.py EDGE.RSC --add-free 'text'        add a free string (index printed)

Several operations may be given; they are applied in order and the file is
rewritten once.  Strings are encoded in latin-1 (use the Atari character set
for 8-bit characters).

How it works: the resource keeps its layout; new texts are appended after the
end of the standard part of the file (rsh_rssize), and the pointers that used
the old texts are moved to them (the old texts stay, unused: a resource editor
drops them when it saves the file).  New free strings get a new, longer free
string table, appended the same way.  Files with the colour icon extension
(rsh_vrsn bit 2: a table of longs at rsh_rssize, starting with the file size,
then the offset of the colour icon list and of the palette, ending with 0)
have that extension moved after the new data, and its offsets updated; the
colour icon data itself holds no file offsets.

Limit: the standard part must stay below 64 KB (16-bit offsets in the header).
"""
import argparse
import struct
import sys

HDR = ('vrsn', 'object', 'tedinfo', 'iconblk', 'bitblk', 'frstr', 'string', 'imdata',
       'frimg', 'trindex', 'nobs', 'ntree', 'nted', 'nib', 'nbb', 'nstring', 'nimages', 'rssize')
G_TEXT, G_BOXTEXT, G_FTEXT, G_FBOXTEXT = 21, 22, 29, 30
G_BUTTON, G_STRING, G_TITLE = 26, 28, 32


class Rsc:
    def __init__(self, path):
        self.path = path
        self.b = bytearray(open(path, 'rb').read())
        self.h = dict(zip(HDR, struct.unpack('>18H', self.b[:36])))
        self.end = self.h['rssize']
        self.ext = self.b[self.end:] if self.h['vrsn'] & 4 else b''
        self.new = bytearray()            # data appended after the standard part

    def data(self):
        """the standard part as it will be written: original + appended data"""
        return bytes(self.b[:self.end]) + bytes(self.new)

    def u32(self, o):
        return struct.unpack('>I', self.data()[o:o + 4])[0]

    def cstr(self, o):
        d = self.data()
        return d[o:d.index(0, o)]

    def obj_offset(self, tree, i):
        first = self.u32(self.h['trindex'] + 4 * tree)
        return first + 24 * i

    def append(self, data):
        if (self.end + len(self.new)) & 1:
            self.new += b'\0'
        pos = self.end + len(self.new)
        self.new += data
        return pos

    def free_ptr(self, n):
        assert 0 <= n < self.h['nstring'], 'no free string %d' % n
        return self.h['frstr'] + 4 * n

    def set_free(self, n, text):
        struct.pack_into('>I', self.b, self.free_ptr(n), self.append(text + b'\0'))

    def set_object(self, tree, i, text):
        o = self.obj_offset(tree, i)
        ty = struct.unpack('>H', self.b[o + 6:o + 8])[0] & 0xff
        spec = self.u32(o + 12)
        if ty in (G_STRING, G_BUTTON, G_TITLE):
            struct.pack_into('>I', self.b, o + 12, self.append(text + b'\0'))
        elif ty in (G_TEXT, G_BOXTEXT, G_FTEXT, G_FBOXTEXT):
            struct.pack_into('>I', self.b, spec, self.append(text + b'\0'))
            struct.pack_into('>H', self.b, spec + 24, len(text) + 1)      # te_txtlen
        else:
            sys.exit('object %d of tree %d has no text (type %d)' % (i, tree, ty))
        w = struct.unpack('>H', self.b[o + 20:o + 22])[0]
        if (w & 0xff) < len(text):                     # width: low byte = characters
            struct.pack_into('>H', self.b, o + 20, (w & 0xff00) | len(text))

    def add_free(self, text):
        n = self.h['nstring']
        table = [self.u32(self.free_ptr(k)) for k in range(n)]
        table.append(self.append(text + b'\0'))
        tpos = self.append(b''.join(struct.pack('>I', t) for t in table))
        self.h['frstr'] = tpos
        self.h['nstring'] = n + 1
        return n

    def save(self):
        if len(self.new) & 1:
            self.new += b'\0'
        newend = self.end + len(self.new)
        assert newend < 0x10000, 'standard part over 64 KB'
        ext = bytearray(self.ext)
        if ext:
            shift = len(self.new)
            for k in range(0, len(ext), 4):
                v = struct.unpack('>i', ext[k:k + 4])[0]
                if k and v == 0:
                    break
                if v > 0:
                    struct.pack_into('>i', ext, k, v + shift)
        self.h['rssize'] = newend
        out = bytearray(self.b[:self.end]) + self.new + ext
        out[:36] = struct.pack('>18H', *[self.h[k] for k in HDR])
        if ext:
            assert struct.unpack('>i', out[newend:newend + 4])[0] == len(out)
        open(self.path, 'wb').write(out)

    def info(self):
        print(self.h)
        for n in range(self.h['nstring']):
            print('free %3d: %r' % (n, self.cstr(self.u32(self.free_ptr(n)))))


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('rsc')
    ap.add_argument('--info', action='store_true')
    ap.add_argument('--free', nargs=2, action='append', metavar=('N', 'TEXT'), default=[])
    ap.add_argument('--object', nargs=3, action='append', metavar=('TREE', 'OBJ', 'TEXT'), default=[])
    ap.add_argument('--add-free', action='append', metavar='TEXT', default=[])
    a = ap.parse_args()
    r = Rsc(a.rsc)
    if a.info:
        r.info()
        return
    for n, t in a.free:
        r.set_free(int(n), t.encode('latin-1'))
    for tr, i, t in a.object:
        r.set_object(int(tr), int(i), t.encode('latin-1'))
    for t in a.add_free:
        print('new free string', r.add_free(t.encode('latin-1')))
    r.save()


if __name__ == '__main__':
    main()
