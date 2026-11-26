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
  When asked to edit it: `tools/rsc_patch.py` (replace texts, add free
  strings; keeps the colour icon extension), then add the names to `edge.h`
  and `EDGE.HRD` (record: type byte 2 = alert / 3 = free string, 0, tree or
  string index (word), object (word), name, 0), check the object widths, and
  look at the dialogs in Hatari. Alert lines: ≤ 30 characters.
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
- The French TOS ROMs use an AZERTY keyboard: digits need Shift in xdotool
  (`shift+2`), and a key may be repeated; prefer buttons to typed numbers.
- Screen positions (ST high, 640x400): Optimize Alt-O then Yes (287,247),
  Yes (282,247); Medic report buttons Fix now / Continue at (43,292); FAT
  choice "FAT #1" at (232,231); read-error alert "Continue" at (370,247).

## Atari disk structures (manual chapter 2, and what the code does with them)

- **Physical sector 0** (root sector, AHDI): `0x1C2` disk size (long, in 512-
  byte sectors), 4 partition entries of 12 bytes at `0x1C6`, `0x1D2`, `0x1DE`,
  `0x1EA`: flag byte (bit 0 active, bit 7 bootable), 3-character id, start
  sector (long), size in sectors (long); bad sector list start `0x1F6`, size
  `0x1FE`. Big-endian (Motorola). Root sector word checksum `$1234` =
  bootable (driver code, e.g. HDDriver on the 512 MB SD image).
- Ids: **GEM** (logical sector = 512 bytes; ≤16 MB on TOS 1.0/1.02, ≤32 MB on
  TOS ≥1.04), **BGM** (logical sectors of 512·2^n bytes; the driver maps them
  to 512-byte physical sectors), **XGM** (extended: points to another
  pseudo-root sector, chainable, for more than 4 partitions); also `F32`
  (FAT32, HDDriver), `ACK`/`OOP` (Spectre), `OS9`. ICD/Supra: up to 12
  entries in sector 0, extra ones at `0x156`…`0x1AA` (step 12); an Atari
  driver only sees the first 4. TOS 1.00 partitions ≤ 32767 sectors, TOS 1.04
  ≤ 65535, TOS 4.00 logical sectors up to 16384 bytes.
- **Not in the manual, and not handled by Edge**: DOS **MBR** disks (`55AA`
  signature, table at `0x1BE`, which overlaps the AHDI entries) and **GPT**
  disks (`EFI PART` in sector 1), used by EmuTOS, HDDriver and PC-formatted
  cards (the Falcon CF image is MBR, the 32 MB SD image is GPT). The
  partition editor (`part1.c`, `load_root_sectors()`) reads sector 0 as AHDI
  only: installing partition info there would destroy an MBR/GPT table.
  Such disks (and hybrid AHDI+DOS sectors) are now refused:
  `is_foreign_root_sector()` / `refuse_foreign_disk()` in part1.c. F32
  partitions are protected in the editor: `is_kept_f32()` before sector 0
  is written, and `rebuild()` only formats GEM/BGM partitions.
- **Partition layout**: boot sector (logical 0) | reserved | FAT #1 | FAT #2 |
  root directory (fixed size) | data area (clusters from number 2). FAT n at
  `res + n·spf`, root at `res + nfat·spf`, data at that + root sectors
  (`dpb.firstd`). Control sectors = everything before the data area.
- **Boot sector** (little-endian, Intel): `0x00` branch, `0x02` OEM (6),
  `0x08` 24-bit serial (media change), `0x0B` bytes/sector, `0x0D`
  sectors/cluster (usually 2), `0x0E` reserved sectors, `0x10` number of
  FATs (2), `0x11` root entries, `0x13` total sectors (16-bit), `0x15` media
  (ignored by the Atari BIOS), `0x16` sectors/FAT, `0x18` sectors/track,
  `0x1A` sides, `0x1C` hidden sectors; checksum `$1234` over the sector =
  bootable (floppies). Edge: `load_mybpb()` (disk.c, copy in dmirror.c),
  `MYBPB dpb`.
- **FAT**: 12-bit (≤ 4096 clusters, floppies; 2 entries in 3 bytes) or 16-bit
  (little-endian). Special values: 0 free, `$FF7`/`$FFF7` bad, `$FFF`/`$FFFF`
  end of chain. Edge converts both into big-endian 16-bit tables in memory
  (`fat12_to_fat16()`, `swab()`), with `dpb.bitmask` (end) and `dpb.badmask`
  (bad) = `$FFF`/`$FF7` or `$FFFF`/`$FFF7`. Microsoft's spec allows any of
  `$FFF8`–`$FFFF` (`$FF8`–`$FFF`) as end of chain: `fat_normalize_eoc()`
  (fatutil.c) turns them into `dpb.bitmask` when a FAT is loaded (Diamond
  Edge 2.57 reported them as bad FAT entries). Call it after any new FAT load. Two copies of the FAT; Medic
  compares them.
- **Directory entry** (32 bytes): name 8 + extension 3 (space padded, no
  dot), attribute (`0x01` RO, `0x02` hidden, `0x04` system, `0x08` volume,
  `0x10` directory, `0x20` archive; `0x0F` = VFAT long name), 10 reserved
  bytes (`0x0C`; VFAT systems store creation time, access date, high cluster
  there: Medic counts non-zero as one "garbage" point), time `0x16`, date
  `0x18`, first cluster `0x1A` (16-bit), size `0x1C` (32-bit), all
  little-endian. Deleted: first byte `$E5` (FAT chain cleared; Undelete
  guesses consecutive free clusters, or uses the Mirror FAT). Subdirectories
  are files with a FAT chain, starting with `.` and `..`, grown one cluster
  at a time (→ directory fragmentation, which the frag map shows by its
  drawing order). Root directory has a fixed size.
- **Optimize**: Full = folders contiguous, then each file contiguous, files of
  a folder consecutive; Compress Free Space = only removes holes. Read
  priority packs data at the start (C: AUTO folder files and root first, AUTO
  order kept), write priority at the end. Diagrams: manual pages 8 and 41.
- Causes of damage the tools address: crashes while writing (run Medic),
  weak media (Map Bad, several passes), lost sector 0 / boot sector / FAT /
  root (Save/Restore SCSI and Disk Mirror, emergency floppy), power loss
  during Optimize (nothing protects against it: backups).

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
