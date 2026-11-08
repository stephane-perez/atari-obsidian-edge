# Obsidian Edge

**Atari ST/TT/Falcon disk management utility, derived from Diamond Edge.**

Obsidian Edge works on FAT12 and FAT16 partitions on TOS-compatible systems.
Like its ancestor, it aims to provide:

- disk and partition validation (FAT, directories, lost clusters),
- defragmentation / optimisation,
- partition copying,
- file undeletion.

> **Status: version 0.0.1 (work in progress).** This is the first release of the
> fork. It is based on Diamond Edge 2.57 and does not yet contain functional
> changes. Always back up your data before running any disk optimiser.

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
See [`NOTICE`](NOTICE) for copyright and modification details.

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
make            # build/EDGE.PRG, build/DMIRROR.PRG, build/INSTALL.PRG
make zip        # build/obsidian-edge-<version>.zip, ready to copy to the Atari
make LIBC=mintlib BUILD=build/mintlib   # the same with MiNTLib, for comparison
```

### Testing in Hatari

`make smoke-test EMUTOS=/path/to/etos512uk.img` runs `tools/smoke_test.py`:
in Hatari (ST, 4 MB, monochrome), it runs `INSTALL.PRG`, fills in and
validates its dialog, checks the installed files, then starts the installed
`EDGE.PRG` and checks that it gets past the registration check. Then, on an
STE in medium resolution (colour icons), it drops down a menu and checks
that the menu appears and that nothing crashed: this catches stack overflows
in the code the AES calls (see `port/callstack.S`). With a TOS 1.62 image,
add `SMOKE_MACHINE=ste`. Screenshots
and logs are kept in `build/smoke/`. It needs `hatari`, `xvfb` and
`xdotool`, and an EmuTOS image (free, from <https://emutos.sourceforge.io/>)
or a TOS ROM image you own (never commit TOS ROMs).

This only proves that the programs start and that their user interface
works in the emulator. **No disk function is tested yet, and nothing has
been tested on real hardware.**

### Continuous integration

`.github/workflows/build.yml` builds the programs on every push and pull
request (with libcmini, and with MiNTLib for comparison), writes a table of
the program sizes compared with Diamond Edge 2.57 in the run summary, runs
the smoke test with EmuTOS on the libcmini build, and keeps the Atari files
and the screenshots as workflow artifacts.

## Versioning

Releases of this fork start at **0.0.1** and are independent from the upstream
numbering (upstream's last version is 2.57).

## License

Obsidian Edge is free software, released under the
**GNU General Public License, version 2** (June 1991), like the upstream
source release. See [`COPYING`](COPYING) for the full license text
(copied unchanged from the upstream repository).

This program is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE.

## Contributing

Issues and pull requests are welcome. By contributing, you agree that your
contribution is licensed under the same GPL v2 terms.
