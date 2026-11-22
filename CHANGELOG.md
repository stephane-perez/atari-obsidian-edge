# Changelog

All notable changes to Obsidian Edge (fork of Diamond Edge 2.57) are listed
here: changes, fixes and improvements, including those to the build, the
tests and the documentation. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/); versions follow
the `VERSION` file.

User-visible changes are also summarised, in plain ASCII for the Atari, in
[`DOC/CHANGES.TXT`](DOC/CHANGES.TXT).

## [Unreleased]

### Added
- FAT32 partitions (not supported) are recognised, from their partition
  type given by the XHDI driver (`F32`, or DOS types `$0B`/`$0C`/`$1B`/`$1C`)
  or from their boot sector. Choosing one shows "FAT32 partitions are not
  supported by Obsidian Edge." instead of a boot sector read error, and the
  active drive does not change; "All Info" shows "FAT32: not supported" for
  it; the multi-drive functions and "Save info" skip it; drive C: is not
  the default drive at start-up if it is FAT32. The texts are in the code
  (`edgedefs.h`), not in `EDGE.RSC` yet.

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
- Medic, unreadable directory cluster (fix mode): the cluster count was
  not decremented (`*ncl--` decremented the pointer), and the cluster
  dropped from the chain was the wrong one: the next, readable, cluster was
  marked bad and its link lost, so the scan then ran through cluster
  `$FFF7`; when the bad cluster was the last one, `fat16[-1]` was
  overwritten. Both copies (`load_directory()`, `load_directory_bad()`).
  Tested in Hatari with an unreadable first, middle and last folder
  cluster: the folder chain is repaired, the cluster is marked bad and the
  disk is clean for `fsck.fat`; Diamond Edge 2.57 hangs on the middle case
  and leaves the last one unmarked.
- Medic: `*problems++` (pointer) instead of `(*problems)++` when the two
  FATs differ.
- Disk functions and Diamond Mirror: a partition with more than 32765 data
  clusters overflowed the cluster counts (`short`) and the old check could
  never fail; such partitions are now refused. A boot sector with 0
  sectors per cluster no longer divides by zero.
- Undelete: the error message after a failed seek in the FAT16 mirror file
  showed an uninitialised value.
- SCSIDRV `phys_read()`/`phys_write()`: uninitialised return value for 0
  sectors.
- File viewer: only the first line offset of a new text block was
  initialised (missing `lptr++`).
- Resource loader: `R_IMAGEDATA` read a byte instead of a pointer (not
  used by the program).

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
