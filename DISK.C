/*
 *	disk.c
 *
 *	performs the following major functions:
 *		do_copyimage:	copy partition image
 *		do_chart:		display 'all info' chart
 *	plus a large number of widely-used utility functions
 *	(these should probably be in misc.c)
 *
 *
 *	version 2.10 (december/2002)		roger burrows
 *		a highly cleaned up and reorganised version of
 *		the DE v2.04 source; no new function
 */
#include <stdio.h>
#include <stdlib.h>
#include <aes.h>
#include <dos.h>
#include <limits.h>
#include <oserr.h>
#include <portab.h>
#include <string.h>

#include "edge.h"
#include "edgedefs.h"
#include "miscdef.h"
#include "proto.h"

#define MODULE_NUMBER	2		/* used by error() macro */

extern long drivemap;
extern short dev, win1;
extern short doing_opt;
extern OPT opt;
extern short fullx, fully, fullh, fullw;
extern short workx, worky, workw, workh, windhidden;
extern MYBPB dpb;
extern STATUS status;
extern OBJECT *chart, *pcsel, *text;


/*
 *	private variables
 */
MLOCAL struct {			/* remember which drives have been scanned */
	short scanned;
	long total;
	long free;
} devstats[NUM_DEVICES] =
	{ { TRUE, 0L, 0L }, { TRUE, 0L, 0L } };	/* say A & B have been scanned (is this necessary?) */

MLOCAL char alert[MAX_ALERT_SIZE];

/*
 *	function prototypes
 */
PRIVATE long bootsector_read(short dev,void *buf,int pgmloc);
PRIVATE int copy_image(int dev1,int dev2);
PRIVATE void disastrous_error(int type,int errmsgno,int start,int end,int pgmloc);
PRIVATE int dos_retry(int errnum,int type,char *name,int pgmloc);
PRIVATE void load_cstr(char *str,long num,int k);
PRIVATE int sector_retry(long errnum,int type,int errmsgno,int sector,int pgmloc);
PRIVATE void trim_name(char *to,char *from,int length);


PUBLIC unsigned short logsector(short cluster)
{
	return ((cluster - 2) * dpb.spc + dpb.firstd);
}

/*
 *	convert in-memory 16-bit big-endian FAT to 12-bit little-endian equivalent
 *
 *	NO validity checking is done!
 */
PUBLIC void fat16_to_fat12(unsigned short *f16,unsigned char *f12)
{
int j;

	/*
	 *	in loop n, we convert clusters 2n and 2n+1 ...
	 */
	for (j = 0; j < (dpb.maxc+1)/2; j++, f12 += 3, f16 += 2) {
		*f12 = *f16 & 0xff;
		*(f12+1) = ((*f16>>8) & 0x0f) | ((*(f16+1)<<4) & 0xf0);
		*(f12+2) = (*(f16+1)>>4) & 0xff;
	}
}

/*
 *	convert in-memory little-endian 12-bit FAT to 16-bit big-endian equivalent
 */
PUBLIC void fat12_to_fat16(unsigned char *f12,unsigned short *f16)
{
int j;

	/*
	 *	in loop n, we convert clusters 2n and 2n+1 ...
	 */
	for (j = 0; j < (dpb.maxc+1)/2; j++, f12 += 3, f16 += 2) {
		*f16 = (*f12 | (*(f12+1)<<8)) & 0x0fff;
		*(f16+1) = ((*(f12+1)>>4) | (*(f12+2)<<4)) & 0x0fff;
	}
}

/*
 *	populate MYBPB from device
 *
 *	returns:	0		ok
 *				-1		most errors
 *				-EDRVNR	device not ready
 */
