/*
 * extendos.h - interface to ExtenDOS xbios routines for Lattice C 5 & Pure C
 *
 * This file contains the general interface to C5 via #pragmas
 *
 * version 3.0	november/1998
 *		added definitions of structures returned by EDinit()
 *
 * version 4.0	march/2001
 *		protected against redefinition of EXTENDOS_COOKIE
 *
 * Copyright ½ 1993-2001 Roger Burrows
 */
#ifndef _EXTENDOS_H
#define _EXTENDOS_H

#ifndef EXTENDOS_COOKIE
#define EXTENDOS_COOKIE		0x4550726fL		/* 'EPro' */
#endif

#define	ED_MAGIC			0x4643544cL		/* 'FCTL' */
#define	_MET_MAGIC			0x5f4d4554L		/* '_MET' */

struct edinit_return {				/* structure returned by Edinit() */
	unsigned long bos_drvbits;
	char *metados_id;
	long unused;
	struct _met_ext *_met_ptr;
};
struct _met_ext {
	short _met_ver;					/* e.g. 0x0204 for v2.4 */
	long _met_id;					/* _MET_MAGIC */
	char *metados_map;
};

#ifdef LATTICE

void __vep(int,void *);
long __lesp(int,int,void *);
long __les(int,int);
long __lesplu(int,int,void *,long,unsigned int);
long __lesl(int,int,long);
long __leslsp(int,int,long,int,void *);
long __lesllplp(int,int,long,long,void *,long,void *);
long __lessp(int,int,int,void *);
long __lessll(int,int,int,long,long);
long __lessn(int,int,int,long *);
long __lesn(int,int,long *);

#pragma inline __vep((short),)	{register d2,a2; "4e4e";}
#pragma inline d0=__lesp((short),(short),)	{register d2,a2; "4e4e";}
#pragma inline d0=__les((short),(short))	{register d2,a2; "4e4e";}
#pragma inline d0=__lesplu((short),(short),,,(short))	{register d2,a2; "4e4e";}
#pragma inline d0=__lesl((short),(short),)	{register d2,a2; "4e4e";}
#pragma inline d0=__leslsp((short),(short),,(short),)	{register d2,a2; "4e4e";}
#pragma inline d0=__lesllplp((short),(short),,,,,)	{register d2,a2; "4e4e";}
#pragma inline d0=__lessp((short),(short),(short),)	{register d2,a2; "4e4e";}
#pragma inline d0=__lessll((short),(short),(short),,)	{register d2,a2; "4e4e";}
#pragma inline d0=__lessn((short),(short),(short),)	{register d2,a2; "4e4e";}
#pragma inline d0=__lesn((short),(short),)	{register d2,a2; "4e4e";}

#define	EDinit(a)					__vep(48,a)
#define	EDopen(a,b)					__lesp(49,a,b)
#define	EDclose(a)					__les(50,a)
#define	EDread(a,b,c,d)				__lesplu(51,a,b,c,d)
#define	EDseek(a,b)					__lesl(53,a,b)
#define	EDstatus(a)					__les(54,a)
#define	EDcontrol(a,b,c)			__leslsp(55,a,ED_MAGIC,b,c)
#define	EDget_sess(a,b)				__lesl(56,a,b)
#define EDexec_cdb(a,b,c,d,e,f)		__lesllplp(57,a,b,c,d,e,f)
#define	EDstart_aud(a,b,c)			__lessp(59,a,b,c)
#define	EDstop_aud(a)				__les(60,a)
#define	EDset_songtime(a,b,c,d)		__lessll(61,a,b,c,d)
#define	EDget_toc(a,b,c)			__lessn(62,a,b,c)
#define	EDdisc_info(a,b)			__lesn(63,a,b)

#else
#include <tos.h>		/* for xbios() prototype */

#define	EDinit(a)					xbios(48,a)
#define	EDopen(a,b)					xbios(49,a,b)
#define	EDclose(a)					xbios(50,a)
#define	EDread(a,b,c,d)				xbios(51,a,b,c,d)
#define	EDseek(a,b)					xbios(53,a,b)
#define	EDstatus(a)					xbios(54,a)
#define	EDcontrol(a,b,c)			xbios(55,a,ED_MAGIC,b,c)
#define	EDget_sess(a,b)				xbios(56,a,b)
#define EDexec_cdb(a,b,c,d,e,f)		xbios(57,a,b,c,d,e,f)
#define	EDstart_aud(a,b,c)			xbios(59,a,b,c)
#define	EDstop_aud(a)				xbios(60,a)
#define	EDset_songtime(a,b,c,d)		xbios(61,a,b,c,d)
#define	EDget_toc(a,b,c)			xbios(62,a,b,c)
#define	EDdisc_info(a,b)			xbios(63,a,b)

#endif

#endif
