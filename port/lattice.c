/*
 *	lattice.c: Lattice C library extensions, for the GCC build
 *
 *	Part of the GCC port layer of Obsidian Edge: implements the few
 *	functions of the Lattice C 5 library used by the Diamond Edge sources
 *	that MiNTLib/GEMlib do not provide (see port/include/).
 *
 *	This file is distributed under the GPL, version 2 or at your
 *	option any later version.  See DOC/LICENSE.TXT for details.
 */
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "include/oe_gem.h"
#include <mint/cookie.h>
#include <mint/sysvars.h>
#include "include/lattice.h"
#include "include/dos.h"
#include "include/cookie.h"

int _OSERR;

/*
 *	<cookie.h>
 */
int getcookie(long cookie,long *value)
{
	long v;

	if (Getcookie(cookie,&v) != C_FOUND)
		return 0;
	if (value)
		*value = v;
	return 1;
}

/*
 *	TOS version, from the OS header pointed to by _sysbase
 */
static long read_tos_version(void)
{
	OSHEADER *os = *(OSHEADER **)0x4f2L;	/* _sysbase */

	return os->os_beg->os_version;
}

unsigned short oe_tos_version(void)
{
	static unsigned short version;

	if (!version)
		version = (unsigned short)Supexec(read_tos_version);
	return version;
}

/*
 *	<stdio.h> extensions
 */
int fgetw(FILE *fp)
{
	int hi, lo;

	if ((hi = getc(fp)) == EOF || (lo = getc(fp)) == EOF)
		return EOF;
	return (hi << 8) | lo;
}

int fputw(int w,FILE *fp)
{
	if (putc((w >> 8) & 0xff,fp) == EOF || putc(w & 0xff,fp) == EOF)
		return EOF;
	return w;
}

long fgetl(FILE *fp)
{
	int hi, lo;

	if ((hi = fgetw(fp)) == EOF || (lo = fgetw(fp)) == EOF)
		return EOF;
	return ((long)hi << 16) | lo;
}

long fputl(long l,FILE *fp)
{
	if (fputw((int)(l >> 16),fp) == EOF || fputw((int)l,fp) == EOF)
		return EOF;
	return l;
}

/*
 *	Lattice C srand48()/mrand48(): see lattice.h
 */
static unsigned long long rand48_state;

void oe_srand48(long seed)
{
	rand48_state = ((unsigned long long)(unsigned long)seed << 16) | 0x330eULL;
}

long oe_mrand48(void)
{
	rand48_state = (rand48_state * 0x5deece66dULL + 0xbULL) & 0xffffffffffffULL;
	return (long)(unsigned long)rand48_state;
}

/*
 *	swab(), safe when from == to
 */
void oe_swab(const void *from,void *to,ssize_t n)
{
	const unsigned char *f = from;
	unsigned char *t = to, c;

	for ( ; n > 1; n -= 2, f += 2, t += 2) {
		c = f[0];
		t[0] = f[1];
		t[1] = c;
	}
}

/*
 *	<dos.h>: file name manipulation
 */

/* stcgfn: get the file name (without the path) of 'path' */
void stcgfn(char *name,const char *path)
{
	const char *p, *start = path;

	for (p = path; *p; p++)
		if (*p == '\\' || *p == '/' || *p == ':')
			start = p + 1;
	strcpy(name,start);
}

/* strmfp: make a full path from a directory and a file name */
void strmfp(char *path,const char *dir,const char *name)
{
	size_t n;

	if (!dir || !*dir) {
		strcpy(path,name);
		return;
	}
	strcpy(path,dir);
	n = strlen(path);
	if (path[n-1] != '\\' && path[n-1] != '/' && path[n-1] != ':')
		path[n++] = '\\';
	strcpy(path+n,name);
}

/* strmfe: make a file name with a new extension */
void strmfe(char *newname,const char *oldname,const char *ext)
{
	char *p, *dot = NULL;

	strcpy(newname,oldname);
	for (p = newname; *p; p++) {
		if (*p == '.')
			dot = p;
		else if (*p == '\\' || *p == '/' || *p == ':')
			dot = NULL;
	}
	if (!dot)
		dot = p;
	*dot++ = '.';
	strcpy(dot,ext);
}

/*
 *	<dos.h>: current drive
 */
int getdsk(void)
{
	return Dgetdrv();
}

long chgdsk(int drive)
{
	return Dsetdrv(drive);
}

/*
 *	<io.h>: length of an open file (the file position is preserved)
 */
long filelength(int fd)
{
	long pos, len;

	if ((pos = lseek(fd,0L,SEEK_CUR)) < 0)
		return -1L;
	len = lseek(fd,0L,SEEK_END);
	lseek(fd,pos,SEEK_SET);
	return len;
}

/*
 *	<dos.h>: directory search
 */
int dfind(struct FILEINFO *info,const char *name,int attr)
{
	_DTA *old = Fgetdta();
	long rc;

	Fsetdta((_DTA *)info);
	rc = Fsfirst(name,attr);
	Fsetdta(old);
	return rc ? (_OSERR = (int)-rc, -1) : 0;
}

int dnext(struct FILEINFO *info)
{
	_DTA *old = Fgetdta();
	long rc;

	Fsetdta((_DTA *)info);
	rc = Fsnext();
	Fsetdta(old);
	return rc ? (_OSERR = (int)-rc, -1) : 0;
}

/*
 *	<dos.h>: file date/time, packed in a long.  The sources only save the
 *	value of getft() to restore it later with chgft(), so the packing
 *	((date << 16) | time) only has to be the same in both functions.
 */
long getft(int fd)
{
	_DOSTIME dt;

	if (Fdatime(&dt,fd,0) < 0)
		return -1L;
	return ((long)dt.date << 16) | dt.time;
}

int chgft(int fd,long ft)
{
	_DOSTIME dt;

	dt.time = (unsigned short)ft;
	dt.date = (unsigned short)(ft >> 16);
	return Fdatime(&dt,fd,1) < 0 ? -1 : 0;
}

/*
 *	<aes.h> utility functions
 */
void objc_xywh(OBJECT *tree,short obj,GRECT *rect)
{
	objc_offset(tree,obj,&rect->g_x,&rect->g_y);
	rect->g_w = tree[obj].ob_width;
	rect->g_h = tree[obj].ob_height;
}

int rc_inside(short x,short y,const GRECT *rect)
{
	return (x >= rect->g_x) && (y >= rect->g_y)
		&& (x < rect->g_x + rect->g_w) && (y < rect->g_y + rect->g_h);
}
