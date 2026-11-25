Obsidian Edge @VERSION@
===================

Disk management utility for Atari ST/STE/TT/Falcon (FAT12/FAT16),
derived from Diamond Edge 2.57 (GPL release by Roger Burrows).
Source code: https://github.com/stephane-perez/atari-obsidian-edge

It is the Diamond Edge 2.57 source code, rebuilt with a modern compiler
(GCC) instead of Lattice C, without the copy protection and the demo
version, with bug fixes.  Some dialogs still say "Diamond Edge"; Obsidian
Mirror (DMIRROR.PRG, formerly Diamond Mirror) still says v2.57.
VFAT long file names are kept; FAT32 partitions are not supported.
Back up your disks before using it.

What has been tested
--------------------
In the Hatari emulator (EmuTOS; TOS 1.04 and 1.62; HDDriver 12.53 on ACSI
disks with AHDI and GPT partitions): Medic (test and repair), Optimize,
the disk information and fragmentation map, the FAT32 detection, with the
same results as Diamond Edge 2.57.  On real machines: Atari STE, Falcon030
(Optimize on a CompactFlash card) and Falcon CT60.  Undelete, Obsidian
Mirror in the AUTO folder, the partition and SCSI functions have not been
tested much yet.

How to install
--------------
1. Copy the OBSIDIAN folder to your hard disk.  Keep the HELP folder next
   to EDGE.PRG and EDGE.RSC.
2. Double-click on EDGE.PRG to run Obsidian Edge.
3. Optional: Obsidian Mirror runs at boot time; it saves the FAT and root
   directory of the drives you choose (which helps Undelete) and can test
   them, every boot, once a day or once a week.  Copy DMIRROR.PRG to the
   AUTO folder of your boot drive, then configure it in EDGE.PRG (Undelete
   menu, "Configure Mirror...").

License: GNU General Public License, version 2 (see LICENSE.TXT).