PUBLIC int get_mybpb(MYBPB *mybpb,short device)
{
int ret;
long n;
BPB *tbpb;
char *bbuf;

	_mediach(device);
	n = (device>=FIRST_HD_DEVICE) ? MAX_HD_LOGSECSIZE : MAX_FL_LOGSECSIZE;
	if ((bbuf=malloc(n)) == NULL) {
		error(n,MEMALLOC,"");
		return -1;
	}

	if ((ret=bootsector_read(device,bbuf,ERROR_LINENUM)) < 0) {
//was	if (log_read(device,1,0,bbuf,BOOT)) {
		free(bbuf);
		return ret;
	}

	if ((tbpb=Getbpb(device)) == NULL)
		ret = -1;
	else ret = load_mybpb(bbuf,mybpb,tbpb);

	free(bbuf);

	return ret;
}

/*
 *	boot sector read with optional retry
 *
 *	note that this is essentially the same as logical_read(),
 *	except that we handle 'not ready' differently
 */
PRIVATE long bootsector_read(short dev,void *buf,int pgmloc)
{
long rc;

	while(1) {
		rc = Rwabs(2,buf,1,0,dev);
		if ((rc == 0L) || (rc == -EDRVNR))
			break;
		if (!sector_retry(rc,READ,BOOT,0,pgmloc))
			return -1L;
	}

	return rc;
}

/*
 *	load MYPBP structure from boot sector info
 *
 *	returns:	0	ok
 *				-1	error
 */
PUBLIC int load_mybpb(char *buff,MYBPB *mybpb,BPB *tbpb)
{
int n;
long maxclus;

	if (tbpb)
		mybpb->isfat16 = tbpb->bflags & 0x01;

	if (memcmp(buff+DATALITE_ID_OFFSET,DATALITE_ID,4) == 0)
		mybpb->datalite = 1;
	else mybpb->datalite = 0;

	mybpb->tsec = get_word(buff + 19);		/* total sectors */
	mybpb->bps = get_word(buff + 11);		/* bytes per sector */
	mybpb->spc = buff[13];					/* sec per cluster */
	if (tbpb) {
		if (mybpb->bps != tbpb->recsiz)		/* when we first access a partition, we */
			return -1;						/* seem to get an empty boot sector ... */
		if (mybpb->spc != tbpb->clsiz)
			return -1;
	}

	mybpb->spt = get_word(buff + 24);		/* sec per track */
	if ((mybpb->spt > 999) || (mybpb->spt < 0))
		mybpb->spt = 0;						/* silently fix cosmetic error */

	mybpb->tpc = get_word(buff + 26);		/* track per cylinder */
	if ((mybpb->tpc > 999) || (mybpb->tpc < 0))
		mybpb->tpc = 0;						/* silently fix cosmetic error */

	mybpb->media = buff[21];
	mybpb->spf = get_word(buff + 22);		/* sec per fat */
	mybpb->nfat = buff[16];					/* number of fat */
	mybpb->ndir = get_word(buff + 17);		/* max dir root entries */

	mybpb->rds = (short)((mybpb->ndir*sizeof(DIRENTRY)+mybpb->bps-1) / mybpb->bps);
											/* rootdir in sectors */
	mybpb->nhide = get_word(buff + 28);		/* hidden sectors */
	if ((mybpb->nhide > 999) || (mybpb->nhide < 0))
		mybpb->nhide = 0;					/* silently fix */

	mybpb->nres = get_word(buff + 14);		/* number reserved */
	mybpb->firstd = (mybpb->nfat*mybpb->spf) + mybpb->rds + mybpb->nres;

	if (doing_opt) {
		if (tbpb)
			mybpb->cdata = tbpb->numcl;		/* test for DL2 */
	} else {
		mybpb->cdata = (short) ((mybpb->tsec-mybpb->firstd) / mybpb->spc);
		if (mybpb->datalite)
			mybpb->tsec = *(unsigned short *)(buff+DATALITE_SEC_OFFSET);
	}

	mybpb->bpf = (long)mybpb->spf * mybpb->bps;
	mybpb->rdb = (long)mybpb->rds * mybpb->bps;	/* root dir bytes */
	mybpb->bpc = (long)mybpb->spc * mybpb->bps;
	mybpb->tbyte = (long)mybpb->cdata * mybpb->bpc;

	for (n = mybpb->bps, mybpb->sector_shift = 0; n > 1; n >>= 1)
		mybpb->sector_shift++;
	for (n = mybpb->bpc, mybpb->cluster_shift = 0; n > 1; n >>= 1)
		mybpb->cluster_shift++;

	if (mybpb->isfat16) {
		mybpb->bitmask = ENDCHAIN16;
		mybpb->badmask = BADCLUST16;
		maxclus = (mybpb->bpf / 2L) - 2;
	} else {
		mybpb->bitmask = ENDCHAIN12;
		mybpb->badmask = BADCLUST12;
		maxclus = ((mybpb->bpf * 2L) / 3L) - 2;
	}

	if (maxclus < mybpb->cdata)				/* shouldn't happen */
		mybpb->cdata = maxclus;

	mybpb->maxc = mybpb->cdata + 2;			/* max cluster # + 1 */

	/*
	 *	perform final validation
	 */
	for (n = MIN_LOGSECSIZE; n <= MAX_HD_LOGSECSIZE; n <<= 1)
		if (mybpb->bps == n)
			break;
	if (n > MAX_HD_LOGSECSIZE)
		return -1;

	if ((mybpb->spc > 16) || (mybpb->spc < 0))				//MAGNUM
		return -1;

	if (mybpb->nfat != 2)
		return -1;

	if ((mybpb->cdata < 0) || (mybpb->cdata > 32767))		//MAGNUM
		return -1;

	return 0;
}

