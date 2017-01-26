/****************************************************************************/
/*                                                                          */
/* Module	: XRSRC.H                                                       */
/* Date		: 18.02.92                                                      */
/*                                                                          */
/* version 1.20 by Roger Burrows (December 2002)							*/
/*	. add required includes to allow standalone clean compile				*/
/*	. remove CONST (not defined in standard portability header)				*/
/*	. remove usage of compatibility macro for prototyping					*/
/*																			*/
/****************************************************************************/

#ifndef __XRSRC__
#define __XRSRC__
#include <aes.h>
#include <portab.h>

/****** DEFINES **************************************************************/

/****** TYPES ****************************************************************/

typedef struct
{
	UWORD rsh_vrsn;				/* should be 3 */
	UWORD rsh_extvrsn;			/* not used, initialised to 'IN' for Interface */
	ULONG rsh_object;
	ULONG rsh_tedinfo;
	ULONG rsh_iconblk;			/* list of ICONBLKS */
	ULONG rsh_bitblk;
	ULONG rsh_frstr;
	ULONG rsh_string;
	ULONG rsh_imdata;			/* image data */
	ULONG rsh_frimg;
	ULONG rsh_trindex;
	ULONG rsh_nobs; 			/* counts of various structs */
	ULONG rsh_ntree;
	ULONG rsh_nted;
	ULONG rsh_nib;
	ULONG rsh_nbb;
	ULONG rsh_nstring;
	ULONG rsh_nimages;
	ULONG rsh_rssize;			/* total bytes in resource */
} RSXHDR;

/****** VARIABLES ************************************************************/

/****** FUNCTIONS ************************************************************/

WORD xrsrc_load(BYTE *re_lpfname,WORD *pglobal);
WORD xrsrc_free(WORD *pglobal);
WORD xrsrc_gaddr(WORD re_gtype,WORD re_gindex,VOID *re_gaddr,WORD *pglobal);
WORD xrsrc_saddr(WORD re_stype,WORD re_sindex,VOID *re_saddr,WORD *pglobal);
WORD xrsrc_obfix(OBJECT *re_otree,WORD re_oobject);

BOOLEAN init_xrsrc(WORD vdi_handle,GRECT *desk,WORD gl_wbox,WORD gl_hbox);
VOID term_xrsrc(VOID);

#endif	/* __XRSRC__ */
