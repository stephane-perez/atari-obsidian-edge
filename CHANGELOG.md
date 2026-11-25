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
- Partitioning, Restore SCSI Mirror: disks with a DOS (MBR) or GPT partition
  table (EmuTOS, HDDriver, PC-formatted CompactFlash and SD cards) are now
  refused with a message. Their sector 0 was read as an Atari (AHDI) root
  sector, whose entries overlap the DOS table: installing partition
  information would have destroyed it. Hybrid sectors (Atari and DOS tables)
  are refused too. The check is also made just before sector 0 is written.
- End of FAT chains written as `$FFF8`–`$FFFE` (`$FF8`–`$FFE` in a 12-bit
  FAT), allowed by Microsoft's specification, were taken for bad FAT entries:
  Medic reported every such file and folder and "Fix now" truncated them.
  They are now read as end of chain (`fatutil.c`, Edge and Obsidian Mirror).
  Tested in Hatari: on a disk whose 85 chains end with `$FFF8`, Diamond Edge
  2.57 reports 82 bad FAT entries and 3 invalid directories, Obsidian Edge
  none.

### Changed
- Obsidian Mirror (`DMIRROR.PRG`) has the same version as Obsidian Edge
  (it showed 2.57); its "run Edge immediately" message names Obsidian Edge.
- Help files: "Diamond Edge" replaced by "Obsidian Edge", except the
  "Diamond Edge Extensible Help System" headers, the references to the
  Diamond Edge manual and the credits. The "About Obsidian Edge" help topic,
  `README.md` and `README.TXT` give the address of the Diamond Edge 2.57
  manual (PDF, not included).

## [0.1.0] - 2026-11-25

### Changed
- Diamond Mirror renamed Obsidian Mirror in its messages, in `EDGE.RSC`
  (the "Mirror not found" alert and the configuration dialog title), the
  help files and the documentation; the program is still `DMIRROR.PRG`,
  version 2.57.
- `README.TXT` of the distribution: what has been tested.
- About box (`EDGE.RSC`, edited with Interface): "About Obsidian Edge",
  "Based on Diamond Edge 2.57", the fork's copyright; the "Registered to"
  and "Serial number" fields are gone.

### Added
- CI: static checks with clang (`tools/clang_check.py`): any clang warning,
  and any comparison between a `short` and an `unsigned short` (16 bits in
  Lattice C, `int` in GCC: the cause of the FAT end-of-chain bug), fails
  the build.
- FAT32 partitions (not supported) are recognised, from their partition
  type given by the XHDI driver (`F32`, or DOS types `$0B`/`$0C`/`$1B`/`$1C`)
  or from their boot sector. Choosing one shows "FAT32 partitions are not
  supported by Obsidian Edge." instead of a boot sector read error, and the
  active drive does not change; "All Info" shows "FAT32: not supported" for
  it; the multi-drive functions and "Save info" skip it; drive C: is not
  the default drive at start-up if it is FAT32. The two texts are free
  strings of `EDGE.RSC` (`FAT32_ALERT`, `FAT32_TEXT`; names in `EDGE.HRD`
  and `edge.h`).

### Fixed
- VFAT long file names (written by Windows, Linux, macOS or MiNT) were
  destroyed: Medic reported them as bad directory entries and "Fix now"
  deleted them; sorting the root directory (Medic and Optimize) took them
  for extra volume labels (they have the volume bit) and deleted them; and
  moving the folders to the start of a directory separated them from their
  8.3 entry. A long name followed by the 8.3 entry that matches its
  checksum is now valid, and moved with that entry (`lfn.c`, shared by
  Edge and Diamond Mirror); only orphans (e.g. left by TOS when it deletes
  a file) are reported and removed. The folder loops of Medic, Optimize and
  Diamond Mirror skip long names instead of stopping at them, and Undelete
  no longer lists deleted long name entries. Tested in Hatari: Medic and
  Optimize keep every long name and file content (`fsck.fat` clean), and
  give the same images as Diamond Edge 2.57 on disks without long names.
- File viewer (help files, F1 to F5) and "Print file": a CR glyph was shown
  at the end of every line of CR LF text files. Lattice C's `"ra"` mode
  removed the CR; the GCC C libraries do not, so the line reading functions
  now drop it.
- `EDGE.INF`: the drives selected for the multi-drive functions were read
  back wrongly when their mask (`%08lx`) had a hexadecimal digit from A to
  F: the `vsscanf()` of `section.c` (Edge and Diamond Mirror) used the digit
  string `"01234567890ABCDEF"`, with a second `0`, so `A` was read as 11 and
  `F` stopped the number (the copy also overflowed its 17-byte buffer by
  one byte).
- Optimize: `newfat[nextcluster] = ++nextcluster` (unspecified order of
  evaluation) written explicitly, as intended.
- All the clang (`-Wformat`, `-Wconditional-uninitialized`,
  `-Wimplicit-fallthrough`, `-Wdangling-else`, ...) and GCC warnings: `long`
  values printed with `%ld`, `ob_spec` passed as `char *` to `%s`, variables
  initialised, intentional fall-throughs marked with `FALLTHROUGH`
  (`port/include/lattice.h`), braces for an ambiguous `else`, 8-bit Atari
  characters in string literals written as octal escapes (same bytes),
  unused code removed.
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
