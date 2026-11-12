Obsidian Edge @VERSION@
===================

Disk management utility for Atari ST/STE/TT/Falcon (FAT12/FAT16),
derived from Diamond Edge 2.57 (GPL release by Roger Burrows).
Source code: https://github.com/stephane-perez/atari-obsidian-edge

THIS IS A TEST BUILD.  It is the Diamond Edge 2.57 source code, rebuilt
with a modern compiler (GCC) instead of Lattice C, without the copy
protection and the demo version.  The window titles and dialogs still say
"Diamond Edge"; Diamond Mirror still says v2.57.
Back up your disks before using it.

What has been tested
--------------------
Only in the Hatari emulator, with EmuTOS 1.3: EDGE.PRG starts and shows
its menus and help files (ST monochrome, and STE in medium resolution with
the colour icons).  NO DISK FUNCTION HAS BEEN TESTED YET, Diamond Mirror
has not been tried from the AUTO folder, and nothing has been tried on a
real machine.

How to install
--------------
1. Copy the OBSIDIAN folder to your hard disk.  Keep the HELP folder next
   to EDGE.PRG and EDGE.RSC.
2. Double-click on EDGE.PRG to run Obsidian Edge.
3. Optional: Diamond Mirror runs at boot time; it saves the FAT and root
   directory of the drives you choose (which helps Undelete) and can test
   them, every boot, once a day or once a week.  Copy DMIRROR.PRG to the
   AUTO folder of your boot drive, then configure it in EDGE.PRG (Undelete
   menu, "Configure Mirror...").

License: GNU General Public License, version 2 (see LICENSE.TXT).
