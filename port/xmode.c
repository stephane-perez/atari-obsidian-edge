/*
 *	xmode.c: Lattice C _XMODE variable, for the GCC build of DMIRROR.PRG
 *
 *	Diamond Mirror was linked with Lattice's AUTO-folder startup code,
 *	which set _XMODE to 3 when the program runs from the AUTO folder (i.e.
 *	before the AES is started).  This file sets it the same way, before
 *	main() runs: if appl_init() leaves the AES global array untouched,
 *	there is no AES yet.
 *
 *	This file is distributed under the GPL, version 2 or at your
 *	option any later version.  See DOC/LICENSE.TXT for details.
 */
#include "include/oe_gem.h"

int _XMODE;

static void __attribute__((constructor)) set_xmode(void)
{
	aes_global[0] = 0;					/* AES version: set by appl_init() */
	appl_init();
	if (aes_global[0] == 0) {
		_XMODE = 3;						/* no AES: AUTO folder */
		return;
	}
	appl_exit();
	_XMODE = 0;
}
