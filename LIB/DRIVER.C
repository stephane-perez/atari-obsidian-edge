/*
 *	interface from edge stuff to hardware
 *
 *	Copyright (C) 2002-2017 Roger Burrows
 *
 *	This file is distributed under the GPL, version 2 or at your
 *	option any later version.  See DOC\LICENSE.TXT for details.
 */

/*
 *	v2.5	roger burrows, anodyne software
 *		. rewritten to use the scsidrv interface
 *		. does i/o via a smallish buffer in ST RAM
 *
 *	v2.57	roger burrows (january/2017)
 *		prepare for public release
 */
#include <cookie.h>
#include <ctype.h>
#include <osbind.h>
#include <stdlib.h>
#include <string.h>

#include "scsidrv.h"

#define MAXDEV			32				/* max # hard disks supported ... */
#define HDSECTOR_LENGTH	512				/* manifest constant */

#define BUFSIZE_SECTORS	8				/* size of our buffer in sectors (must be less than 256) */
#define BUFSIZE			(BUFSIZE_SECTORS*HDSECTOR_LENGTH)

#define READ_TIMEOUT	(3 * 200)		/* in ticks */
#define WRITE_TIMEOUT	(3 * 200)
#define SHORT_TIMEOUT	(100)

static long busmap = 0L;				/* bitmap of valid buses */

static char *ourbuf = NULL;
static char sense[32];					/* most recent sense from any i/o */

static struct {
	short busid;
	short lun;							// we probably don't need this ...
	tHandle handle;
	unsigned long maxlen;				// or this ...
} device[MAXDEV];

static int numdev = 0;					/* number of active entries in device[] */

/*
 * function prototypes
 */
static long decode_rc(tHandle handle,long rc);
static tHandle get_handle(short busid,short lun);
static int read_capacity(tHandle handle,short lun,unsigned long *capacity);
static long read6(tHandle handle,short lun,short numsec,long start,char *buf);
static long read10(tHandle handle,short lun,short numsec,long start,char *buf);
static long write6(tHandle handle,short lun,short numsec,long start,char *buf);
static long write10(tHandle handle,short lun,short numsec,long start,char *buf);

/********************************************
*											*
*	E X T E R N A L   I N T E R F A C E		*
*											*
********************************************/ 

/*
 *	initialise interface
 */
int init_driver(void)
{
long rc;

	if ((rc=SCSI_Init(NULL)) == 0L)		/* probably doesn't have SCSIDRV */
		return -1;

	/*
	 *	allocate buffer in ST RAM, in case of virtual memory
	 */
	ourbuf = getcookie(_FRB,NULL)?Mxalloc(BUFSIZE,0):Malloc(BUFSIZE);
	if (!ourbuf)
		return -1;

	busmap = rc;						/* initialise */

	return 0;							/* ourbuf allocated, at least 1 bus exists */
}

/*
 *	close interface
 */
void close_driver(void)
{
int i;

	if (!ourbuf)						/* init_driver() not called, or failed */
		return;

	/*
	 *	close all open devices
	 */
	for (i = 0; i < numdev; i++)
		SCSI_Close(device[i].handle);	/* for now, ignore errors */
	numdev = 0;

	busmap = 0L;						/* be tidy */

	if (ourbuf)
		Mfree(ourbuf);
	ourbuf = NULL;
}

/*
 *	determine if bus exists
 */
int bus_exists(int bus)
{
	if (busmap & (1<<bus))				/* check if bus exists */
		return TRUE;

	return FALSE;
}

/*
 *	determine if device exists
 *		returns	-1	device does not exist
 *				else device class, plus copies text string from INQUIRY data to user
 */
int device_exists(short busid,short lun,char *buf)
{
DLONG scsi_id;
int bus;
char *p, *q;

	if (!ourbuf)						/* init_driver() not called, or failed */
		return -1;

	bus = busid / 8;
	if ((busmap&(1<<bus)) == 0)			/* check if bus exists */
		return -1;

	scsi_id.hi = 0;
	scsi_id.lo = busid & 0x07;
	if (SCSI_Inquiry(bus,&scsi_id,lun,ourbuf) < 0L)
		return -1;

	for (p = ourbuf+8, q = buf; p < ourbuf+32; ) {	/* copy to caller's area */
		if (!isprint(*p))
			*p = ' ';
		*q++ = *p++;
	}
	*q = '\0';							/* null terminate! */

	return ourbuf[0] & 0x1f;			/* device class */
}