/*************************************************************************/
PUBLIC void loadboot(char *buff,MYBPB *mybpb)
{
	putword((IWORD *)(buff+11),mybpb->bps);		/* bytes per sector */
	buff[13] = mybpb->spc;						/* sec per clus */
	putword((IWORD *)(buff+14),mybpb->nres);	/* number reserved */
	buff[16] = mybpb->nfat;						/* number of fat */
	putword((IWORD *)(buff+17),mybpb->ndir);	/* max dir root entries */
	putword((IWORD *)(buff+19),(unsigned short)mybpb->tsec); 	/* tot sec */
	buff[21] = mybpb->media;					/* media descriptor */
	putword((IWORD *)(buff+22),mybpb->spf); 	/* sec per fat */
	putword((IWORD *)(buff+24),mybpb->spt); 	/* sec per track */
	putword((IWORD *)(buff+26),mybpb->tpc); 	/* track per cylinder */
	putword((IWORD *)(buff+28),mybpb->nhide);	/* hidden sectors */
}

/*
 * extract name from directory entry
 */
PUBLIC int get_name(DIRENTRY *dir,char *fname)
{
int j;
char *p;

	if (!dir->name[0]) {
		*fname = '\0';
		return 0;
	}

	for (j = 0, p = dir->name; (*p != ' ') && (j < NODE_LENGTH); j++)
		*fname++ = *p++;

	if (dir->ext[0] != ' ') {		/* if there is an extension */
		*fname++ = '.';
		for (j = 0, p = dir->ext; (*p != ' ') && (j < EXT_LENGTH); j++)
			*fname++ = *p++;
	}

	*fname = '\0';

	return 1;
}

PUBLIC void permanent_error(long errnum,int type,char *msg,int pgmloc)
{
char fname[MAX_ALERTLINE_SIZE+1];

	switch(type) {
	case READ:
	case WRITE:
	case OPEN:
		trim_name(fname,msg,MAX_ALERTLINE_SIZE);		/* trim long filenames to fit */
		xform_alert(1,ER_DOSERR_ALERT,text[ERROR_TYPE+type].ob_spec,fname,pgmloc,errnum);
		break;
	case DISKFULL:
		xform_alert(1,ER_DISKFUL_ALERT,pgmloc);
		break;
	case MEMALLOC:
		xform_alert(1,ER_NOMEM_ALERT,errnum,pgmloc);
		break;
	}
}

