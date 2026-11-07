# GCC port layer

Diamond Edge 2.57 was written for **Lattice C 5** (compiled under TOS, with
the `*.PRJ` project files). Obsidian Edge is built on Linux with the
`m68k-atari-mint` GCC cross compiler, MiNTLib and GEMlib. This directory holds
what that needs, so that the original sources stay almost unchanged.

## What is here

| File | Role |
|---|---|
| `include/aes.h`, `include/vdi.h`, `include/oe_gem.h` | Lattice `<aes.h>`/`<vdi.h>` on top of GEMlib: 23-argument `evnt_multi()` (timer as two words), `evnt_timer(lo,hi)`, variadic `wind_set()` with `ADDR()`, `WF_CXYWH`, colour names, `_AESglobal`, `objc_xywh()`, `rc_inside()` |
| `include/dos.h`, `include/tos.h`, `include/oserr.h`, `include/cookie.h`, `include/portab.h` | Lattice/Pure C headers: `FA_*`, `FILEINFO`, `dfind()`, `DTA`, `BPB`, `KEYTAB`, `_tos`, `_OSERR`, BIOS error codes, `getcookie()`, `WORD`/`LONG`/`MLOCAL`... |
| `include/lattice.h` | included in every C file (`gcc -include`): `min()`/`max()`, `fgetw()`/`fputw()`/`fgetl()`/`fputl()`, the Lattice `srand48()`/`mrand48()`, an in-place-safe `swab()`, `_mediach()`, `getdsk()`/`chgdsk()`, `filelength()` |
| `include/scsidefs.h` | the SCSIDRV interface (the upstream sources include it but do not ship it) |
| `include/misc.h`, `include/extendos.h` | `<misc.h>`/`<extendos.h>` as found by the Lattice include path |
| `lattice.c` | implementation of the functions above |
| `mediach.S` | `_mediach()`: forces GEMDOS to see a media change (hooks `hdv_bpb`, `hdv_rw`, `hdv_mediach`) |
| `edgeasm.S`, `compmem.S` | GNU as versions of `edgeasm.s` and `lib/compmem.s` |
| `xmode.c` | `_XMODE` for DMIRROR.PRG (3 when run from the AUTO folder) |
| `cmini/` | for the libcmini build (`LIBC=cmini`, the default): headers libcmini lacks (`<osbind.h>`, `<compiler.h>` for GEMlib, `<math.h>`), our own `stat()` and `struct stat`, `access()`, `mkdir()`, `rindex()` |
| `README.TXT` | the README of the distribution (plain ASCII, for the Atari) |

The Makefile also generates `build/include/mt_gem.h`: a copy of GEMlib's
header where `OBJECT.ob_spec` is a `void *` (as in Lattice C) instead of the
`OBSPEC` union of the same size, because the sources use it as a pointer in
455 places.

## Differences between Lattice C and GCC that matter

- **`char` and `unsigned char`**: Lattice `-cu` made them interchangeable;
  GCC (`-funsigned-char`) does not, so a few prototypes were aligned with
  the function definitions.
- **`int` is 32 bits** in both builds. `lib/scsidrv.c` is the exception: the
  SCSIDRV routines take 16-bit parameters, so it is compiled with `-mshort`,
  like it was compiled with Lattice's `-w`. Its public interface only uses
  `long` and pointers.
- **`mrand48()`**: Lattice returns the low 32 bits of the 48-bit state, POSIX
  (MiNTLib) the high 32 bits. `lib/encrypt.c` (registration) depends on the
  Lattice behaviour: the fixed serial number / registration code of the free
  version (`E-0000-25` / `DSFMDSCN`) only match with it.
- **The protection structure** in `protect.c` is never written by EDGE.PRG
  itself, only patched into the file by INSTALL.PRG: GCC removed it and used
  its initial values, so it is declared `volatile`. GCC (a.out) only aligns
  it on 2 bytes, so INSTALL.PRG now searches for it in steps of 2 bytes.
- **Casts as lvalues** (`(char *)p = ...`), accepted by Lattice, were
  rewritten.
- **Assembler**: Lattice/Devpac syntax was translated to GNU as;
  `fast_copy()` now preserves `d2`, which GCC expects.

## C library

The default build uses libcmini (`scripts/get-libcmini.sh`), compiled with
`-nostdinc` so that no MiNTLib header can be mixed in; `printf()` and
friends come from `libiiomini` (no floating point: the sources use none).
`make LIBC=mintlib` builds with MiNTLib instead; the objects of the two
builds are kept apart (`build/obj-cmini`, `build/obj-mintlib`).

libcmini has no `<sys/stat.h>`, and the layout of the `struct stat` its
`stat()` fills in depends on the headers found when libcmini is compiled:
we use our own minimal `stat()` instead (the sources only need the size of
a file).

## Not ported

- `EDGEDEMO.PRG` (demo version, `-dDEMO`) and `lib/GENREG.TTP` (the
  registration key generator): not needed for the free version.
- The prebuilt Lattice libraries `lib/*.LIB` are not used; the GCC build
  compiles their sources (`lib/*.c`, `lib/compmem.s` via `compmem.S`).
