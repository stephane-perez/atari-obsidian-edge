/*
 *	oserr.h: Lattice C <oserr.h> for GCC
 *
 *	Part of the GCC port layer of Obsidian Edge: replaces a header of the
 *	Lattice C 5 library that the Diamond Edge sources were written for.
 *
 *	This file is distributed under the GPL, version 2 or at your
 *	option any later version.  See DOC/LICENSE.TXT for details.
 */
#ifndef OE_OSERR_H
#define OE_OSERR_H

/* BIOS/GEMDOS error codes, as positive numbers (the calls return -Exxx) */
#define EDRVNR		2		/* drive not ready */
#define EUNCMD		3		/* unknown command */
#define E_CRC		4		/* CRC error */
#define EBADRQ		5		/* bad request */
#define E_SEEK		6		/* seek error */
#define EMEDIA		7		/* unknown media */
#define ESECNF		8		/* sector not found */
#define EPAPER		9		/* out of paper */
#define EWRITF		10		/* write fault */
#define EREADF		11		/* read fault */
#define EWRPRO		13		/* write protected */
#define E_CHNG		14		/* media change */
#define EUNDEV		15		/* unknown device */
#define EBADSF		16		/* bad sectors on format */
#define EOTHER		17		/* insert other disk */

extern int _OSERR;

#endif