/*
 *	trim name to 'length' characters (trailing null not counted)
 *
 *	if length <= 6 (not expected), just copy first 'length' characters
 *	else if name < 'length' characters, just copy name
 *	else copy first (length/2) characters, add 3 dots, fill with tail of input
 *
 *	examples:
 *		input string			length			output string
 *		------------			------			-------------
 *		c:\abcdefghijklmnop		6				c:\abc
 *		d:\abcdefghijklmnop		20				d:\abcdefghijklmnop
 *		e:\abcdefghijklmnop		10				e:\ab...op
 *		f:\abcdefghijklmnop     15              f:\abcd...lmnop
 */
PRIVATE void trim_name(char *to,char *from,int length)
{
int i, n;
char *p, *q;

	if (length <= 6) {			/* handle degenerate case */
		strncpy(to,from,length);
		to[length] = '\0';
		return;
	}

	n = strlen(from);
	if (n <= length) {			/* handle easy case */
		strcpy(to,from);
		return;
	}

	for (i = 0, p = to, q = from; i < length/2; i++)
		*p++ = *q++;
	for (i = 0; i < 3; i++, q++)
		*p++ = '.';
	for (q += n-length+3; *q; )
		*p++ = *q++;
	*p = '\0';
}

/*
 *	perform straight image copy (no defrag)
 */
PUBLIC void do_copyimage(void)
{
int dev1, dev2, savedev;
int rc;
char temp[MAX_DISPLAYSIZE];

	wind_update(BEG_UPDATE);
	savedev = dev;

	if ((rc=get_src_dest()) < 0) {
		wind_update(END_UPDATE);
		return;
	}
	dev1 = rc >> 8;
	dev2 = rc & 0xff;

	mouse_off();

	start_bar(text[PROG_PARTCOPY].ob_spec);
	delay(PARTCOPY_START_DELAY);
	sprintf(temp,text[PROG_SRCDEST].ob_spec,'A'+dev1,'A'+dev2);
	update_barmsg(temp);

	rc = copy_image(dev1,dev2);		/* perform copy */

	mouse_on();
	wind_update(END_UPDATE);

	if (rc < 0)
		xform_alert(1,GEN_OPFAIL_ALERT);

	wrap_bar();
	dev = savedev;
	if (get_mybpb(&dpb,dev) < 0)
		xform_alert(1,GEN_BPB_ALERT);

	_mediach(dev2);
	force_update(dev2);
}

/*
 *	the actual copy image routine
 */
