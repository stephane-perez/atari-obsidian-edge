# CLAUDE.md — Obsidian Edge

Notes for Claude sessions working on this repository. Obsidian Edge is a fork
of Diamond Edge 2.57 (Atari ST/STE/TT/Falcon disk utility, GPL v2), rebuilt
with GCC (`m68k-atari-mint`) and libcmini. See `README.md`, `port/README.md`
(the Lattice C → GCC port layer) and `CHANGELOG.md`.

## Working with the owner

- Talk to the owner in **French**; code, comments, commits, CHANGELOG in English.
- **`EDGE.RSC` is edited by the owner** with the Interface resource editor:
  list the strings or objects to change (tree / object number / name from
  `edge.h`, or free string index) rather than editing it, unless asked.
  `EDGE.HRD` holds the names (Interface), `edge.h` the generated indices.
- Record every change, fix or improvement in **`CHANGELOG.md`** (Keep a
  Changelog, section `[Unreleased]`); user-visible changes also in
  `DOC/CHANGES.TXT` (plain ASCII, shipped).
- GPL §2(a): a modified source file gets a line `Modified for Obsidian Edge
  (<Month> <Year>): ...` below its copyright notice.
- Never commit TOS ROMs, disk images or the Diamond Edge manual. Private
  test material (ROMs, HDDriver disk images) lives in the private repository
  `stephane-perez/atari-common-tools` (`tos/`, `diskimages/harddisks/`); follow
  its `CLAUDE.md` (never copy its files elsewhere).
- Versioning: `VERSION` and `version.h` (`PROGRAM_VERSION`), shared by
  `EDGE.PRG` and `DMIRROR.PRG` (Obsidian Mirror). The owner creates the tags.

## Sources

- Inherited sources are **8-bit (Atari character set) with CR LF**: edit
  them as bytes (Python `latin-1`, keep `\r\n`); never convert to UTF-8 (CI
  checks for U+FFFD). Help files `HELP/*.HLP` are CR LF too.
- `tools/clang_check.py` (run after `make`, also in CI): zero clang warnings,
  and no comparison between a `short` and an `unsigned short` (Lattice C
  compared on 16 bits, GCC on `int`: cluster `$FFFF` in a `short` is -1, never
  equal to `dpb.bitmask` 65535 → that was the big FAT bug). Use
  `(short)dpb.bitmask` / `(short)dpb.badmask` for 16-bit comparisons.
- Cluster numbers are `short` everywhere: partitions are limited to
  `MAX_DATA_CLUSTERS` (32765). FAT32 is not supported (detected with XHDI
  `XHInqDev2` partition id `F32` / DOS 0B,0C,1B,1C, or the boot sector).
- VFAT long names (`lfn.c`): a run of `$0F` entries followed by the 8.3 entry
  whose checksum matches is one unit; `sort_units()` moves it as a whole.
- `dmirror/dmirror.c` duplicates much of `medic1.c`/`opt2.c` (fix both).
- Lattice `fopen(..., "ra")` removed CRs; the GCC libraries do not.

## Testing (Hatari)

- `make smoke-test EMUTOS=<etos512uk.img>`: start-up and menus only.
- Disk functions: differential tests against the original Diamond Edge 2.57
  binaries (SourceForge `v257_binaries.zip`, also used by the CI), on copies
  of the same disk image, then byte comparison of the images, `fsck.fat -n`,
  per-file hashes (mtools `-i img@@offset`). The harness scripts (mkhd.py,
  fattool.py, drive.py, cmp16.py, mkbad.py, mklfn.py) are in the appendix
  and comments of issue #2 (not committed yet).
- EmuTOS mounts AHDI ACSI images itself (no driver needed). For HDDriver:
  TOS 1.62 on `--machine ste`, `--acsi 0=<image>`; the 512 MB SD image
  boots HDDriver from its root sector (BGM + F32 partitions); for the 32 MB
  GPT image, run `HDDRIVER.PRG` from the AUTO folder of a floppy.
- Use `--fast-forward on` (AltGr+X) to speed Hatari up; Optimize under
  EmuTOS needs several minutes of emulated time (an interrupted Optimize
  corrupts file contents silently: wait long enough).
