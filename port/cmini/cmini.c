/*
 *	cmini.c: additions to libcmini, for the libcmini build (make LIBC=cmini)
 *
 *	Part of the GCC port layer of Obsidian Edge: the POSIX functions used by
 *	the sources that libcmini does not provide (see cmini.h).
 *
 *	This file is distributed under the GPL, version 2 or at your
 *	option any later version.  See DOC/LICENSE.TXT for details.
 */
#include <errno.h>
#include <string.h>
#include <mint/osbind.h>
#include "cmini.h"
#include "sys/stat.h"

/*
 *	access(): GEMDOS has no permissions, only the read-only attribute
 */
int access(const char *path,int mode)
{
	long attr = Fattrib(path,0,0);

	if (attr < 0) {
		errno = (int)-attr;			/* libcmini errno values are GEMDOS codes */
		return -1;
	}
	if ((mode & W_OK) && (attr & 0x01)) {		/* FA_RDONLY */
		errno = EACCES;
		return -1;
	}
	return 0;
}

int mkdir(const char *path,mode_t mode)
{
	long rc = Dcreate(path);

	(void)mode;
	if (rc < 0) {
		errno = (int)-rc;
		return -1;
	}
	return 0;
}

char *rindex(const char *s,int c)
{
	return strrchr(s,c);
}

/*
 *	stat(): only what the sources use (size; type and date for completeness)
 */
int stat(const char *path,struct stat *st)
{
	_DTA dta, *old = Fgetdta();
	long rc;

	Fsetdta(&dta);
	rc = Fsfirst(path,0x17);		/* FA_RDONLY|FA_HIDDEN|FA_SYSTEM|FA_DIR */
	Fsetdta(old);
	if (rc < 0) {
		errno = (int)-rc;
		return -1;
	}
	st->st_mode = (dta.dta_attribute & 0x10) ? S_IFDIR : S_IFREG;
	st->st_size = dta.dta_size;
	st->st_mtime = ((long)dta.dta_date << 16) | dta.dta_time;
	return 0;
}