PRIVATE int copy_image(int dev1,int dev2)
{
int cmpbps, cmpcdata, nout, ndo;
int nclus, ls, i;
long rc;
unsigned short *f16;
short nextcluster;
char *readbuf = NULL;
char *rootbuf;

	dev = dev1;
	if (get_mybpb(&dpb,dev) < 0) {
		xform_alert(1,GEN_BPB_ALERT);
		return 0;
	}
	cmpbps = dpb.bps;
	cmpcdata = dpb.cdata;
	dev = dev2;
	if (get_mybpb(&dpb,dev) < 0) {
		xform_alert(1,GEN_BPB_ALERT);
		return 0;
	}
	if ((cmpbps != dpb.bps) || (cmpcdata != dpb.cdata)) {
		xform_alert(1,CP_COMPAT_ALERT);
		return 0;
	}

	if ((f16=malloc(dpb.nfat*dpb.bpf)) == NULL) {
		error(dpb.nfat*dpb.bpf,MEMALLOC,"");
		return -1;
	}
	rc = log_read(dev1,dpb.nfat*dpb.spf,1,f16,FAT);
	if (rc == 0) {
		rc = log_write(dev2,dpb.nfat*dpb.spf,1,f16,FAT);
		if (dpb.isfat16) {						/* fat16 */
			for (i = 2; i < dpb.maxc; i++) {
				if (f16[i]) {
					if (f16[i] != BADCLUST16_INTEL)	/* because we haven't swabbed */
						nextcluster = i;
				}
			}
		} else nextcluster = dpb.maxc - 1;		/* fat12 */
	}
	free(f16);
	if (rc)
		return -1;

	if ((rootbuf=malloc(dpb.rdb)) == NULL) {
		error(dpb.rdb,MEMALLOC,"");
		return -1;
	}
	rc = log_read(dev1,dpb.rds,dpb.nfat*dpb.spf+1,rootbuf,DIR);
	if (rc == 0)
		rc = log_write(dev2,dpb.rds,dpb.nfat*dpb.spf+1,rootbuf,DIR);
	free(rootbuf);

	nclus = max_allocatable_clusters(dpb.bpc);
	if (nclus)
		readbuf = malloc(nclus*dpb.bpc);
	if (!readbuf) {
		error(nclus*dpb.bpc,MEMALLOC,"");
		return -1;
	}

	nout = 2;
	init_bar(nextcluster);
	while (nout < nextcluster) {
		ndo = min(nextcluster-nout+1,nclus);
//was		ndo = ((nextcluster - nout + 1) < nclus) ? (nextcluster - nout + 1) : nclus;
		ls = logsector(nout);
		if (log_read(dev1,ndo*dpb.spc,ls,readbuf,HARDDISK)) {
			free(readbuf);
			return -1;
		}
		update_bar(nout + (ndo / 2));

		if (log_write(dev2,ndo*dpb.spc,ls,readbuf,HARDDISK)) {
			free(readbuf);
			return -1;
		}
		update_bar(nout + ndo);
		nout += ndo;
	}

	ring_bell();
	ring_bell();

	free(readbuf);
	return 0;
}

/************************************************************************/
PUBLIC void mark_chart(int device)
{
	if ((device >= FIRST_HD_DEVICE) && (device < NUM_DEVICES))
		devstats[device].scanned = FALSE;	/* mark device to be rescanned */
}

