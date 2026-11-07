/*
 *	scsidefs.h: SCSIDRV interface definitions, for the GCC build
 *
 *	The upstream Diamond Edge sources include <scsidefs.h>, "a (slightly)
 *	modified version of Steffen Engel's header" (see lib/SCSIDRV.TXT), but
 *	do not ship it.  This file is a minimal re-creation that declares only
 *	what lib/scsidrv.c and lib/driver.c use, following the published
 *	SCSIDRV interface (cookie 'SCSI', revision 1.01).
 *
 *	The SCSIDRV routines take 16-bit integer parameters on the stack, so
 *	lib/scsidrv.c is compiled with -mshort (the original was compiled with
 *	Lattice C's -w option for the same reason).  The public interface in
 *	lib/scsidrv.h only uses long and pointer types, so the rest of the
 *	program is compiled with 32-bit ints as before.
 *
 *	This file is distributed under the GPL, version 2 or at your
 *	option any later version.  See DOC/LICENSE.TXT for details.
 */
#ifndef SCSIDEFS_H
#define SCSIDEFS_H

#include "portab.h"

#define SCSIRevision	0x0101

#define MAXBUSNO		31			/* highest possible bus number */

/* error codes returned by In() and Out() */
#define NOSCSIERROR		0L
#define SELECTERROR		-1L
#define STATUSERROR		-2L
#define PHASEERROR		-3L
#define BSYERROR		-4L
#define BUSERROR		-5L
#define TRANSERROR		-6L
#define FREEERROR		-7L
#define TIMEOUTERROR	-8L
#define DATATOOLONG		-9L
#define LINKERROR		-10L
#define TIMEOUTARBIT	-11L
#define PENDINGERROR	-12L
#define PARITYERROR		-13L

typedef struct {
	unsigned long hi;
	unsigned long lo;
} DLONG;

typedef struct {
	unsigned long BusIds;
	char resrvd[28];
} tPrivate;

typedef short *tHandle;

typedef struct {
	tHandle Handle;
	char *Cmd;
	unsigned short CmdLen;
	void *Buffer;
	unsigned long TransferLen;
	char *SenseBuffer;
	unsigned long Timeout;			/* in 1/200 sec */
	unsigned short Flags;
} tSCSICmd;
typedef tSCSICmd *tpSCSICmd;

#define Disconnect		0x10		/* tSCSICmd.Flags */

typedef struct {
	tPrivate Private;
	char BusName[20];
	unsigned short BusNo;
	unsigned short Features;
	unsigned long MaxLen;
} tBusInfo;

/* tBusInfo.Features */
#define cArbit			0x01
#define cAllCmds		0x02
#define cTargCtrl		0x04
#define cTarget			0x08
#define cCanDisconnect	0x10
#define cScatterGather	0x20

typedef struct {
	char Private[32];
	DLONG SCSIId;
} tDevInfo;

/* InquireSCSI() / InquireBus() 'what' */
#define cInqFirst		0
#define cInqNext		1

/* Error() 'rwflag' and 'ErrNo' */
#define cErrRead		0
#define cErrWrite		1
#define cErrMediach		0
#define cErrReset		1

/* the 'SCSI' cookie points to this; only the initiator part is declared */
typedef struct {
	unsigned short Version;
	long (*In)(tpSCSICmd Parms);
	long (*Out)(tpSCSICmd Parms);
	long (*InquireSCSI)(short what,tBusInfo *Info);
	long (*InquireBus)(short what,short BusNo,tDevInfo *Dev);
	long (*CheckDev)(short BusNo,const DLONG *SCSIId,char *Name,unsigned short *Features);
	long (*RescanBus)(short BusNo);
	long (*Open)(short BusNo,const DLONG *SCSIId,unsigned long *MaxLen);
	long (*Close)(tHandle handle);
	long (*Error)(tHandle handle,short rwflag,short ErrNo);
} tScsiCall;
typedef tScsiCall *tpScsiCall;

#endif
