/*
 *	compiler.h: what GEMlib's <mt_gem.h> needs from MiNTLib's <compiler.h>
 *
 *	Part of the GCC port layer of Obsidian Edge, for the libcmini build
 *	(make LIBC=cmini): header that libcmini does not provide.
 *
 *	This file is distributed under the GPL, version 2 or at your
 *	option any later version.  See DOC/LICENSE.TXT for details.
 */
#ifndef _COMPILER_H
#define _COMPILER_H 1

#include <sys/cdefs.h>		/* __BEGIN_DECLS, __END_DECLS */

#ifndef __CDECL
#define __CDECL				/* parameters on the stack: the GCC default */
#endif

#endif
