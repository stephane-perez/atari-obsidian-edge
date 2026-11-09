/*
 *	cookie.h: Lattice C <cookie.h> for GCC (MiNTLib)
 *
 *	Part of the GCC port layer of Obsidian Edge: replaces a header of the
 *	Lattice C 5 library that the Diamond Edge sources were written for.
 *
 *	This file is distributed under the GPL, version 2 or at your
 *	option any later version.  See DOC/LICENSE.TXT for details.
 */
#ifndef OE_COOKIE_H
#define OE_COOKIE_H

#define _CPU	0x5f435055L			/* '_CPU' */
#define _FRB	0x5f465242L			/* '_FRB' */
#define _MCH	0x5f4d4348L			/* '_MCH' */

/*
 *	Lattice: look for a cookie; returns non-zero if found, and if found
 *	and value is not NULL, stores the cookie value in *value
 */
int getcookie(long cookie,long *value);

#endif
