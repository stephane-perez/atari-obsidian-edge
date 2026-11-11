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
| `include/lattice.h` | included in every C file (`gcc -include`): `min()`/`max()`, `fgetw()`/`fputw()`/`fgetl()`/`fputl()`, an in-place-safe `swab()`, `_mediach()`, `getdsk()`/`chgdsk()`, `filelength()` |
| `include/scsidefs.h` | the SCSIDRV interface (the upstream sources include it but do not ship it) |
| `include/misc.h`, `include/extendos.h` | `<misc.h>`/`<extendos.h>` as found by the Lattice include path |
| `lattice.c` | implementation of the functions above |
| `mediach.S` | `_mediach()`: forces GEMDOS to see a media change (hooks `hdv_bpb`, `hdv_rw`, `hdv_mediach`) |
| `edgeasm.S`, `compmem.S` | GNU as versions of `edgeasm.s` and `lib/compmem.s` |
| `callstack.S` | `oe_call_on_stack()`: runs `xdraw_cicon()` (colour icons, called by the AES) on a private 8 KB stack |
| `xmode.c` | `_XMODE` for DMIRROR.PRG (3 when run from the AUTO folder) |
| `cmini/` | for the libcmini build (`LIBC=cmini`, the default): headers libcmini lacks (`<osbind.h>`, `<compiler.h>` for GEMlib), our own `stat()` and `struct stat`, `access()`, `mkdir()`, `rindex()` |
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
- **Code called by the AES** (USERDEF objects: `xdraw_cicon()`, which draws
  the colour icons under TOS versions whose AES cannot) runs on the AES's
  own small supervisor stack. GEMlib's VDI bindings put their parameter
  arrays on the stack (`v_gtext()`: more than 2 KB), Lattice's used static
  arrays: the stack overflowed into AES variables, and the AES crashed
  (TOS 1.62, STE, colour: 2 then 3 bombs as soon as a menu dropped down).
  `xdraw_cicon()` now runs on its own stack.
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

## Removed

- The copy protection and registration (`protect.c`, `lib/encrypt.c`,
  `INSTALL.PRG`, `lib/GENREG.TTP`) and the demo version (`-DDEMO`): the
  program is free software now. `EDGE.PRG` is used straight from the
  distribution folder. Note: the "About" box of `EDGE.RSC` still has the
  "Registered to" and "Serial number" fields that `INSTALL.PRG` used to fill.
- The Lattice C project files (`*.PRJ`) and prebuilt libraries (`lib/*.LIB`);
  the GCC build compiles their sources (`lib/*.c`, `lib/compmem.s` via
  `compmem.S`).
