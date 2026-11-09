/*
 *	scsidrv.h: header for scsidrv.c
 *
 *	This provides an interface to SCSIDRV for Lattice C.  Unless otherwise
 *	specified, routines may be called from either user or supervisor mode.
 *	The following functions are available:
 *
 *		long SCSI_Init(char *busnames[NUMBUSES])
 *	 		Initialises the interface, must be called before any other
 *			functions.  Returns a bitmap of the busses available (0L
 *			means no busses available via SCSIDRV).  If the argument
 *			is not NULL, SCSI_Init() also returns the names of the
 *			busses into the strings pointed to by busnames[].
 *
 *		long SCSI_Modeswitch(long required)
 *			Controls how the system SCSIDRV routines are called.  Iff
 *			'required' is TRUE, SCSI_In() and SCSI_Out() will switch to
 *			supervisor mode (via Supexec()) before calling SCSIDRV.
 *
 *		long SCSI_In(tpSCSICmd parms)
 *		long SCSI_Out(tpSCSICmd parms)
 *		long SCSI_InquireSCSI(long what,tBusInfo *Info)
 *		long SCSI_InquireBus(long what,long BusNo,tDevInfo *Dev)
 *		long SCSI_CheckDev(long BusNo,const DLONG *SCSIId,char *name,unsigned short *features)
 *		long SCSI_RescanBus(long BusNo)
 *		long SCSI_Open(long BusNo,const DLONG *SCSIId,unsigned long *MaxLen)
 *		long SCSI_Close(tHandle handle)
 *		long SCSI_Error(tHandle handle,longrwflag,long ErrNo)
 *			These correspond to the basic SCSIDRV functions as described
 *			in the specification document; refer there for further info.
 *
 *		long SCSI_Inquiry(long BusNo,const DLONG *SCSIId,long lun,char *inqdata)
 *			Issues SCSI Inquiry command to device specified by bus, id,
 *			lun.  If the Inquiry is successful, returns 0 and copies 32
 *			bytes of inquiry data into 'inqdata'; otherwise, returns -1.
 *
 *
 *	version 1.0		rfb (march 1999)
 *		original version
 *
 *	version 1.1		rfb (july 2001)
 *		. added new functions SCSI_In_Super()/SCSI_Out_Super()
 *		. added function descriptions
 *
 *	version 1.2		rfb (august 2001)
 *		. updated function descriptions
 *
 *	version 1.3		rfb (february 2002)
 *		. removed functions SCSI_In_Super()/SCSI_Out_Super(), since SCSI_In()
 *		  & SCSI_Out() are now callable from either user or supervisor mode
 *
 *	version 1.5		rfb (may 2002)
 *		. added new function SCSI_Options()
 *
 *	version 1.6		rfb (august 2007)
 *		. add #ifndef wrapper to protect against double inclusion
 *
 *	version 2.0		rfb (november 2007)
 *		. removed SCSI_Options() & related #defines; added SCSI_Modeswitch()
 */
#ifndef SCSIDRV_H
#define SCSIDRV_H

#include <cookie.h>
#include <scsidefs.h>

#define SCSI_COOKIE		0x53435349L		/* 'SCSI' */

#define	NUMBUSES	(MAXBUSNO+1)
#define	BUSNAMELEN	20

/*
 *	function prototypes
 */
ULONG SCSI_Init(char *busnames[BUSNAMELEN]);
LONG SCSI_Modeswitch(LONG required);

LONG SCSI_In(tpSCSICmd parms);
LONG SCSI_Out(tpSCSICmd parms);
LONG SCSI_InquireSCSI(LONG what,tBusInfo *Info);
LONG SCSI_InquireBus(LONG what,LONG BusNo,tDevInfo *Dev);
LONG SCSI_CheckDev(LONG BusNo,const DLONG *SCSIId,char *name,UWORD *features);
LONG SCSI_RescanBus(LONG BusNo);
LONG SCSI_Open(LONG BusNo,const DLONG *SCSIId,ULONG *MaxLen);
LONG SCSI_Close(tHandle handle);
LONG SCSI_Error(tHandle handle,LONG rwflag,LONG ErrNo);

LONG SCSI_Inquiry(LONG BusNo,const DLONG *SCSIId,LONG lun,char *inqdata);

#endif
