/*
 *	tos.h: Pure C <tos.h> for GCC (MiNTLib)
 *
 *	Part of the GCC port layer of Obsidian Edge: replaces a header of the
 *	Lattice C 5 library that the Diamond Edge sources were written for.
 *
 *	This file is distributed under the GPL, version 2 or at your
 *	option any later version.  See DOC/LICENSE.TXT for details.
 */
#ifndef OE_TOS_H
#define OE_TOS_H

#include <osbind.h>

/* file attributes */
#ifndef FA_SUBDIR
#define FA_RDONLY	0x01
#define FA_HIDDEN	0x02
#define FA_SYSTEM	0x04
#define FA_VOLUME	0x08
#define FA_SUBDIR	0x10
#define FA_ARCHIVE	0x20
#endif

/* Pure C: GEMDOS disk transfer address */
typedef struct {
	char d_reserved[21];
	unsigned char d_attrib;
	unsigned short d_time;
	unsigned short d_date;
	long d_length;
	char d_fname[14];
} DTA;

#endif