- Simulating a read error: move a cluster to the end of the partition and
  truncate the image there (Hatari's ACSI then fails the read).
- Screen positions (ST high, 640x400): Optimize Alt-O then Yes (287,247),
  Yes (282,247); Medic report buttons Fix now / Continue at (43,292); FAT
  choice "FAT #1" at (232,231); read-error alert "Continue" at (370,247).

## How the program works (Diamond Edge 2.57 manual)

Manual (CC BY-SA 4.0, not in the repository):
<https://sourceforge.net/projects/diamond-edge/files/releases/v2.57/v257_manual.pdf/download>

- Almost every function works on the **Active Drive** (button bar, or the
  drive letter key); exceptions: SCSI-level functions and drive copies.
- **Preferences** (Alt-F): Novice/Expert, warning mode, Medic pause delay
  (seconds before continuing after an error-free Medic pass), default path
  for the files below. Saved in `EDGE.INF`, with the menu check marks.
- **Medic**: Test Disk Structure (Alt-T) / Test Multiple; options Auto-Fix,
  Ignore Illegal Names, Save Lost Clusters (`LOSTnnnn.DAT` in the root),
  Visual Update. Report counts: FAT1≠FAT2, invalid directories, unreadable
  dir sectors, illegal filenames (and duplicates), bad directory entries,
  inconsistent file size, bad FAT entries, FAT chain collisions, lost
  clusters, bad clusters. Without Auto-Fix: test pass, then Fix now / View
  (log `MEDIC_X.ERR`) / Cancel. FAT1≠FAT2: choose a FAT for the pass.
- **Validation**: Create CRC/Sum file (`DRIVE_X.VAL`), Validate (`VALID_X.ERR`).
- **Map Bad Drive** (Alt-B): read-read, read-write-read, write-read
  (destructive); Fix now marks bad clusters then runs Medic.
- **Partition Hard Disk** (Alt-H, needs SCSIDRV, e.g. HDDriver): edit
  physical sector 0 (AHDI/ICD, GEM/BGM, TOS 1.00/1.04/4.00 limits, up to 64
  partitions, active/boot flags); install & rebuild all / one, partition
  info only, boot sector only.
- **Optimize**: Full Optimization or Compress Free Space; priority Reading
  (data at the start; for C: the AUTO folder files first, order kept) or
  Writing (data at the end); option Remove Deleted. A Medic pass runs first
  and errors must be fixed. Not abortable. Alt-O / Ctrl-O (multiple).
- **Undelete** (Alt-U): Simple (consecutive free clusters from the start
  cluster) or Mirror (uses the Mirror file's FAT); shows Yes / No / m/n
  recoverable; search mask with * and ?; asks for the first character.
- **Mirror** (Obsidian Mirror, `DMIRROR.PRG` in AUTO, configured with Alt-N):
  at boot, Test and/or Mirror (saves FAT + root) per drive, every boot /
  daily / weekly, files in a chosen folder; config in `DMIRROR.INF`.
- **Save/Restore Disk Mirror** (Alt-S/Alt-Y: boot sector, FATs, root),
  **Save/Restore SCSI Mirror** (Ctrl-S/Ctrl-Y: physical sector 0).
- **Utility**: Copy Drive Image (Ctrl-I, same size/type), Copy Drive Defrag
  (Ctrl-C), Force Media Change (Ctrl-M), View/Print File, Browse Disk,
  Zero (saves a mirror first) / Un-Zero / Wipe (need "Enable zeroing").
- Help F1–F5 (File, Medic, Optimize, Undelete, Utility). Ctrl-C aborts most
  functions, not Optimize nor Medic.

## Open items

- Not tested much: Undelete, Obsidian Mirror from AUTO, partitioning / SCSI
  functions, Map Bad, Copy Drive, Zero/Wipe, Validation, floppies on real
  hardware, TT / TOS 2.06 / 4.0x / MagiC / FreeMiNT.
- Mirror configuration shows the path as "G:\NNNN…" in Hatari: the owner
  checks on real hardware.
- Ideas: backup of FAT + root before Optimize; differential tests in CI;
  unit tests; `CLUSTER` type and a shared FAT module (issue #2).
