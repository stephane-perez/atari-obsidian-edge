#!/usr/bin/env python3
"""Compare the sizes of Atari programs (GEMDOS .PRG header), as a Markdown table.

  tools/prg_sizes.py NAME=DIR [NAME=DIR ...]

For each program found in the first DIR (*.PRG), prints the file size and the
memory it needs (TEXT + DATA + BSS) in each DIR.
"""
import os
import struct
import sys


def prg(path):
    with open(path, "rb") as f:
        head = f.read(28)
    magic, text, data, bss = struct.unpack(">HIII", head[:14])
    if magic != 0x601A:
        sys.exit("%s: not a GEMDOS program" % path)
    return os.path.getsize(path), text + data + bss


def main():
    builds = [a.split("=", 1) for a in sys.argv[1:]]
    if not builds or any(len(b) != 2 for b in builds):
        sys.exit(__doc__)
    names = sorted(n for n in os.listdir(builds[0][1]) if n.upper().endswith(".PRG"))
    cols = " | ".join("%s file | %s memory" % (b[0], b[0]) for b in builds)
    print("| Program | %s |" % cols)
    print("|---|" + "---:|" * (2 * len(builds)))
    for n in names:
        cells = []
        for _, d in builds:
            p = os.path.join(d, n)
            if os.path.exists(p):
                size, mem = prg(p)
                cells += ["%d" % size, "%d" % mem]
            else:
                cells += ["-", "-"]
        print("| %s | %s |" % (n, " | ".join(cells)))


if __name__ == "__main__":
    main()