/*
 *	get capacity of (presumed) hard disk device
 */
int get_capacity(short busid,short lun,unsigned long *capacity)
{
tHandle handle;
long rc;

	if (!ourbuf)						/* init_driver() not called, or failed */
		return -1;

	handle = get_handle(busid,lun);
	if (!handle)						/* not open, and can't open it */
		return -1;

	/*
	 *	we have a scsidrv handle: attempt to get disk capacity
	 */
	rc = read_capacity(handle,lun,capacity);

	return decode_rc(handle,rc);
}

/*
 *	issue a physical read to a (presumed) hard disk device
 */
int phys_read(short busid,short lun,short numsec,long start,char *buf)
{
long (*read)(tHandle,short,short,long,char *);
tHandle handle;
long rc;
int i, n;

	if (!ourbuf)						/* init_driver() not called, or failed */
		return -1;

	handle = get_handle(busid,lun);
	if (!handle)						/* not open, and can't open it */
		return -1;

	/*
	 *	decide which version of READ to use
	 */
	if (start+numsec > 0x1fffffL)		/* too big for READ(6) */
		read = &read10;
	else read = &read6;

	/*
	 *	we have a scsidrv handle: attempt to read from the disk
	 */
	for (i = 0; i < numsec; i += n) {
		n = min(BUFSIZE_SECTORS,numsec-i);
		if ((rc=read(handle,lun,n,start+i,ourbuf)) != 0)
			break;
		memcpy(buf+i*HDSECTOR_LENGTH,ourbuf,n*HDSECTOR_LENGTH);
	}

	return decode_rc(handle,rc);
}

/*
 *	issue a physical write to a (presumed) hard disk device
 */
int phys_write(short busid,short lun,short numsec,long start,char *buf)
{
long (*write)(tHandle,short,short,long,char *);
tHandle handle;
long rc;
int i, n;

	if (!ourbuf)			/* init_driver() not called, or failed */
		return -1;

	handle = get_handle(busid,lun);
	if (!handle)			/* not open, and can't open it */
		return -1;

	/*
	 *	decide which version of WRITE to use
	 */
	if (start+numsec > 0x1fffffL)		/* too big for WRITE(6) */
		write = &write10;
	else write = &write6;

	/*
	 *	we have a scsidrv handle: attempt to write to the disk
	 */
	for (i = 0; i < numsec; i += n) {
		n = min(BUFSIZE_SECTORS,numsec-i);
		memcpy(ourbuf,buf+i*HDSECTOR_LENGTH,n*HDSECTOR_LENGTH);
		if ((rc=write(handle,lun,n,start+i,ourbuf)) != 0)
			break;
	}

	return decode_rc(handle,rc);
}

void rescan_buses(void)
{
int n;

	for (n = 0; n < NUMBUSES; n++)
		if (busmap&(1<<n))
			SCSI_RescanBus(n);
}

/****************************************
*										*
*	I N T E R N A L   R O U T I N E S	*
*										*
****************************************/ 

