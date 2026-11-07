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

The upstream project provides the C and 68000 assembly sources (`*.C`, `*.H`,
`EDGEASM.S`), the GEM resource files and the original project files
(`EDGE.PRJ`). Build instructions for this fork will be added here once the
toolchain is set up.

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