PUBLIC void do_chart(int device)
{
short savedev, i, j, free_clus;
int rc;
long bfree, tbyte, thousandths;
long ttot = 0L;
long tfree = 0L;
unsigned short *f16;
char *f12;

	mark_chart(device);				/* mark device to be rescanned */
	change_mode(ALLINFO);

	savedev = dev;

	for (dev = FIRST_HD_DEVICE, i = 0; dev < NUM_DEVICES; dev++, i++) {
		chart[CHC+i].ob_state &= ~SELECTED;
		if (!((1L<<dev) & drivemap)) {
			chart[CHC+i].ob_state |= DISABLED;
			chart[CBAR+i*2].ob_flags |= HIDETREE;
		}
	}
	for (dev = FIRST_HD_DEVICE, i = 0; dev < NUM_DEVICES; dev++, i++) {
		if (!devstats[dev].scanned) {
			chart[CBAR+i*2].ob_flags |= HIDETREE;
			((TEDINFO *)chart[CBARMAX+i*2].ob_spec)->te_ptext = "";
			strcpy(chart[CTOT+i].ob_spec,"");
			strcpy(chart[CUSED+i].ob_spec,"");
			strcpy(chart[CFREE+i].ob_spec,"");
			strcpy(chart[CFREEP+i].ob_spec,"");
		}
	}
	strcpy(chart[TTOT].ob_spec,"");
	strcpy(chart[TUSED].ob_spec,"");
	strcpy(chart[TFREE].ob_spec,"");
	strcpy(chart[TFREEP].ob_spec,"");

	objc_draw(chart,0,MAX_DEPTH,chart->ob_x,chart->ob_y,
			  chart->ob_width,chart->ob_height);

	wind_update(BEG_UPDATE);
	mouse_off();
	set_clip();
	for (dev = FIRST_HD_DEVICE, i = 0; dev < NUM_DEVICES; dev++, i++) {
		if (!((1L<<dev) & drivemap))
			continue;
		if (devstats[dev].scanned)
			continue;

		objc_sel(chart,CHC+i);

		if ((rc=get_mybpb(&dpb,dev)) < 0) {
			if (rc == -EDRVNR) {
				((TEDINFO *)chart[CBARMAX+i*2].ob_spec)->te_ptext = text[DRIVE_NOT_READY].ob_spec;
				redraw_object(chart,CBARMAX+i*2);
			} else xform_alert(1,GEN_BPB_ALERT);
			continue;
		}
		if (dpb.isfat16) {							/* fat16 */
			if ((f16=malloc(dpb.bpf)) == NULL) {
				error(dpb.bpf,MEMALLOC,"");
				continue;
			}
			if (log_read(dev,dpb.spf,1,f16,FAT)) {
				free(f16);
				continue;
			}
		} else {									/* fat12 */
			if ((f12=malloc(dpb.bpf * 2L)) == NULL) {
				error(dpb.bpf*2,MEMALLOC,"");
				continue;
			}
			if ((f16=malloc(dpb.bpf * 4L)) == NULL) {
				error(dpb.bpf*4,MEMALLOC,"");
				free(f12);
				continue;
			}
			if (log_read(dev,dpb.spf,1,f12,FAT)) {
				free(f12);
				free(f16);
				continue;
			}
			fat12_to_fat16(f12,f16);
			free(f12);
		}

		tbyte = (long)dpb.cdata * dpb.bpc;
		for (j = 2, free_clus = 0; j < dpb.maxc; j++)
			if (!f16[j])
				free_clus++;
		bfree = (long)free_clus * dpb.bpc;

		load_cstr(chart[CTOT+i].ob_spec,tbyte,0);
		load_cstr(chart[CUSED+i].ob_spec,tbyte-bfree,0);
		load_cstr(chart[CFREE+i].ob_spec,bfree,0);

		devstats[dev].total = tbyte;
		devstats[dev].free = bfree;

		if (tbyte > LONG_MAX/1000) {			/* scale to avoid overflow */
			bfree /= 1024;						/* scaling factor for faster calc */
			tbyte /= 1024;
		}
		thousandths = (tbyte - bfree) * 1000 / tbyte;	/* used */
		chart[CBAR+i*2].ob_width = chart[CBARMAX+i*2].ob_width * thousandths / 1000;

		thousandths = 1000 - thousandths;				/* free */
		if (thousandths >= 1000) {
			strcpy(chart[CFREEP+i].ob_spec,"100.");
		} else {
			chart[CBAR+i*2].ob_flags &= ~HIDETREE;
			sprintf(chart[CFREEP+i].ob_spec,"%2d.%1d",thousandths/10,thousandths%10);
			redraw_object(chart,CBAR+i*2);
		}

		redraw_object(chart,CTOT+i);
		redraw_object(chart,CUSED+i);
		redraw_object(chart,CFREE+i);
		redraw_object(chart,CFREEP+i);

		objc_dsel(chart,CHC+i);
		devstats[dev].scanned = TRUE;
		free(f16);
	}

	for (i = FIRST_HD_DEVICE; i < NUM_DEVICES; i++) {
		ttot += (devstats[i].total+KBYTE/2) / KBYTE;	/* scale for display */
		tfree += (devstats[i].free+KBYTE/2) / KBYTE;
	}

	load_cstr(chart[TTOT].ob_spec,ttot,1);
	load_cstr(chart[TUSED].ob_spec,ttot-tfree,1);
	load_cstr(chart[TFREE].ob_spec,tfree,1);

	if (tfree > LONG_MAX/1000) {			/* scale to avoid overflow */
		tfree /= 1024;						/* scaling factor for faster calc */
		ttot /= 1024;
	}
	thousandths = tfree * 1000 / ttot;		/* free */

	if (thousandths >= 1000)
		strcpy(chart[TFREEP].ob_spec,"100.");
	else sprintf(chart[TFREEP].ob_spec,"%2d.%1d",thousandths/10,thousandths%10);

	redraw_object(chart,TTOT);
	redraw_object(chart,TUSED);
	redraw_object(chart,TFREE);
	redraw_object(chart,TFREEP);

	mouse_on();
	wind_update(END_UPDATE);

	dev = savedev;
	get_mybpb(&dpb,dev);
}

