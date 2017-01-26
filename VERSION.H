/*
 *	version.h: version-related stuff for Edge
 *
 *
 *	Copyright (C) 2002-2017 Roger Burrows
 *
 *	This file is distributed under the GPL, version 2 or at your
 *	option any later version.  See DOC\LICENSE.TXT for details.
 */

#ifdef BETA
#define PROGRAM_VERSION	"2.57á1"
#define EXPIRY_DATE		( ((2017-1980)<<9)+(12<<5)+31 )		/* 31 Dec 2017 */
#else
#define PROGRAM_VERSION	"2.57"
#endif

#define RSC_VERSION		"2.55"				/* note: dependency on size of object in EDGE.RSC !!! */
