/*
 *	misc.h:	header for misc.lib
 *
 *	function prototypes
 *	-------------------
 *	long compare_memory(const void *s,const void *t,long n);
 *		fast compare of memory areas pointed to by s & t, for a length of
 *		n bytes; for best performance, both areas should be word-aligned
 *
 *
 *	notes
 *	-----
 *	eventually we will move all *minor* local functions into misc.lib,
 *	probably including those in crc.c, clock.c.  at that time, we'll
 *	do a proper library, with multiple .o files
 */
#ifndef _MISC_H
#define _MISC_H

long compare_memory(const void *s,const void *t,long n);

#endif
