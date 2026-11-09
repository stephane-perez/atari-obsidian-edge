/*
 *	sys/stat.h: struct stat and stat(), for the libcmini build (make LIBC=cmini)
 *
 *	Part of the GCC port layer of Obsidian Edge.  libcmini has no
 *	<sys/stat.h>, and the layout of the struct stat its stat() fills in
 *	depends on the headers found when libcmini itself is compiled.  The
 *	sources only use stat() to get the size of a file, so we use our own
 *	stat() (port/cmini/cmini.c) and our own minimal struct stat.
 *
 *	This file is distributed under the GPL, version 2 or at your
 *	option any later version.  See DOC/LICENSE.TXT for details.
 */
#ifndef OE_SYS_STAT_H
#define OE_SYS_STAT_H

#include <sys/types.h>

struct stat {
	unsigned short st_mode;
	long st_size;
	long st_mtime;			/* GEMDOS date << 16 | GEMDOS time */
};

#define S_IFMT	0170000
#define S_IFDIR	0040000
#define S_IFREG	0100000
#define S_ISDIR(m)	(((m) & S_IFMT) == S_IFDIR)
#define S_ISREG(m)	(((m) & S_IFMT) == S_IFREG)

int stat(const char *path,struct stat *st);
int mkdir(const char *path,mode_t mode);	/* mode is ignored */

#endif
