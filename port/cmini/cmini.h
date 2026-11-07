/*
 *	cmini.h: additions to libcmini, for the libcmini build (make LIBC=cmini)
 *
 *	Part of the GCC port layer of Obsidian Edge.  Included in every C file
 *	of the libcmini build (gcc -include), after lattice.h.  Declares the
 *	few POSIX functions used by the sources that libcmini does not have;
 *	they are implemented in port/cmini/cmini.c (mkdir() and stat(): see
 *	port/cmini/sys/stat.h).
 *
 *	This file is distributed under the GPL, version 2 or at your
 *	option any later version.  See DOC/LICENSE.TXT for details.
 */
#ifndef OE_CMINI_H
#define OE_CMINI_H

#include <sys/types.h>

/* the sources only test whether a file exists: access(name,0) */
#ifndef F_OK
#define F_OK	0
#define X_OK	1
#define W_OK	2
#define R_OK	4
#endif
int access(const char *path,int mode);

char *rindex(const char *s,int c);

#endif
