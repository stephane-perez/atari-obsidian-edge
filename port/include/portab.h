/*
 *	portab.h: Lattice C <portab.h> for GCC
 *
 *	Part of the GCC port layer of Obsidian Edge: replaces a header of the
 *	Lattice C 5 library that the Diamond Edge sources were written for.
 *
 *	This file is distributed under the GPL, version 2 or at your
 *	option any later version.  See DOC/LICENSE.TXT for details.
 */
#ifndef OE_PORTAB_H
#define OE_PORTAB_H

#include <stddef.h>

typedef char			BYTE;
typedef unsigned char	UBYTE;
typedef short			WORD;
typedef unsigned short	UWORD;
typedef long			LONG;
typedef unsigned long	ULONG;

#ifndef TRUE
#define TRUE	1
#define FALSE	0
#endif

#ifndef VOID
#define VOID	void
#endif

/* storage classes and types of the Digital Research / Atari portab.h */
#define MLOCAL	static
#define LOCAL	static
#define GLOBAL
#define EXTERN	extern
#define REG		register
#define BOOLEAN	int

/* Lattice C keywords without meaning for GCC */
#define __saveds
#define __stdargs

#ifndef CONST
#define CONST	const
#endif

#endif
