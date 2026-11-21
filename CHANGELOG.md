# Changelog

All notable changes to Obsidian Edge (fork of Diamond Edge 2.57) are listed
here: changes, fixes and improvements, including those to the build, the
tests and the documentation. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/); versions follow
the `VERSION` file.

User-visible changes are also summarised, in plain ASCII for the Atari, in
[`DOC/CHANGES.TXT`](DOC/CHANGES.TXT).

## [Unreleased]

### Fixed
- Disk functions (Medic, Optimize, Undelete, fragmentation map, Diamond
  Mirror): the end of a cluster chain and bad clusters were not recognised
  in the GCC build. Lattice C compared a `short` with an `unsigned short`
  on 16 bits; GCC promotes both to `int`, so `$FFFF` read into a `short`
  never equalled `dpb.bitmask`. Loops then ran through the FAT and wrote
  past their buffers: a Bus Error in the check before Optimize as soon as
  the disk had a folder. 31 comparisons fixed (found with a clang AST scan).
  Tested in Hatari (EmuTOS, ACSI disk, FAT16): Optimize and Medic (with
  6 injected defects) now give disk images identical to Diamond Edge 2.57.
  See issue #2.

## [0.0.1] - 2026-11

### Added
- GCC build (`m68k-atari-mint` cross compiler) with a Lattice C
  compatibility layer in `port/`, replacing the Lattice C project files.
- libcmini as the default C library, built from a pinned commit
  (`scripts/get-libcmini.sh`); `make LIBC=mintlib` builds with MiNTLib.
- Continuous integration: build, size report against the 2.57 binaries,
  and a Hatari smoke test under EmuTOS (`tools/smoke_test.py`).
- Dockerfile for the cross-compilation.
- Help topic "About Obsidian Edge" (credits, license, source code).
- GPL section 2(a) modification notices in the changed source files.

### Changed
- Renamed Obsidian Edge; the About box shows the fork's version.
- Source files and folders renamed to lowercase.
- The Technical Support help topic no longer refers to Anodyne Software's
  support and registration.
- NOTICE merged into the README, the help files and `DOC/CHANGES.TXT`.

### Removed
- Copy protection, registration, `INSTALL.PRG` and the demo version;
  Diamond Mirror's beta expiry date.
- Lattice C project files and prebuilt libraries; leftover protection
  definitions in `edgedefs.h`.

### Fixed
- Crash on colour screens (TOS 1.62, STE: bombs as soon as a menu dropped
  down): the colour icon drawing, called by the AES, now runs on its own
  stack.
- 8-bit Atari characters lost in a conversion to UTF-8.
