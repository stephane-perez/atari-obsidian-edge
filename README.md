# Obsidian Edge

**Atari ST/TT/Falcon disk management utility, derived from Diamond Edge.**

Obsidian Edge works on FAT12 and FAT16 partitions on TOS-compatible systems.
Like its ancestor, it aims to provide:

- disk and partition validation (FAT, directories, lost clusters),
- defragmentation / optimisation,
- partition copying,
- file undeletion.

> **Status: version 0.0.1 (work in progress).** This is the first release of the
> fork. It is based on Diamond Edge 2.57, rebuilt with GCC, without the copy
> protection and the demo version; the disk functions are unchanged and not
> tested yet. Always back up your data before running any disk optimiser.

## Origin and credits

Obsidian Edge is a fork of **Diamond Edge**.

- Diamond Edge was created by Robert D. Luneski at **Oregon Research Associates**
  and sold as a commercial product in the 1990s.
- The GPL source release (version 2.57, January 2017) was published by
  **Roger Burrows / Anodyne Software** on SourceForge:
  <https://sourceforge.net/p/diamond-edge/>

This project is **not affiliated with, nor endorsed by**, Oregon Research
Associates, Anodyne Software or the original authors. "Diamond Edge" is the
name of the original product; this fork uses a different name to avoid any
confusion with it.

The full git history of the upstream repository is preserved in this repository.
The changes made to Diamond Edge are summarised in
[`DOC/CHANGES.TXT`](DOC/CHANGES.TXT).

```
Copyright (C) Robert D. Luneski / Oregon Research Associates
Copyright (C) 2002-2017 Roger Burrows / Anodyne Software
Copyright (C) 2026 Stephane Perez (Obsidian Edge)
```

The original copyright notices are kept unchanged in every source file.

## Compatibility

Target systems: Atari ST, STE, TT, Falcon and TOS-compatible clones (as upstream).
Limits depend on the TOS version and the hard disk driver; for example, TOS 1.04
to 3.06 supports FAT16 partitions up to 512 MiB.

## Building

The programs are cross-compiled on Linux with GCC (`m68k-atari-mint` toolchain,
MiNTLib and GEMlib). The original Lattice C project files (`*.PRJ`) are kept
for reference; the GCC build uses a small compatibility layer described in
[`port/README.md`](port/README.md).

Install the toolchain (Ubuntu/Debian), from Vincent Rivière's PPA:

```sh
sudo add-apt-repository ppa:vriviere/ppa
sudo apt-get update
sudo apt-get install cross-mint-essential
```

or, where the PPA cannot be reached, build it from his packaging repository
(about 16 minutes): `sh scripts/get-cross-mint.sh`.

The programs are linked with [libcmini](https://github.com/freemint/libcmini),
a small C library for TOS, instead of MiNTLib (which adds about 100 KB to
each program, for features they do not use). `scripts/get-libcmini.sh`
fetches it at a pinned commit, builds it and installs it in `.tools/libcmini`.

Then:

```sh
sh scripts/get-libcmini.sh
make            # build/EDGE.PRG, build/DMIRROR.PRG
make zip        # build/obsidian-edge-<version>.zip, ready to copy to the Atari
make LIBC=mintlib BUILD=build/mintlib   # the same with MiNTLib, for comparison
```

### Testing in Hatari

`make smoke-test EMUTOS=/path/to/etos512uk.img` runs `tools/smoke_test.py`:
in Hatari (ST, 4 MB, monochrome), it starts `EDGE.PRG` and checks that it
loads its resource file and gets through its start-up. Then, on an STE in
medium resolution (colour icons), it drops down a menu and checks that the
menu appears and that nothing crashed (this catches stack overflows in the
code the AES calls, see `port/callstack.S`), and opens a help file from the
Help menu (layout of the distribution). With a TOS 1.62 image,
add `SMOKE_MACHINE=ste`. Screenshots
and logs are kept in `build/smoke/`. It needs `hatari`, `xvfb` and
`xdotool`, and an EmuTOS image (free, from <https://emutos.sourceforge.io/>)
or a TOS ROM image you own (never commit TOS ROMs).

This only proves that the programs start and that their user interface
works in the emulator. **No disk function is tested yet, and nothing has
been tested on real hardware.**

### Continuous integration

`.github/workflows/build.yml` builds the programs on every push and pull
request (with libcmini, and with MiNTLib for comparison), checks that no
source file was converted to UTF-8 by an editor (see `.editorconfig`: the
inherited sources are 8-bit, CR LF), writes a table of
the program sizes compared with Diamond Edge 2.57 in the run summary, runs
the smoke test with EmuTOS on the libcmini build, and keeps the Atari files
and the screenshots as workflow artifacts.

## Versioning

Releases of this fork start at **0.0.1** and are independent from the upstream
numbering (upstream's last version is 2.57).

## License

Obsidian Edge is free software, released under the
**GNU General Public License, version 2** (June 1991), like the upstream
source release. See [`DOC/LICENSE.TXT`](DOC/LICENSE.TXT) for the full license
text (unchanged from the upstream repository; it is also in the distribution as
`LICENSE.TXT`).

This program is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE.

The complete corresponding source code is in this repository. Any binary
distribution (for example a `.PRG` file attached to a release) must be
accompanied by, or give a link to, the matching source code.

## Contributing

Issues and pull requests are welcome. By contributing, you agree that your
contribution is licensed under the same GPL v2 terms.

As required by section 2(a) of the GPL v2, a source file modified in this fork
carries a notice saying that it was changed, and when, below its original
copyright notice (for example `Modified for Obsidian Edge (November 2026): ...`).
User-visible changes are also listed in [`DOC/CHANGES.TXT`](DOC/CHANGES.TXT).

The sources inherited from Diamond Edge use the Atari 8-bit character set,
CR LF line ends and tabs of 4: see `.editorconfig`. Do not let an editor
convert them to UTF-8 (the CI rejects it).
