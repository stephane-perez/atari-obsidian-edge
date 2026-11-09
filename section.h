/*
 * functions defined in section.c
 *
 *
 *	Copyright (C) 2002-2017 Roger Burrows
 *
 *	This file is distributed under the GPL, version 2 or at your
 *	option any later version.  See DOC\LICENSE.TXT for details.
 */

void close_section(void);
int open_section(char *basename,char *mode,char *sectionname);
void read_section_line(char *pattern,char *fmt,...);
int write_section_line(char *pattern,char *fmt,...);