/*
 *	the following routine is called:
 *		. whenever we switch away from ALLINFO
 *		. at termination
 *
 *	for now it does nothing
 */
PUBLIC void cleanup_allinfo()
{
}

/*
 *	convert number to printable 12-character string
 *		if 'k' is non-zero, it indicates that the number is already in kB
 */
PRIVATE void load_cstr(char *str,long num,int k)
{
long high, mid, low;
long temp;

	/*
	 *	handle pre-scaled display ...
	 */
	if (k) {
		high = num / MBYTE;
		temp = num - (high * MBYTE);
		mid = temp / KBYTE;
		low = temp - (mid * KBYTE);
		if (high)
			sprintf(str,"%5ld.%03ld GB",high,mid);
		else if (mid)
			sprintf(str,"%5ld.%03ld MB",mid,low);
		else sprintf(str,"%9ld KB",low);
		return;
	}

	/*
	 *	handle normal display ...
	 */
	high = num / 1000000L;
	temp = num - (high * 1000000L);
	mid = temp / 1000L;
	low = temp - (mid * 1000L);

	if (high)
		sprintf(str,"%4ld,%03ld,%03ld",high,mid,low);
	else if (mid)
		sprintf(str,"%8ld,%03ld",mid,low);
	else sprintf(str,"%12ld",low);
}

/**************************************************************************/

/*
 *	file read with optional retry
 */
PUBLIC long gemdos_read(short fh,long count,void *buf,char *name,int pgmloc)
{
long pos, rc;

	if ((pos=Fseek(0L,fh,SEEK_CUR)) < 0L)
		return -1L;

	while(1) {
		if ((rc=Fread(fh,count,buf)) == count)
			break;
		if (!dos_retry(rc,READ,name,pgmloc))
			return -1L;
		if (Fseek(pos,fh,SEEK_SET) != pos)
			return -1L;
	}

	return 0L;
}

/*
 *	file write with optional retry
 */
PUBLIC long gemdos_write(short fh,long count,void *buf,char *name,int pgmloc)
{
long pos, rc;

	if ((pos=Fseek(0L,fh,SEEK_CUR)) < 0L)
		return -1L;

	while(1) {
		if ((rc=Fwrite(fh,count,buf)) == count)
			break;
		if (!dos_retry(rc,WRITE,name,pgmloc))
			return -1L;
		if (Fseek(pos,fh,SEEK_SET) != pos)
			return -1L;
	}

	return 0L;
}

/*
 *	dos_retry: issue warning alert for GEMDOS i/o error
 *		return 1 for retry, 0 otherwise
 */
PRIVATE int dos_retry(int errnum,int type,char *msg,int pgmloc)
{
char fname[MAX_ALERTLINE_SIZE+1];

	trim_name(fname,msg,MAX_ALERTLINE_SIZE);		/* trim long filenames to fit */
	if (xform_alert(1,ER_DOSRET_ALERT,text[ERROR_TYPE+type].ob_spec,fname,pgmloc,errnum) == 1)
		return 1;

	return 0;
}

/*
 *	logical read with optional retry
 */
PUBLIC long logical_read(short dev,short numsec,long start,void *buf,int errmsgno,int pgmloc)
{
long rc;

	while(1) {
		if ((rc=Rwabs(2,buf,numsec,start,dev)) == 0L)
			break;
		if (!sector_retry(rc,READ,errmsgno,start,pgmloc))
			return -1L;
	}

	return 0L;
}

