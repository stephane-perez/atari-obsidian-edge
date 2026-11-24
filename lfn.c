/*
 *	lfn.c: VFAT long file names, for Obsidian Edge and Diamond Mirror
 *
 *	Copyright (C) 2026 the Obsidian Edge contributors
 *
 *	This file is distributed under the GPL, version 2 or at your
 *	option any later version.  See DOC\LICENSE.TXT for details.
 */

#include <string.h>
#include <portab.h>
#include <dos.h>
#include "edgedefs.h"

int lfn_entries(char *p,char *end);
int dir_unit_len(char *p,char *end);
void move_unit(char *inspoint,char *curpoint,int len);
char *sort_units(char *inspoint,char *endpoint,int rmdel);

/*
 *	VFAT long file names
 *
 *	lfn_entries(): if p is the first entry of a valid long name (entries
 *	with the attribute FA_LFN, numbered n..1, followed by the 8.3 entry of
 *	a file whose name matches their checksum), returns n; otherwise 0.
 *	end is the end of the directory buffer.
 */
static unsigned char lfn_checksum(unsigned char *name)
{
unsigned char sum = 0;
int i;

	for (i = 0; i < NAME_LENGTH; i++)
		sum = (unsigned char)(((sum & 1) << 7) + (sum >> 1) + name[i]);

	return sum;
}

int lfn_entries(char *p,char *end)
{
unsigned char *e = (unsigned char *)p;
int n, i;

	if ((*e == DELETION_MARK) || (*e == 0x00) || !is_lfn(e) || !(*e & 0x40))
		return 0;
	n = *e & 0x3f;
	if ((n < 1) || (n > MAX_LFN_ENTRIES) || (p + (n+1)*sizeof(DIRENTRY) > end))
		return 0;

	for (i = 0; i < n; i++, e += sizeof(DIRENTRY)) {
		if (!is_lfn(e) || ((*e & 0x3f) != n-i) || (e[13] != p[13]))	/* order, checksum */
			return 0;
		if (i && (*e & 0x40))
			return 0;
	}

	/* e is the 8.3 entry */
	if ((*e == DELETION_MARK) || (*e == 0x00) || is_lfn(e) || (fattr(e) & FA_VOLUME))
		return 0;
	if (lfn_checksum(e) != (unsigned char)p[13])
		return 0;

	return n;
}

/*
 *	length in bytes of the directory 'unit' starting at p: a long name
 *	with its 8.3 entry, or a single entry
 */
int dir_unit_len(char *p,char *end)
{
	return (lfn_entries(p,end) + 1) * sizeof(DIRENTRY);
}

/*
 *	VFAT long file names: a long name (entries with the attribute FA_LFN)
 *	and the 8.3 entry that follows it are moved together, as one 'unit'
 *	(see dir_unit_len() in misc.c); every other entry is a unit by itself.
 *	unit_attr() is the attribute of the unit's 8.3 entry.
 */
#define UNIT_BUFSIZE	((MAX_LFN_ENTRIES+1)*sizeof(DIRENTRY))

/*
 *	move the unit at curpoint (len bytes) to inspoint (inspoint < curpoint),
 *	shifting the entries in between
 */
void move_unit(char *inspoint,char *curpoint,int len)
{
char temp[UNIT_BUFSIZE];

	memcpy(temp,curpoint,len);
	memmove(inspoint+len,inspoint,curpoint-inspoint);
	memcpy(inspoint,temp,len);
}

/*
 *	the part of sort_dir() and sort_root() after the insertion point:
 *	moves the subdirectory units to inspoint, keeping the order of the
 *	other units, and (optionally) removes deleted entries
 *
 *	returns the new endpoint
 */
char *sort_units(char *inspoint,char *endpoint,int rmdel)
{
char *curpoint;
int len;

	/*
	 *	move insertion point to first NON-subdirectory unit
	 */
	while (*inspoint && (inspoint < endpoint)) {
		len = dir_unit_len(inspoint,endpoint);
		if (!(unit_attr(inspoint,len)&FA_SUBDIR))
			break;
		inspoint += len;
	}

	/*
	 *	scan through remainder of directory:
	 *	. if not a deleted entry:
	 *		. if a subdirectory unit, move directory entries up & insert it
	 *		. else just skip it
	 *	. else it's a deleted entry:
	 *		. if removing deleted entries, move directory entries down 1
	 *		. else just skip it
	 */
	if (inspoint >= endpoint)
		return endpoint;
	for (curpoint = inspoint + dir_unit_len(inspoint,endpoint); curpoint < endpoint; ) {
		if (*curpoint != DELETION_MARK) {
			len = dir_unit_len(curpoint,endpoint);
			if (*curpoint && (unit_attr(curpoint,len)&FA_SUBDIR)) {
				move_unit(inspoint,curpoint,len);
				inspoint += len;
			}
			curpoint += len;
		} else {
			if (rmdel) {
				memmove(curpoint,curpoint+sizeof(DIRENTRY),endpoint-curpoint-sizeof(DIRENTRY));
				endpoint -= sizeof(DIRENTRY);
				memset(endpoint,0x00,sizeof(DIRENTRY));
			} else {
				curpoint += sizeof(DIRENTRY);
			}
		}
	}

	return endpoint;
}
