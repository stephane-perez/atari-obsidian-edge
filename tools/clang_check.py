#!/usr/bin/env python3
"""Static checks of the sources with clang (run after 'make': it needs build/include).

  tools/clang_check.py [FILE.c ...]

1. clang warnings (-Wformat, -Wconditional-uninitialized, -Wimplicit-fallthrough,
   ...): any warning fails.
2. 16-bit comparisons: Lattice C compared a short with an unsigned short on
   16 bits; GCC promotes both to int, so $FFFF read into a short (-1) is not
   equal to an unsigned short 65535.  This broke the end-of-chain tests of
   the whole FAT code (see CHANGELOG.md).  GCC does not warn about it: this
   check walks clang's AST and reports every comparison between a short and
   an unsigned short, a short and a constant >= 0x8000, or an unsigned short
   and a negative constant.  Make the intent explicit with a cast, e.g.
   (short)dpb.bitmask for a 16-bit comparison, (int)x for an int one.

Without arguments, checks every C file of the programs.  Exits 1 on any
finding.  Needs clang and the m68k-atari-mint cross compiler (for GCC's
own headers), and libcmini (LIBCMINI, default .tools/libcmini).
"""
import glob
import json
import os
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
LIBCMINI = os.environ.get("LIBCMINI", ".tools/libcmini")
CROSS = os.environ.get("CROSS", "m68k-atari-mint-")

WARNINGS = ["-Wformat", "-Wconditional-uninitialized", "-Wsometimes-uninitialized",
            "-Wimplicit-fallthrough", "-Wdangling-else", "-Wunused-value",
            "-Wunsequenced", "-Wfortify-source", "-Wincompatible-pointer-types",
            "-Wint-to-void-pointer-cast", "-Winvalid-source-encoding", "-Wmacro-redefined"]
# the code relies on these (Lattice C), as in the Makefile's CFLAGS
NO_WARNINGS = ["-Wno-int-conversion", "-Wno-pointer-sign", "-Wno-char-subscripts",
               "-Wno-parentheses"]
CMP = {"==", "!=", "<", ">", "<=", ">="}


def flags():
    gccinc = subprocess.run([CROSS + "gcc", "-print-file-name=include"],
                            capture_output=True, text=True, check=True).stdout.strip()
    return ["-target", "m68k-unknown-linux", "-fsyntax-only", "-funsigned-char",
            "-nostdinc", "-Ibuild/include", "-Iport/include", "-Iport/cmini",
            "-I" + LIBCMINI + "/include", "-isystem", gccinc,
            "-include", "lattice.h", "-include", "cmini.h", "-DNDEBUG", "-D__MINT__"]


def strip(e):
    """the expression under implicit promotions and parentheses, and its type"""
    while e.get("kind") in ("ImplicitCastExpr", "ParenExpr") and e.get("inner"):
        if e["kind"] == "ImplicitCastExpr" and e.get("castKind") not in ("IntegralCast", "LValueToRValue", None):
            break
        e = e["inner"][0]
    return e, e.get("type", {}).get("qualType", "")


def literal(e):
    if e.get("kind") == "IntegerLiteral":
        return int(e["value"])
    if (e.get("kind") == "UnaryOperator" and e.get("opcode") == "-"
            and e["inner"][0].get("kind") == "IntegerLiteral"):
        return -int(e["inner"][0]["value"])
    return None


def short_kind(t):
    t = t.replace("const ", "").replace("volatile ", "").strip()
    return {"short": "s", "unsigned short": "u", "WORD": "s", "UWORD": "u"}.get(t)


def update(loc, l):
    if not l:
        return
    l = l.get("expansionLoc", l)
    if l.get("file"):
        loc["file"] = l["file"]
    if l.get("line"):
        loc["line"] = l["line"]


def visit(node, fname, out, loc):
    update(loc, node.get("loc"))
    update(loc, (node.get("range") or {}).get("begin"))
    if node.get("kind") == "BinaryOperator" and node.get("opcode") in CMP:
        (a, ta), (b, tb) = strip(node["inner"][0]), strip(node["inner"][1])
        ka, kb, la, lb = short_kind(ta), short_kind(tb), literal(a), literal(b)
        why = None
        if {ka, kb} == {"s", "u"}:
            why = "short vs unsigned short"
        elif ka == "s" and lb is not None and lb >= 0x8000:
            why = "short vs constant 0x%x" % lb
        elif kb == "s" and la is not None and la >= 0x8000:
            why = "short vs constant 0x%x" % la
        elif ka == "u" and lb is not None and lb < 0:
            why = "unsigned short vs %d" % lb
        elif kb == "u" and la is not None and la < 0:
            why = "unsigned short vs %d" % la
        if why and os.path.basename(loc.get("file", fname)) == os.path.basename(fname):
            out.append((loc.get("line", 0), why))
    for c in node.get("inner", []) or []:
        visit(c, fname, out, loc)


def main():
    os.chdir(ROOT)
    files = sys.argv[1:] or sorted(glob.glob("*.c") + glob.glob("lib/*.c") +
                                   glob.glob("dmirror/*.c") + glob.glob("port/*.c") +
                                   glob.glob("port/cmini/*.c"))
    base = flags()
    problems = 0
    for f in files:
        # 1. warnings
        p = subprocess.run(["clang"] + base + WARNINGS + NO_WARNINGS + [f],
                           capture_output=True, text=True)
        for line in p.stderr.splitlines():
            if "warning:" in line or "error:" in line:
                print(line)
                problems += 1
        if p.returncode:
            print("%s: clang failed" % f)
            problems += 1
            continue
        # 2. 16-bit comparisons
        p = subprocess.run(["clang"] + base + ["-w", "-Xclang", "-ast-dump=json", f],
                           capture_output=True, text=True)
        out = []
        visit(json.loads(p.stdout), f, out, {})
        src = open(f, encoding="latin-1").read().split("\n")
        for line, why in sorted(set(out)):
            print("%s:%d: 16-bit comparison [%s]: %s" % (f, line, why, src[line - 1].strip()[:90]))
            problems += 1
    print("%d file(s) checked, %d problem(s)" % (len(files), problems))
    sys.exit(1 if problems else 0)


if __name__ == "__main__":
    main()