/*
 *	logical write with optional retry
 */
PUBLIC long logical_write(short dev,short numsec,long start,void *buf,int errmsgno,int pgmloc)
{
long rc;

	while(1) {
		if ((rc=Rwabs(3,buf,numsec,start,dev)) == 0L)
			break;
		if (!sector_retry(rc,WRITE,errmsgno,start,pgmloc))
			return -1L;
	}

	return 0L;
}

/*
 *	sector_retry: issue warning alert for logical sector i/o error
 *		return 1 for retry, 0 otherwise
 */
PRIVATE int sector_retry(long errnum,int type,int errmsgno,int sector,int pgmloc)
{
	if (xform_alert(1,ER_BIOSRET_ALERT,text[ERROR_TYPE+type].ob_spec,
					text[ERROR_LOC+errmsgno].ob_spec,sector,pgmloc,errnum) == 1)
		return 1;

	return 0;
}

/*
 *	logical sector read with automatic multisector retries and subsequent sector-by-sector retry:
 *	used for read operations that *must* succeed
 */
PUBLIC int critical_read(short dev,short numsec,long start,char *buf,int errmsgno,int pgmloc)
{
int i, errors, tries;
long rc;

	ce_off();
	for (tries = 0; tries < MAX_TRIES; tries++)
		if ((rc=Rwabs(2,buf,numsec,start,dev)) == 0L)
			break;
	ce_on();
	if (rc == 0L)
		return 0;

	ce_off();
	for (i = 0, errors = 0; i < numsec; i++, buf += dpb.bps) {
		for (tries = 0; tries < MAX_TRIES; tries++)
			if (Rwabs(2,buf,1,start+i,dev) == 0L)
				break;
		if (tries >= MAX_TRIES)
			errors++;
	}
	ce_on();

	if (errors) {
		disastrous_error(READ,errmsgno,start,start+numsec-1,pgmloc);
		return -1;
	}

	return 0;
}

/*
 *	logical sector write with automatic multisector retries and subsequent sector-by-sector retry:
 *	used for write operations that *must* succeed
 */
PUBLIC int critical_write(short dev,short numsec,long start,char *buf,int errmsgno,int pgmloc)
{
int i, errors, tries;
long rc;

	ce_off();
	for (tries = 0; tries < MAX_TRIES; tries++)
		if ((rc=Rwabs(3,buf,numsec,start,dev)) == 0L)
			break;
	ce_on();
	if (rc == 0L)
		return 0;

	ce_off();
	for (i = 0, errors = 0; i < numsec; i++, buf += dpb.bps) {
		for (tries = 0; tries < MAX_TRIES; tries++)
			if (Rwabs(3,buf,1,start+i,dev) == 0L)
				break;
		if (tries >= MAX_TRIES)
			errors++;
	}
	ce_on();

	if (errors) {
		disastrous_error(WRITE,errmsgno,start,start+numsec-1,pgmloc);
		return -1;
	}

	return 0;
}

/*
 *	report unrecoverable read/write error via alert
 */
PRIVATE void disastrous_error(int type,int errmsgno,int start,int end,int pgmloc)
{
	xform_alert(1,ER_DISAST_ALERT,text[ERROR_TYPE+type].ob_spec,
				text[ERROR_LOC+errmsgno].ob_spec,start,end,pgmloc);
}

/*
 *	read a sector without retry
 */
PUBLIC int read_no_retry(short dev,short numsec,long start,char *buf)
{
long rc;

	ce_off();
	rc = Rwabs(6,buf,numsec,start,dev);
	ce_on();

	return rc;
}

/*
 *	write a sector without retry
 */
PUBLIC int write_no_retry(short dev,short numsec,long start,char *buf)
{
long rc;

	ce_off();
	rc = Rwabs(7,buf,numsec,start,dev);
	ce_on();

	return rc;
}