static int read_capacity(tHandle handle,short lun,unsigned long *capacity)
{
tSCSICmd cmd;
char rcapCDB[] = { 0x25, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
int rc;

	rcapCDB[1] = lun << 5;			/* set up CDB */

	cmd.Handle = handle;			/* set up SCSI command structure */
	cmd.Cmd = rcapCDB;
	cmd.CmdLen = 10;
	cmd.Buffer = ourbuf;
	cmd.TransferLen = 8;
	cmd.SenseBuffer = sense;
	cmd.Timeout = SHORT_TIMEOUT;
	cmd.Flags = 0;
	rc = SCSI_In(&cmd);
	if (rc == 0)
		*capacity = 1 + *(long *)ourbuf;

	return rc;
}

static long read6(tHandle handle,short lun,short numsec,long start,char *buf)
{
tSCSICmd cmd;
char readCDB[] = { 0x08, 0x00, 0x00, 0x00, 0x00, 0x00 };

	readCDB[1] = (lun << 5) | (start >> 16);	/* set up CDB */
	readCDB[2] = (start >> 8) & 0xff;
	readCDB[3] = start & 0xff;
	readCDB[4] = numsec & 0xff;

	cmd.Handle = handle;			/* set up SCSI command structure */
	cmd.Cmd = readCDB;
	cmd.CmdLen = 6;
	cmd.Buffer = buf;
	cmd.TransferLen = numsec * HDSECTOR_LENGTH;
	cmd.SenseBuffer = sense;
	cmd.Timeout = READ_TIMEOUT;
	cmd.Flags = 0;
	return SCSI_In(&cmd);
}

static long read10(tHandle handle,short lun,short numsec,long start,char *buf)
{
tSCSICmd cmd;
char readCDB[] = { 0x28, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

	readCDB[1] = lun << 5;			/* set up CDB */
	readCDB[2] = (start >> 24) & 0xff;
	readCDB[3] = (start >> 16) & 0xff;
	readCDB[4] = (start >> 8) & 0xff;
	readCDB[5] = start;
	readCDB[7] = numsec >> 8;
	readCDB[8] = numsec;

	cmd.Handle = handle;			/* set up SCSI command structure */
	cmd.Cmd = readCDB;
	cmd.CmdLen = 10;
	cmd.Buffer = buf;
	cmd.TransferLen = numsec * HDSECTOR_LENGTH;
	cmd.SenseBuffer = sense;
	cmd.Timeout = READ_TIMEOUT;
	cmd.Flags = 0;
	return SCSI_In(&cmd);
}

static long write6(tHandle handle,short lun,short numsec,long start,char *buf)
{
tSCSICmd cmd;
char writeCDB[] = { 0x0a, 0x00, 0x00, 0x00, 0x00, 0x00 };

	writeCDB[1] = (lun << 5) | (start >> 16);	/* set up CDB */
	writeCDB[2] = (start >> 8) & 0xff;
	writeCDB[3] = start & 0xff;
	writeCDB[4] = numsec & 0xff;

	cmd.Handle = handle;			/* set up SCSI command structure */
	cmd.Cmd = writeCDB;
	cmd.CmdLen = 6;
	cmd.Buffer = buf;
	cmd.TransferLen = numsec * HDSECTOR_LENGTH;
	cmd.SenseBuffer = sense;
	cmd.Timeout = WRITE_TIMEOUT;
	cmd.Flags = 0;
	return SCSI_Out(&cmd);
}

static long write10(tHandle handle,short lun,short numsec,long start,char *buf)
{
tSCSICmd cmd;
char writeCDB[] = { 0x2a, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

	writeCDB[1] = lun << 5;			/* set up CDB */
	writeCDB[2] = (start >> 24) & 0xff;
	writeCDB[3] = (start >> 16) & 0xff;
	writeCDB[4] = (start >> 8) & 0xff;
	writeCDB[5] = start;
	writeCDB[7] = numsec >> 8;
	writeCDB[8] = numsec;

	cmd.Handle = handle;			/* set up SCSI command structure */
	cmd.Cmd = writeCDB;
	cmd.CmdLen = 10;
	cmd.Buffer = buf;
	cmd.TransferLen = numsec * HDSECTOR_LENGTH;
	cmd.SenseBuffer = sense;
	cmd.Timeout = WRITE_TIMEOUT;
	cmd.Flags = 0;
	return SCSI_Out(&cmd);
}

static tHandle get_handle(short busid,short lun)
{
DLONG scsi_id;
tHandle handle;
unsigned long maxlen;
long rc;
int i;

	/*
	 * see if it's already open ...
	 */
	for (i = 0; i < numdev; i++)
		if ((busid == device[i].busid)
		 && (lun == device[i].lun))
			return device[i].handle;

	/*
	 *	not found: attempt to open
	 */
	scsi_id.hi = 0L;
	scsi_id.lo = busid & 0x07;
	rc = SCSI_Open(busid/8,&scsi_id,&maxlen);	/* open ... */
	if (rc < 0L)
		return NULL;							/* failed */
	handle = (tHandle) rc;
	device[numdev].busid = busid;				/* else update table */
	device[numdev].lun = lun;
	device[numdev].handle = handle;
	device[numdev].maxlen = maxlen;
	numdev++;

	return handle;
}

static long decode_rc(tHandle handle,long rc)
{
	if (rc == PENDINGERROR) {
		SCSI_Error(handle,cErrRead,0);				/* clear it */
		rc = -1L;
	} else if (rc == 2) {						/* check condition */
		SCSI_Error(handle,cErrWrite,cErrMediach);	/* broadcast the error */
		rc = -1L;
	}

	return rc;
}
