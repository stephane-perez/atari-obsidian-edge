/*
 *	lattice.h: Lattice C library extensions, for the GCC build
 *
 *	Part of the GCC port layer of Obsidian Edge.  This header is included
 *	in every C file of the GCC build (gcc -include lattice.h); it declares
 *	the functions and macros of the Lattice C 5 library that the Diamond
 *	Edge sources use without including a specific header.  They are
 *	implemented in port/lattice.c and port/mediach.S.
 *
 *	This file is distributed under the GPL, version 2 or at your
 *	option any later version.  See DOC/LICENSE.TXT for details.
 */
#ifndef OE_LATTICE_H
#define OE_LATTICE_H

#ifndef __GNUC__
#error "lattice.h is only for the GCC build"
#endif

#define _GNU_SOURCE				/* MiNTLib: mrand48(), srand48() ... */
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

/* <stdlib.h> */
#ifndef min
#define min(a,b)	((a) < (b) ? (a) : (b))
#endif
#ifndef max
#define max(a,b)	((a) > (b) ? (a) : (b))
#endif

/* <stdio.h>: 16-bit and 32-bit big-endian (native) binary I/O */
int fgetw(FILE *fp);			/* returns 0..65535, or EOF */
int fputw(int w,FILE *fp);		/* returns w, or EOF */
long fgetl(FILE *fp);			/* returns the long; check ferror()/feof() */
long fputl(long l,FILE *fp);	/* returns l, or EOF */

/*
 *	<string.h>: swap the bytes of each 16-bit word.  The sources call it
 *	with from == to (FAT16 entries are little-endian), which POSIX swab()
 *	does not guarantee to support: use our own version.
 */
#define swab	oe_swab
void oe_swab(const void *from,void *to,ssize_t n);

/* <dos.h>: current drive (0 = A:), set the current drive (returns Drvmap()) */
int getdsk(void);
long chgdsk(int drive);

/* <io.h>: length of an open file */
long filelength(int fd);

/*
 *	<stdlib.h>: the Lattice C rand48 generator.  It is the POSIX linear
 *	congruential generator (same multiplier, increment and srand48()
 *	seeding), but Lattice's mrand48() returns the LOW 32 bits of the 48-bit
 *	state instead of the high ones.  lib/encrypt.c depends on it: with the
 *	POSIX version, INSTALL.PRG rejects the fixed serial number/registration
 *	code pair (E-0000-25 / DSFMDSCN) of the free version.
 */
#define srand48	oe_srand48
#define mrand48	oe_mrand48
void oe_srand48(long seed);
long oe_mrand48(void);

/* force GEMDOS to see a media change on 'drive' (0 = A:) */
int _mediach(int drive);

#endif
