/*
 *	dos.h: Lattice C <dos.h> for GCC (MiNTLib)
 *
 *	Part of the GCC port layer of Obsidian Edge: replaces a header of the
 *	Lattice C 5 library that the Diamond Edge sources were written for.
 *	Declares the GEMDOS/BIOS/XBIOS bindings (from MiNTLib) and the few
 *	Lattice library extensions used by the sources, which are implemented
 *	in port/lattice.c.
 *
 *	This file is distributed under the GPL, version 2 or at your
 *	option any later version.  See DOC/LICENSE.TXT for details.
 */
#ifndef OE_DOS_H
#define OE_DOS_H

#include <osbind.h>
#include <mintbind.h>

/* file attributes */
#ifndef FA_SUBDIR
#define FA_RDONLY	0x01
#define FA_HIDDEN	0x02
#define FA_SYSTEM	0x04
#define FA_VOLUME	0x08
#define FA_SUBDIR	0x10
#define FA_ARCHIVE	0x20
#endif

/* Lattice: Fseek() modes */
#define FSEEK_SET	0
#define FSEEK_CUR	1
#define FSEEK_END	2

/* Lattice names of the BIOS/XBIOS structures */
typedef _BPB BPB;
typedef _KEYTAB KEYTAB;

#define FNSIZE	13			/* max size of a file name: 8.3 + NUL */
#define FMSIZE	128			/* max size of a full path name */

/* Lattice: GEMDOS error code of the last failed library call */
extern int _OSERR;

/* Lattice: TOS version (from the OS header), e.g. 0x0104 */
unsigned short oe_tos_version(void);
#define _tos	(oe_tos_version())

/* Lattice: as returned by dfind()/dnext() */
struct FILEINFO {
	char resv[21];
	char attr;
	unsigned short time;
	unsigned short date;
	long size;
	char name[14];
};

int dfind(struct FILEINFO *info,const char *name,int attr);
int dnext(struct FILEINFO *info);

/* Lattice: file name manipulation */
void stcgfn(char *name,const char *path);
void strmfp(char *path,const char *dir,const char *name);
void strmfe(char *newname,const char *oldname,const char *ext);

/* Lattice: file date/time, as a long (date << 16 | time) */
long getft(int fd);
int chgft(int fd,long ft);

#endif
