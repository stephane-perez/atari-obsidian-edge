/*
 *	opt2.c: implements the optimisation functions
 *
 *
 *	version 2.50 (april/2003)		roger burrows
 *		a cleaned up and reorganised version of
 *		the DE v2.04 source; no new function
 *
 *	version 2.51 (may/2003)			roger burrows
 *		fix bug in opt_full():
 *			instead of using dpb.rds for the size of the root (in logical sectors), we
 *			calculated it from the number of directory entries, but did not round up.
 *			in some cases (e.g. logical sector size = 16K, # root dir entries = 256),
 *			this caused fewer sectors to be written than should have been, which munged
 *			a partition if any entries in the root were deleted AND the 'Remove deleted
 *			entries' option was set.
 *
 *	version 2.52 (june/2003)		roger burrows
 *		fix bug in a_index():
 *			the sort of indx[] looped forever iff n == 1.  this occurred when a_index()
 *			was entered with one remaining cluster to move.  the simplest situation is
 *			when optimizing a partition from which all files have been deleted except
 *			one, which itself occupies only a single cluster.  however, this could occur
 *			any time the number of clusters to move is (n * M + 1), where M is the size
 *			of the input buffer in clusters.
 */
#include <stdio.h>
#include <stdlib.h>
#include <aes.h>
#include <assert.h>
#include <dos.h>
#include <portab.h>
#include <string.h>

#include "edge.h"
#include "edgedefs.h"
#include "miscdef.h"
#include "map.h"
#include "proto.h"

#define MODULE_NUMBER	10		/* used by error() macro */

#define NEW_WAY					/* undefine this to get the old memory alloc method in do_defrag() */

extern short dev, dirlevel, win1;
extern short rez;				/* char height / 8 */
extern short doing_opt;
extern MYBPB dpb;
extern DIRLIST dirlist[MAX_DIR_DEPTH];
extern OPT opt;
extern STATUS status;
extern OBJECT *dmap, *obar, *text;


/*
 *	private variables
 */
MLOCAL char *fat12;
MLOCAL unsigned short *fat16;
MLOCAL char *rootbuf;
MLOCAL unsigned short *newfat;
MLOCAL short *fmold, *fmnew;	/* used by opt_full() & friends only */
MLOCAL short nextcluster;		/* likewise */
MLOCAL short nbad;
MLOCAL short *startf, *cumf;	/* used by opt_compress() & friends only */
MLOCAL short *bad;				/* likewise */

MLOCAL void (*update_newfat)(short);
MLOCAL int (*update_map)(short);

MLOCAL short obarx, obary, ototbar;	/* for managing the optimisation bar */
MLOCAL long obarmax;


/*
 *	function prototypes
 */
PRIVATE void a_index(short n,short *arrin,short *indx);
PRIVATE int alloc_badclus_array(void);
PRIVATE int alloc_hole_arrays(void);
PRIVATE int calc_clusters(void);
PRIVATE int comp_read(void);
PRIVATE int comp_write(void);
PRIVATE int do_defrag(int show);
PRIVATE short end_free_cluster(short startnum);
PRIVATE short end_nonbad_cluster(short startnum,short maxcount);
PRIVATE short end_used_cluster(short startnum,short maxcount);
PRIVATE void full_obar(void);
PRIVATE short get_next_readopt_cluster(short previous);
PRIVATE short get_next_writeopt_cluster(short previous);
PRIVATE void init_obar(long max);
PRIVATE short next_free_cluster(short startnum);
PRIVATE short next_nonbad_cluster(short startnum);
PRIVATE short next_used_cluster(short startnum);
PRIVATE int opt_compress(void);
PRIVATE int opt_full(void);
PRIVATE short prev_nonbad_cluster(short startnum);
PRIVATE short prev_used_cluster(short startnum);
PRIVATE int setup_compress_dir(short dstart,DIRENTRY *end);
PRIVATE int setup_defrag_dir(short dstart,unsigned char *end);
PRIVATE void start_obar(unsigned char *str1);
PRIVATE short start_nonbad_cluster(short startnum,short maxcount);
PRIVATE short start_used_cluster(short startnum,short maxcount);
PRIVATE int up_map_bad(short start);
PRIVATE int up_map_good(short start);
PRIVATE void up_newfat_bad(short cnt);
PRIVATE void up_newfat_good(short cnt);
PRIVATE void update_compread(short oldstart);
PRIVATE void update_compwrite(short oldstart);
PRIVATE void update_obar(long num);
PRIVATE void update_obarmsg(unsigned char *strng);
PRIVATE void wrap_obar(void);


/********************************************************/
/*														*/
/*		C O M P R E S S   F R E E   S P A C E			*/
/*														*/
/********************************************************/

PUBLIC short do_opt_compress(short display)
{
int rc;

	if (do_tdisk(1) < 0) {			//MAGNUM
		force_update(-1);
		doing_opt = 0;				/* probably unnecessary */
		return -1;
	}

	toggle_bottom(BDOOPT);
	doing_opt = 1;					/* flag for bpb with DATAlite */

	wind_update(BEG_UPDATE);
	mouse_off();
	rc = opt_compress();
	mouse_on();
	wind_update(END_UPDATE);

	if (fat16)
		free(fat16);
	fat16 = NULL;

	if (fat12)
		free(fat12);
	fat12 = NULL;

	if (rootbuf)
		free(rootbuf);
	rootbuf = NULL;

	if (newfat)
		free(newfat);
	newfat = NULL;

	if (startf)
		free(startf);
	startf = NULL;

	if (cumf)
		free(cumf);
	cumf = NULL;

	if (bad)
		free(bad);
	bad = NULL;

	_mediach(dev);
	update_barmsg(text[PROG_OPTCOMPLETE].ob_spec);
	wrap_bar();

	status.mapdev = -1;			/* force updates the next time these windows are displayed */
	status.infodev = -1;

	if (rc) {
		xform_alert(1,GEN_OPFAIL_ALERT);
		if (status.action == DISKMAP)
			change_mode(ALLINFO);
	}
	doing_opt = 0;				/* flag for bpb with DATAlite */

	if (display)
		force_update(-1);

	return rc;
}

/*
 *	perform freespace compression only
 *
 *	for READ priority:
 *		startf[0] is the cluster# of the start of the first 'hole' (unallocated space), counting from cluster 2 up
 *		startf[1]                ..                   second                         ..
 *		   ...
 *		startf[n] is maxcluster+1
 *		cumf[0] is 0
 *		cumf[1] is the size in clusters of hole #1
 *		cumf[2] is the size in clusters of hole #1 + hole #2
 *		   ...
 *
 *	for WRITE priority:
 *		startf[0] is dpb.maxc
 *		startf[1] is the cluster# of the start of the first 'hole' (unallocated space), counting from maxcluster down
 *		startf[2]                ..                   second                         ..
 *		   ...
 *		cumf[0] is 0
 *		cumf[1] is the size in clusters of hole #1
 *		cumf[2] is the size in clusters of hole #1 + hole #2
 *		   ...
 */
PRIVATE int opt_compress(void)
{
short i, j, totfree;
int fail;
char msg[MAX_DISPLAYSIZE];
DIRENTRY *temp;

	sprintf(msg,text[PROG_COMPRESSING].ob_spec,dev+'A');
	start_bar(msg);

	if (get_mybpb(&dpb,dev) < 0) {
		xform_alert(1,GEN_BPB_ALERT);
		return -1;
	}

	if (dpb.isfat16) {								/* fat16 */
		if ((newfat=calloc(1,dpb.bpf)) == NULL) {
			error(dpb.bpf,MEMALLOC,"");
			return -1;
		}
		if ((fat16=malloc(dpb.bpf)) == NULL) {
			error(dpb.bpf,MEMALLOC,"");
			return -1;
		}
		if (log_read(dev,dpb.spf,1,fat16,FAT))
			return -1;
		swab(fat16,fat16,dpb.bpf);
	} else {										/* fat12 */
		if ((newfat=calloc(4,dpb.bpf)) == NULL) {
			error(4*dpb.bpf,MEMALLOC,"");
			return -1;
		}
		if ((fat12=calloc(2,dpb.bpf)) == NULL) {
			error(2*dpb.bpf,MEMALLOC,"");
			return -1;
		}
		if ((fat16=calloc(4,dpb.bpf)) == NULL) {
			error(4*dpb.bpf,MEMALLOC,"");
			return -1;
		}
		if (log_read(dev,dpb.spf,1,fat12,FAT))
			return -1;
		fat12_to_fat16(fat12,fat16);
	}

	if ((rootbuf=malloc(dpb.rdb)) == NULL) {
		error(dpb.rdb,MEMALLOC,"");
		return -1;
	}
	if (log_read(dev,dpb.rds,2*dpb.spf+1,rootbuf,DIR))
		return -1;

	memset(&dirlist[0],0x00,MAX_DIR_DEPTH*sizeof(DIRLIST));
	dirlist[0].dirbuf = rootbuf;
	dirlist[0].firstcluster = 0;
	dirlevel = 0;

	if (alloc_badclus_array() < 0)
		return -1;

	if (alloc_hole_arrays() < 0)
		return -1;

	if (opt.priority == READ) {
		cumf[0] = 0;
		for (i = 2, j = 0, totfree = 0; i < dpb.maxc; i++) {
			if (fat16[i] == 0) {
				totfree++;
				startf[j++] = i;				/* find the holes and build */
				for (i++; i < dpb.maxc; i++) {	/* start and cum arrays */
					if (fat16[i])
						break;
					totfree++;
				}
				cumf[j] = totfree;
				i--;
			}
		}
		startf[j] = prev_used_cluster(dpb.maxc) + 1;	/* last used + 1 */
	} else {		/* opt.priority == WRITE */
		startf[0] = dpb.maxc;
		cumf[0] = 0;
		for (i = dpb.maxc-1, j = 1, totfree = 0; i >= 2; i--) {
			if (fat16[i] == 0) {
				totfree++;					/* find the holes and build */
				for (i--; i >= 2; i--) {	/* start and cum arrays */
					if (fat16[i])
						break;				/* reverse order for opt write */
					totfree++;
				}
				startf[j] = ++i;
				cumf[j++] = totfree;
			}
		}
	}

	temp = (DIRENTRY *) sort_root(dirlist[0].dirbuf,opt.rmdel);

	update_barmsg(text[PROG_SCANFOLDER].ob_spec);

	if (setup_compress_dir(0,temp) < 0)				//MAGNUM
		return -1;

	if (dpb.isfat16) {				/* fat16 */
		fat12 = (char *)newfat;
		newfat = NULL;
		swab(fat12,fat12,dpb.bpf);
	} else {						/* fat12 */
		fat16_to_fat12(newfat,fat12);
		free(newfat);
		newfat = NULL;
	}

	init_bar(calc_clusters());

	if (dpb.datalite) {
		update_barmsg(text[PROG_WORKING].ob_spec);

		if (opt.priority == READ)
			fail = comp_read();
		else fail = comp_write();

		update_barmsg(text[PROG_ADJUSTFAT].ob_spec);
		must_write(dev,dpb.spf,1,fat12,FAT);
		must_write(dev,dpb.spf,dpb.spf+1,fat12,FAT);
		must_write(dev,dpb.rds,2*dpb.spf+1,rootbuf,DIR);
		free(fat12);
		fat12 = NULL;

	} else {
		update_barmsg(text[PROG_ADJUSTFAT].ob_spec);
		must_write(dev,dpb.spf,1,fat12,FAT);
		must_write(dev,dpb.spf,dpb.spf+1,fat12,FAT);
		must_write(dev,dpb.rds,2*dpb.spf+1,rootbuf,DIR);
		free(fat12);
		fat12 = NULL;
		update_barmsg(text[PROG_WORKING].ob_spec);

		if (opt.priority == READ)
			fail = comp_read();
		else fail = comp_write();
	}
	full_bar();
	ring_bell();

	return fail;
}

PRIVATE int alloc_badclus_array(void)
{
int i, j;
unsigned short *f16;

	for (i = 2, f16 = &fat16[i], nbad = 0; i < dpb.maxc; i++) {
		if (*f16++ == dpb.badmask) {
			newfat[i] = dpb.badmask;
			nbad++;
		}
	}
	if (nbad) {
		if ((bad=calloc(nbad,sizeof(short))) == NULL) {
			error(nbad*sizeof(short),MEMALLOC,"");
			return -1;
		}
		if (opt.priority == READ) {
			for (i = 2, f16 = &fat16[i], j = 0; i < dpb.maxc; i++)
				if (*f16++ == dpb.badmask)
					bad[j++] = i;
		} else {					/* opt.priority == WRITE */
			for (i = dpb.maxc-1, f16 = &fat16[i], j = 0; i >= 2; i--)
				if (*f16-- == dpb.badmask)
					bad[j++] = i;
		}
	}

	return 0;
}

/*
 *	allocate startf[], cumf[] arrays
 */
PRIVATE int alloc_hole_arrays(void)
{
int i;
int holes;

	for (i = 0, holes = 0; i < dpb.maxc; i++) {
		i = next_free_cluster(i);
		if (i >= dpb.maxc)
			break;
		i = end_free_cluster(i);
		holes++;
	}

	holes += 2;		/* should probably be += 1, but safety ... */

	startf = calloc(holes,sizeof(short));
	cumf = calloc(holes,sizeof(short));

	if (startf && cumf)
		return 0;

	return -1;
}

/*
 *	return number of next free cluster after specified one
 */
PRIVATE short next_free_cluster(short startnum)
{
int i;
unsigned short *f16;

	for (i = startnum+1, f16 = &fat16[i]; i < dpb.maxc; i++)
		if (*f16++ == 0)
			break;
	return i;
}

/*
 *	return number of last free cluster of run
 */
PRIVATE short end_free_cluster(short startnum)
{
int i;
unsigned short *f16;

	for (i = startnum+1, f16 = &fat16[i]; i < dpb.maxc; i++)
		if (*f16++ != 0)
			break;
	return i-1;
}

/*
 *	calculate clusters that will be moved
 *	(must match comp_read()/comp_write() logic)
 */
PRIVATE int calc_clusters(void)
{
int total = 0;
short readpos, read_start, read_end;
short writepos, write_start, write_end, count;

	if (opt.priority == READ) {
		for (readpos = writepos = 1; readpos < dpb.maxc; ) {
			read_start = next_used_cluster(readpos);
			if (read_start >= dpb.maxc)			/* trailing free space */
				break;
			read_end = end_used_cluster(read_start,dpb.maxc);

			write_start = next_nonbad_cluster(writepos);
			write_end = end_nonbad_cluster(write_start,dpb.maxc);

			count = min(write_end-write_start,read_end-read_start) + 1;
			readpos = read_start + count - 1;
			writepos = write_start + count - 1;

			total += count;
		}
	} else {
		for (readpos = writepos = dpb.maxc; readpos >= 2; ) {
			read_end = prev_used_cluster(readpos);
			if (read_end < 2)				/* leading free space */
				break;
			read_start = start_used_cluster(read_end,dpb.maxc);

			write_end = prev_nonbad_cluster(writepos);
			write_start = start_nonbad_cluster(write_end,dpb.maxc);

			count = min(write_end-write_start,read_end-read_start) + 1;
			readpos = read_end - count + 1;
			writepos = write_end - count + 1;

			total += count;
		}
	}

	return total;
}


/****************************************************************
*																*
*	compress free space (read priority): move data clusters		*
*																*
****************************************************************/
PRIVATE int comp_read(void)
{
char *newbuf = NULL;
int nclus, total;
short readpos, read_start, read_end;
short writepos, write_start, write_end, count;

	nclus = max_allocatable_clusters(dpb.bpc);
	if (nclus)
		newbuf = malloc(nclus*dpb.bpc);
	if (!newbuf) {
		error(nclus*dpb.bpc,MEMALLOC,"");
		return -1;
	}

	for (readpos = writepos = 1, total = 0; readpos < dpb.maxc; ) {
		read_start = next_used_cluster(readpos);
		if (read_start >= dpb.maxc)			/* trailing free space */
			break;
		read_end = end_used_cluster(read_start,nclus);

		write_start = next_nonbad_cluster(writepos);
		write_end = end_nonbad_cluster(write_start,nclus);

		count = min(write_end-write_start,read_end-read_start) + 1;
		must_read(dev,count*dpb.spc,logsector(read_start),newbuf,HARDDISK);
		must_write(dev,count*dpb.spc,logsector(write_start),newbuf,HARDDISK);
		readpos = read_start + count - 1;
		writepos = write_start + count - 1;

		total += count;
		update_bar(total);
	}

	free(newbuf);

	return 0;
}

/*
 *	return number of next used cluster after specified one
 */
PRIVATE short next_used_cluster(short startnum)
{
int i;
unsigned short *f16;

	for (i = startnum+1, f16 = &fat16[i]; i < dpb.maxc; i++, f16++)
		if ((*f16 != 0) && (*f16 != dpb.badmask))
			break;
	return i;
}

/*
 *	return number of last used cluster of run (with a limit of 'maxcount')
 */
PRIVATE short end_used_cluster(short startnum,short maxcount)
{
int i, stop;
unsigned short *f16;

	stop = min(startnum+maxcount,dpb.maxc);
	for (i = startnum+1, f16 = &fat16[i]; i < stop; i++, f16++)
		if ((*f16 == 0) || (*f16 == dpb.badmask))
			break;
	return i-1;
}

/*
 *	return number of next non-bad cluster after specified one
 */
PRIVATE short next_nonbad_cluster(short startnum)
{
int i;
unsigned short *f16;

	for (i = startnum+1, f16 = &fat16[i]; i < dpb.maxc; i++)
		if (*f16++ != dpb.badmask)
			break;
	return i;
}

/*
 *	return number of last non-bad cluster of run (with a limit of 'maxcount')
 */
PRIVATE short end_nonbad_cluster(short startnum,short maxcount)
{
int i, stop;
unsigned short *f16;

	stop = min(startnum+maxcount,dpb.maxc);
	for (i = startnum+1, f16 = &fat16[i]; i < stop; i++)
		if (*f16++ == dpb.badmask)
			break;
	return i-1;
}


/****************************************************************
*																*
*	compress free space (write priority): move data clusters	*
*																*
****************************************************************/
PRIVATE int comp_write(void)
{
char *newbuf = NULL;
int nclus, total;
short readpos, read_start, read_end;
short writepos, write_start, write_end, count;

	nclus = max_allocatable_clusters(dpb.bpc);
	if (nclus)
		newbuf = malloc(nclus*dpb.bpc);
	if (!newbuf) {
		error(nclus*dpb.bpc,MEMALLOC,"");
		return -1;
	}

	for (readpos = writepos = dpb.maxc, total = 0; readpos >= 2; ) {
		read_end = prev_used_cluster(readpos);
		if (read_end < 2)				/* leading free space */
			break;
		read_start = start_used_cluster(read_end,nclus);

		write_end = prev_nonbad_cluster(writepos);
		write_start = start_nonbad_cluster(write_end,nclus);

		count = min(write_end-write_start,read_end-read_start) + 1;
		readpos = read_end - count + 1;
		writepos = write_end - count + 1;
		must_read(dev,count*dpb.spc,logsector(readpos),newbuf,HARDDISK);
		must_write(dev,count*dpb.spc,logsector(writepos),newbuf,HARDDISK);

		total += count;
		update_bar(total);
	}

	free(newbuf);

	return 0;
}

/*
 *	return number of first used cluster before specified one
 */
PRIVATE short prev_used_cluster(short startnum)
{
int i;
unsigned short *f16;

	for (i = startnum-1, f16 = &fat16[i]; i >= 2; i--, f16--)
		if ((*f16 != 0) && (*f16 != dpb.badmask))
			break;
	return i;
}

/*
 *	return number of first used cluster of run (with a limit of 'maxcount')
 */
PRIVATE short start_used_cluster(short startnum,short maxcount)
{
int i, stop;
unsigned short *f16;

	stop = max(startnum-maxcount+1,2);
	for (i = startnum-1, f16 = &fat16[i]; i >= stop; i--, f16--)
		if ((*f16 == 0) || (*f16 == dpb.badmask))
			break;
	return i+1;
}

/*
 *	return number of first non-bad cluster before specified one
 */
PRIVATE short prev_nonbad_cluster(short startnum)
{
int i;
unsigned short *f16;

	for (i = startnum-1, f16 = &fat16[i]; i >= 2; i--)
		if (*f16-- != dpb.badmask)
			break;
	return i;
}

/*
 *	return number of first non-bad cluster of run (with a limit of 'maxcount')
 */
PRIVATE short start_nonbad_cluster(short startnum,short maxcount)
{
int i, stop;
unsigned short *f16;

	stop = max(startnum-maxcount+1,2);
	for (i = startnum-1, f16 = &fat16[i]; i >= stop; i--)
		if (*f16-- == dpb.badmask)
			break;
	return i+1;
}

/************************************************************************
*																		*
*	compress free space: update cluster numbers in FAT & directories	*
*																		*
************************************************************************/
PRIVATE int setup_compress_dir(short dstart,DIRENTRY *end)
{
DIRENTRY *curfile;
DIRENTRY *newdir;
short oldstart;
short ncd, ncl, st;
short olddirstart;
long offset;
DIRENTRY *dirend;

	curfile = (DIRENTRY *)dirlist[dirlevel].dirbuf;
	if (dirlevel == 0) {
		if (curfile->attr == FA_VOLUME)
			curfile++;
	} else curfile += 2;

	for ( ; curfile->attr == FA_SUBDIR; curfile++) {
		if (curfile->name[0] == DELETION_MARK)
			continue;
		dirlevel++;
		st = olddirstart = get_word((char *)&curfile->start);
		ncl = 1;
		while (fat16[st] != dpb.bitmask) {
			st = fat16[st];
			ncl++;
		}
		if ((newdir=(DIRENTRY *)dirlist[dirlevel].dirbuf=calloc(ncl,dpb.bpc)) == NULL) {
			error(ncl*dpb.bpc,MEMALLOC,"");
			dirlevel--;
			if (dirlevel > 0)
				up_dirlist();
			return -1;
		}
		offset = 0L;
		st = olddirstart;		/* read in dir structure */
		do {
			if (log_read(dev,dpb.spc,logsector(st),dirlist[dirlevel].dirbuf+offset,DIR)) {
				curfile->name[0] = DELETION_MARK;
				up_dirlist();
				break;
			}
			st = fat16[st];
			offset += (long) dpb.bpc;
		} while (st != dpb.bitmask);

		if (curfile->name[0] == DELETION_MARK)
			continue;

		if (opt.priority == READ)
			ncd = get_next_readopt_cluster(olddirstart);
		else ncd = get_next_writeopt_cluster(olddirstart);

		putword(&curfile->start,ncd);			/* update parent entry */
		dirlist[dirlevel].firstcluster = ncd;

		putword(&newdir->start,ncd);			/* update child . and .. entries */
		newdir++;
		putword(&newdir->start,dirlist[dirlevel-1].firstcluster);

		if (opt.priority == READ)
			update_compread(olddirstart);
		else update_compwrite(olddirstart);

		dirend = (DIRENTRY *)sort_dir(dirlist[dirlevel].dirbuf,ncl,opt.rmdel);

		if (setup_compress_dir(olddirstart,dirend)) {
			if (dirlevel > 0)
				up_dirlist();
			return -1;
		}
	}

	while (curfile < end) {
		if (curfile->name[0] != DELETION_MARK) {
			oldstart = get_word((char *)&curfile->start);
			if (oldstart) {
				if (opt.priority == READ)
					ncd = get_next_readopt_cluster(oldstart);
				else ncd = get_next_writeopt_cluster(oldstart);
				putword(&curfile->start,ncd);
				if (opt.priority == READ)
					update_compread(oldstart);
				else update_compwrite(oldstart);
			}
		}
		curfile++;
	}

	if (dirlevel > 0) {
		st = dstart;
		offset = 0;

		do {
			must_write(dev,dpb.spc,logsector(st),dirlist[dirlevel].dirbuf+offset,DIR);
			st = fat16[st];
			offset += dpb.bpc;
		} while (st != dpb.bitmask);
		up_dirlist();
	}

	return 0;
}

/*
 *	update cluster numbers for read priority
 */
PRIVATE void update_compread(short oldstart)
{
short nextc, nct;
short st = oldstart;

	nextc = get_next_readopt_cluster(oldstart);

	while (fat16[st] != dpb.bitmask) {
		nct = get_next_readopt_cluster(fat16[st]);
		newfat[nextc] = nct;
		nextc = nct;
		st = fat16[st];
	}
	newfat[nextc] = dpb.bitmask;
}

PRIVATE short get_next_readopt_cluster(short previous)
{
int i;
short nextc;

	for (i = 0; previous > startf[i]; i++) 	/* find the first cluster */
		;
	nextc = previous - cumf[i]; 			/* of the new directory entry */

	if (nbad) {								/* taking bad clusters into account */
		for (i = 0; i < nbad; i++)
			if (previous < bad[i])
				break;
		nextc -= i;

		for (i = 0; i < nbad; i++)
			if ((nextc+i) < bad[i])
				break;
		nextc += i;
	}

	return nextc;
}

/*
 *	update cluster numbers for write priority
 */
PRIVATE void update_compwrite(short oldstart)
{
short nextc, nct;
short st = oldstart;

	nextc = get_next_writeopt_cluster(oldstart);

	while (fat16[st] != dpb.bitmask) {
		nct = get_next_writeopt_cluster(fat16[st]);
		newfat[nextc] = nct;
		nextc = nct;
		st = fat16[st];
	}
	newfat[nextc] = dpb.bitmask;
}

PRIVATE short get_next_writeopt_cluster(short previous)
{
int i;
short nextc;

	for (i = 0; previous < startf[i]; i++)		/* find the first cluster */
		;
	nextc = previous + cumf[i-1];				/* of the new directory entry */

	if (nbad) {									/* taking bad clusters into account */
		for (i = 0; i < nbad; i++)
			if (previous > bad[i])
				break;
		nextc += i;

		for (i = 0; i < nbad; i++)
			if ((nextc - i) > bad[i])
				break;
		nextc -= i;
	}

	return nextc;
}


/********************************************************/
/*														*/
/*		F U L L   O P T I M I S A T I O N   C O D E		*/
/*														*/
/********************************************************/

PUBLIC short do_opt(void)
{
int i, rc;

	if (do_tdisk(1) < 0) {					//MAGNUM
		force_update(-1);
		doing_opt = 0;				/* probably not necessary */
		return -1;
	}

	if (opt.visual) {
		do_diskmap();
	} else {
		redraw_object(dmap,0);
		change_mode(DISKMAP);
	}
	toggle_bottom(BDOOPT);
	doing_opt = 1;				/* flag for bpb with DATAlite */

	wind_update(BEG_UPDATE);
	mouse_off();
	rc = opt_full();
	mouse_on();
	wind_update(END_UPDATE);

	if (fat16)
		free(fat16);
	fat16 = NULL;

	if (fat12)
		free(fat12);
	fat12 = NULL;

	if (rootbuf)
		free(rootbuf);
	rootbuf = NULL;

	if (newfat)
		free(newfat);
	newfat = NULL;

	if (fmold)
		free(fmold);
	fmold = NULL;

	_mediach(dev);

//do we need to reset infodev/mapdev here ???

	if (rc == 0) {
		if (opt.visual) {
			sprintf(dmap[FRAG].ob_spec,"%5d",0);
			sprintf(dmap[FRAGPFF].ob_spec,"%3d.%03d",0,0);
			sprintf(dmap[FRAG100].ob_spec,"%3d.%03d",0,0);
			for (i = 0; i < 7; i++)							//MAGNUM
				dmap[BAR1+i].ob_spec = dmap[B0].ob_spec;
			save_map();
			wrap_obar();
			change_mode(DISKMAP);
		} else {
			do_diskmap();
		}
	} else {
		xform_alert(1,GEN_OPFAIL_ALERT);
		change_mode(ALLINFO);
		force_update(dev);
	}
	doing_opt = 0;				/* flag for bpb with DATAlite */

	return rc;
}

PRIVATE int opt_full(void)
{
short maxcluster;
short i;
char *dirend;
static char title[MAX_DISPLAYSIZE];	/* static because it's a window title */

	sprintf(title,text[OPT_TITLE].ob_spec,dev+'A');
	wind_set(win1,WF_NAME,ADDR(title));
	if (opt.priority == READ)
		wind_set(win1,WF_INFO,ADDR(text[OPT_INFO1].ob_spec));
	else wind_set(win1,WF_INFO,ADDR(text[OPT_INFO2].ob_spec));

	start_obar(text[PROG_READFAT].ob_spec);

	if (get_mybpb(&dpb,dev) < 0) {
		xform_alert(1,GEN_BPB_ALERT);
		return -1;
	}

	if (dpb.isfat16) {							/* fat16 */
		if ((fmold=calloc(2,dpb.bpf)) == NULL) {
			error(2*dpb.bpf,MEMALLOC,"");
			return -1;
		}
		(char *)fmnew = (char *)fmold + dpb.bpf;

		if ((newfat=calloc(1,dpb.bpf)) == NULL) {
			error(dpb.bpf,MEMALLOC,"");
			return -1;
		}
		if ((fat16=calloc(1,dpb.bpf)) == NULL) {
			error(dpb.bpf,MEMALLOC,"");
			return -1;
		}
		if (log_read(dev,dpb.spf,1,fat16,FAT))
			return -1;
		swab(fat16,fat16,dpb.bpf);
	} else {									/* fat12 */
		if ((fmold=calloc(4,dpb.bpf)) == NULL) {
			error(4*dpb.bpf,MEMALLOC,"");
			return -1;
		}
		(char *)fmnew = (char *)fmold + dpb.bpf * 2;

		if ((newfat=calloc(2,dpb.bpf)) == NULL) {
			error(2*dpb.bpf,MEMALLOC,"");
			return -1;
		}
		if ((fat12=calloc(2,dpb.bpf)) == NULL) {
			error(2*dpb.bpf,MEMALLOC,"");
			return -1;
		}
		if ((fat16=calloc(4,dpb.bpf)) == NULL) {
			error(4*dpb.bpf,MEMALLOC,"");
			return -1;
		}
		if (log_read(dev,dpb.spf,1,fat12,FAT))
			return -1;
		fat12_to_fat16(fat12,fat16);
	}

	if ((rootbuf=malloc(dpb.rdb)) == NULL) {
		error(dpb.rdb,MEMALLOC,"");
		return -1;
	}
	if (log_read(dev,dpb.rds,2*dpb.spf+1,rootbuf,DIR))
		return -1;

	for (i = 2, nbad = 0, maxcluster = 0; i < dpb.maxc; i++) {
		if (fat16[i]) {
			if (fat16[i] == dpb.badmask) {
				newfat[i] = dpb.badmask;
				nbad++;
			} else {
				maxcluster++;
			}
		}
	}
	if (nbad) {
		update_map = &up_map_bad;
		update_newfat = &up_newfat_bad;
	} else {
		update_map = &up_map_good;
		update_newfat = &up_newfat_good;
	}

	memset(&dirlist[0],0x00,MAX_DIR_DEPTH*sizeof(DIRLIST));
	dirlist[0].dirbuf = rootbuf;
	dirlist[0].firstcluster = 0;
	dirlevel = 0;
	if (opt.priority == READ) {
		for (nextcluster = 2; fat16[nextcluster] == dpb.badmask; nextcluster++)
			;
	} else {
		nextcluster = dpb.maxc - maxcluster - nbad;
		for ( ; fat16[nextcluster] == dpb.badmask; nextcluster++)
			;
	}
	dirend = sort_root(dirlist[0].dirbuf,opt.rmdel);
	update_obarmsg(text[PROG_SCANFOLDER].ob_spec);

	if (setup_defrag_dir(0,dirend) < 0)
		return -1;

	if (dpb.isfat16) {				/* fat16 */
		free(fat16);
		fat12 = (char *)newfat;
		swab((char *)fat12,(char *)fat12,dpb.bpf);
		fat16 = newfat = NULL;
	} else {						/* fat12 */
		free(fat16);
		fat16_to_fat12(newfat,fat12);
		free(newfat);
		fat16 = newfat = NULL;
	}

	set_clip();

	if (dpb.datalite) {
		update_obarmsg(text[PROG_WORKING].ob_spec);
		if (do_defrag(opt.visual) < 0)
			return -1;
		update_obarmsg(text[PROG_ADJUSTFAT].ob_spec);
		must_write(dev,dpb.spf,1,fat12,FAT);
		must_write(dev,dpb.spf,dpb.spf+1,fat12,FAT);
		must_write(dev,dpb.rds,2*dpb.spf+1,rootbuf,DIR);
		free(fat12);
		fat12 = NULL;
	} else {
		update_obarmsg(text[PROG_ADJUSTFAT].ob_spec);
		must_write(dev,dpb.spf,1,fat12,FAT);
		must_write(dev,dpb.spf,dpb.spf+1,fat12,FAT);
		must_write(dev,dpb.rds,2*dpb.spf+1,rootbuf,DIR);
		free(fat12);
		fat12 = NULL;
		update_obarmsg(text[PROG_WORKING].ob_spec);
		if (do_defrag(opt.visual) < 0)
			return -1;
	}

	full_obar();
	update_obarmsg(text[PROG_OPTCOMPLETE].ob_spec);

	return 0;
}

/***************************************************************************/
/*
 *	memory usage in this routine:
 *		array	#entries	entry size	description
 *		idex	nclus*2			2		array of cluster numbers
 *		newbuf	nclus			bpc		data clusters
 *		oldbuf	nclus			bpc		data clusters
 *
 *	Question: do we really need 2*nclus entries in idex?  from a short inspection
 *	of the code it seems like we need nclus+1.
 *	MAYBE: code a new function that returns the contents of idex[n], and use it
 *	instead of idex[n].  within the new function, keep track of the max value of
 *	n, and display it after defragmentation completes ... of course, the difference
 *	in memory usage between 2*nclus and nclus+1 entries when an entry is 2 bytes
 *	is not really significant anyway ...
 */
PRIVATE int do_defrag(int show)
{
short ii, jj, kk;
short readcnt, freeblocks;
short nextblocks, nextblocke;
short ndo, sclus;
long n, nclus;
#ifndef NEW_WAY
long twobpc;
#endif
short *idex = NULL, *newblock, *oldblock;
char *newbuf, *oldbuf;

#ifdef NEW_WAY			//proposed new way ...
	n = 2 * dpb.bpc + 4;
	nclus = max_allocatable_clusters(n);
	if (nclus)
		idex = malloc(nclus*n);
	if (!idex) {
		error(nclus*n,MEMALLOC,"");
		return -1;
	}

#else			//the old way ...

	twobpc = (long) dpb.bpc * 2L;
	n = (long) Malloc(-1L);
	n = ~(twobpc - 1L) & (n - twobpc);
	nclus = n / (long) dpb.bpc;
	if ((idex=Malloc(n)) == NULL) {
		error(n,MEMALLOC,"");
		return -1;
	}

	/*
	   n -= (long)nclus*4L;
	   n = ~(twobpc-1L) & (n-twobpc);
	   nclus = n/twobpc;
	   newbuf = (char *)idex +(long)nclus*4L;
	   oldbuf = newbuf + (long)nclus*(long)dpb.bpc;
	*/

	n -= (long) nclus * 5L;				//rfb: why 5 ???

	n = ~(twobpc - 1L) & (n - twobpc);
	nclus = n / twobpc;
#endif

	newbuf = (char *) idex + nclus * 4;
	oldbuf = newbuf + nclus * dpb.bpc;

	init_obar(nextcluster);
	for (sclus = 2; fmnew[sclus] == 0; sclus++)
		if (sclus >= dpb.maxc)
			break;

	while (sclus < nextcluster) {
		ndo = min(nextcluster-sclus,nclus);

		/**************** Read in new data for curblock ****************************/

		newblock = &fmnew[sclus];
		oldblock = &fmold[sclus];

		a_index(ndo,newblock,idex);
		for (ii = 1; ii <= ndo; ii++)
			if (newblock[idex[ii]] > 1)
				break;
		while (ii <= ndo) {
			for (jj = ii; jj < ndo; jj++) {
				if (newblock[idex[jj]] + 1 != newblock[idex[jj + 1]])
					break;
				if ((idex[jj] + 1) != idex[jj + 1])
					break;
			}
			readcnt = jj - ii + 1;
			must_read(dev,readcnt*dpb.spc,logsector(newblock[idex[ii]]),newbuf+idex[ii]*dpb.bpc,HARDDISK);
			if (show)
				for (kk = 0; kk < readcnt; kk++)		/* clear it */
					draw_opt(newblock[idex[ii]]+kk,1);

			for (kk = newblock[idex[ii]]; kk < newblock[idex[ii]] + readcnt; fmold[kk++] = 0)
				;
			ii += readcnt;
		}

		update_obar(sclus + (ndo / 4));

		/********************* Read in data that has to be moved ********************/

		ii = 0;
		while (ii < ndo) {
			for (jj = ii; jj < ndo; jj++) 	 	/* Skip ones that don't need moving */
				if (oldblock[jj] > 1)
					break;
			ii = jj;
			if (ii == ndo)
				break;

			for (jj = ii; jj < ndo; jj++) 	 	/* find run size */
				if (oldblock[jj] <= 1)
					break;
			readcnt = jj - ii;
			must_read(dev,readcnt*dpb.spc,logsector(sclus+ii),oldbuf+ii*dpb.bpc,HARDDISK);
			if (show)
				for (kk = 0; kk < readcnt; kk++)	/* clear blocks */
					draw_opt(sclus+ii+kk,1);

			ii += readcnt;
		}

		update_obar(sclus + (ndo / 2));

		/***************** Write out new block **************************/

		ii = 0;
		while (ii < ndo) {
			for (jj = ii; jj < ndo; jj++) {
				if (newblock[jj] != 0) {
					if (show)
						for (kk = 0; kk < jj; kk++)			/* defragged blocks out */
							draw_opt(sclus+kk,2);
					break;		/* skip one that don't need writing */
				}
			}
			ii = jj;
			if (ii == ndo)
				break;
			for (jj = ii; jj < ndo; jj++)
				if (newblock[jj] == 0)
					break;					/* calc runsize */
			readcnt = jj - ii;

			must_write(dev,readcnt*dpb.spc,logsector(sclus+ii),newbuf+ii*dpb.bpc,HARDDISK);
			if (show)
				for (kk = 0; kk < readcnt; kk++)			/* defragged blocks out */
					draw_opt(sclus+ii+kk,2);

			ii += readcnt;
		}

		update_obar(sclus + (3 * ndo / 4));

		/******************** dispose of displaced sectors ***************************/

		nextblocks = sclus + ndo;
		nextblocke = nextblocks + ndo;
		if (nextblocke >= dpb.maxc)
			nextblocke = dpb.maxc - 1;

		a_index(ndo,oldblock,idex);
		for (ii = 1; ii <= ndo; ii++)
			if (oldblock[idex[ii]] > 1)
				break;

		while (ii <= ndo) {
			/* what's been read in is contained in the next scheduled block */

			if (oldblock[idex[ii]] < nextblocke) {
				for (jj = ii; jj < ndo; jj++) {
					if (oldblock[idex[jj]] + 1 != oldblock[idex[jj + 1]])
						break;
					if ((idex[jj] + 1) != idex[jj + 1])
						break;
				}
				if (oldblock[idex[jj]] < nextblocke) {
					readcnt = jj - ii + 1;
				} else {
					readcnt = (nextblocke - oldblock[idex[ii]]);
				}
				n = (long) (((long) oldblock[idex[ii]] - (long) nextblocks)) * dpb.bpc;
				fast_copy(newbuf+n,oldbuf+((long) idex[ii] * dpb.bpc),(long) readcnt * dpb.bpc);
				for (jj = 0; jj < readcnt; jj++)
					fmnew[oldblock[idex[ii + jj]]] = 1;
				ii += readcnt;
			} else {
				/* we have to write it out, but where? */

				/** where it will eventually go is already vacant */
				if (fmold[oldblock[idex[ii]]] == 0) {

					/* find out how many open clusters in a row */
					for (kk = 1; kk < ndo - ii; kk++)
						if (fmold[oldblock[idex[ii]] + kk] != 0)
							break;
					for (jj = ii; jj < ndo; jj++) {
						if (oldblock[idex[jj]] + 1 != oldblock[idex[jj + 1]])
							break;
						if ((idex[jj] + 1) != idex[jj + 1])
							break;
					}
					readcnt = min(jj-ii+1,kk);							/* calc wrtcnt */

					/* E pause(2); */
					must_write(dev,readcnt*dpb.spc,logsector(oldblock[idex[ii]]),oldbuf+idex[ii]*dpb.bpc,HARDDISK);
					if (show)
						for (kk = 0; kk < readcnt; kk++)		/* defragged blocks out */
							draw_opt(oldblock[idex[ii]]+kk,2);

					for (kk = 0; kk < readcnt; kk++)
						fmold[oldblock[idex[ii + kk]]] = 1;
					for (kk = 0; kk < readcnt; kk++)
						fmnew[oldblock[idex[ii + kk]]] = 0;

					ii += readcnt;
				} else {
					for (freeblocks = nextblocks; freeblocks < dpb.maxc; freeblocks++) /* have to find a place to dump it */
						if (fmold[freeblocks] == 0)
							break;
					if (freeblocks >= dpb.maxc) {
						for (freeblocks = 2; freeblocks < dpb.maxc; freeblocks++)
							if (fmold[freeblocks] == 0)
								break;
					}
					if (freeblocks >= dpb.maxc)
						return -1;

					for (jj = freeblocks; jj < (freeblocks + ndo - ii + 1); jj++) {
						if (jj >= dpb.maxc)
							break;
						if (fmold[jj] != 0)
							break;
					}
					if (jj >= dpb.maxc)
						jj = dpb.maxc;

					jj -= freeblocks;	/* size of free slot on disk in clusters */

					for (kk = ii; kk < ndo; kk++)
						if (idex[kk] + 1 != idex[kk + 1])
							break;
					readcnt = min(jj,kk-ii+1);
					must_write(dev,readcnt*dpb.spc,logsector(freeblocks),oldbuf+idex[ii]*dpb.bpc,HARDDISK);
					if (show)
						for (kk = 0; kk < readcnt; kk++)			/* defragged blocks out */
							draw_opt(freeblocks+kk,0);

					for (kk = 0; kk < readcnt; kk++)
						fmold[freeblocks + kk] = oldblock[idex[ii + kk]];
					for (kk = 0; kk < readcnt; kk++)
						fmnew[oldblock[idex[ii + kk]]] = freeblocks + kk;
					ii += readcnt;
				}
			}
		}
		update_obar(sclus + ndo);
		sclus = nextblocks;
	}

#ifdef NEW_WAY
	free(idex);
#else
	Mfree(idex);
#endif

	ring_bell();

	return 0;
}

/***************************************************************************/
PRIVATE int setup_defrag_dir(short dstart, char *end)
{
DIRENTRY *curfile;
DIRENTRY *newdir;
short oldstart;
short ncl, st;
short olddirstart;
long offset;
char *dirend;

	curfile = (DIRENTRY *)dirlist[dirlevel].dirbuf;
	if (dirlevel == 0) {
		if (curfile->attr == FA_VOLUME)
			curfile++;
	} else curfile += 2;

	for ( ; curfile->attr == FA_SUBDIR; curfile++) {
		if (curfile->name[0] == DELETION_MARK)
			continue;
		dirlevel++;
		st = olddirstart = get_word((char *)&curfile->start);
		putword(&curfile->start, nextcluster);
		dirlist[dirlevel].firstcluster = nextcluster;
		ncl = 1;
		while (fat16[st] != dpb.bitmask) {
			st = fat16[st];
			ncl++;
		}
		if ((newdir=(DIRENTRY *)dirlist[dirlevel].dirbuf=calloc(ncl,dpb.bpc)) == NULL) {
			error(ncl*dpb.bpc,MEMALLOC,"");
			dirlevel--;
			if (dirlevel > 0)
				up_dirlist();
			return -1;
		}
		offset = 0L;
		st = olddirstart;
		do {
			if (log_read(dev,dpb.spc,logsector(st),dirlist[dirlevel].dirbuf+offset,DIR)) {
				curfile->name[0] = DELETION_MARK;
				up_dirlist();
				break;
			}
			st = fat16[st];
			offset += (long) dpb.bpc;
		} while (st != dpb.bitmask);

		if (curfile->name[0] == DELETION_MARK)
			continue;

		putword(&newdir->start,nextcluster);
		newdir++;
		putword(&newdir->start,dirlist[dirlevel-1].firstcluster);
		update_newfat(update_map(olddirstart));

		dirend = sort_dir(dirlist[dirlevel].dirbuf,ncl,opt.rmdel);

		if (setup_defrag_dir(olddirstart,dirend)) {
			if (dirlevel > 0)
				up_dirlist();
			return -1;
		}
	}

	while ((void *) curfile < (void *) end) {
		if (curfile->name[0] != DELETION_MARK) {
			oldstart = get_word((char *)&curfile->start);
			if (oldstart) {
				putword(&curfile->start,nextcluster);
				update_newfat(update_map(oldstart));
			}
		}
		curfile++;
	}

	if (dirlevel > 0) {
		st = dstart;
		offset = 0;

		do {
			must_write(dev,dpb.spc,logsector(st),dirlist[dirlevel].dirbuf+offset,DIR);
			st = fat16[st];
			offset += dpb.bpc;
		} while (st != dpb.bitmask);
		up_dirlist();
	}

	return 0;
}

/***************************************************************************/
PRIVATE int up_map_good(short start)
{
short cnt = 0;
short nc = nextcluster;

	do {
		if (nc == start) {
			fmnew[nc] = 0;
			fmold[start] = 0;
			nc++;
		} else {
			fmnew[nc] = start;
			fmold[start] = nc++;
		}
		start = fat16[start];
		cnt++;
	} while(start != dpb.bitmask);
	return cnt;
}

/***************************************************************************/
PRIVATE void up_newfat_good(short cnt)
{
short i;

	for (i = 0; i < cnt - 1; i++)
		newfat[nextcluster] = ++nextcluster;
	newfat[nextcluster++] = dpb.bitmask;
}

/***************************************************************************/
PRIVATE int up_map_bad(short start)
{
short cnt = 0;
short nc = nextcluster;

	do {
		if (nc == start) {
			fmnew[nc] = 0;
			fmold[start] = 0;
			nc++;
		} else {
			fmnew[nc] = start;
			fmold[start] = nc++;
		}
		while (fat16[nc] == dpb.badmask)
			nc++;
		start = fat16[start];
		cnt++;
	} while(start != dpb.bitmask);

	return cnt;
}

/***************************************************************************/
PRIVATE void up_newfat_bad(short cnt)
{
short i, nc;

	for (i = 0; i < cnt - 1; i++) {
		nc = nextcluster;
		while (fat16[++nextcluster] == dpb.badmask)
			;
		newfat[nc] = nextcluster;
	}
	newfat[nextcluster] = dpb.bitmask;
	while (fat16[++nextcluster] == dpb.badmask)
		;
}

/*
 *	sort directory into the following order:
 *		all subdirectory entries
 *		file entries (we preserve the existing entry order)
 *
 *	we also (optionally) remove deleted entries
 *
 *	returns pointer to last entry + 1
 */
char *sort_dir(char *curdir,int ncl,int rmdel)
{
char temp[sizeof(DIRENTRY)];
char *curpoint;
char *inspoint;
char *endpoint;

	inspoint = curdir + 2*sizeof(DIRENTRY);		/* initial insertion point after . and .. */

	for (endpoint = curdir+dpb.bpc*ncl-sizeof(DIRENTRY); endpoint >= inspoint; endpoint -= sizeof(DIRENTRY))
		if (*endpoint != 0x00)
			break;

	endpoint += sizeof(DIRENTRY);				/* endpoint is last entry used + 1 */

	/*
	 *	if removing deleted entries, remove any initial ones
	 */
	if (rmdel) {
		while(*inspoint == DELETION_MARK) {
			memmove(inspoint,inspoint+sizeof(DIRENTRY),endpoint-inspoint-sizeof(DIRENTRY));
			endpoint -= sizeof(DIRENTRY);
			memset(endpoint,0x00,sizeof(DIRENTRY));
		}
	}

	/*
	 *	move insertion point to first NON-subdirectory entry
	 */
	while(*inspoint && (fattr(inspoint)&FA_SUBDIR))
		inspoint += sizeof(DIRENTRY);

	/*
	 *	scan through remainder of directory:
	 *	. if not a deleted entry:
	 *		. if a subdirectory entry, move directory entries up 1 & insert it
	 *		. else just skip it
	 *	. else it's a deleted entry:
	 *		. if removing deleted entries, move directory entries down 1
	 *		. else just skip it
	 */
	for (curpoint = inspoint + sizeof(DIRENTRY); curpoint < endpoint; ) {
		if (*curpoint != DELETION_MARK) {
			if (*curpoint && (fattr(curpoint)&FA_SUBDIR)) {
				memcpy(temp,curpoint,sizeof(DIRENTRY));
				memmove(inspoint+sizeof(DIRENTRY),inspoint,curpoint-inspoint);
				memcpy(inspoint,temp,sizeof(DIRENTRY));
				inspoint += sizeof(DIRENTRY);
			}
			curpoint += sizeof(DIRENTRY);
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

	/*
	 *	now scan directory and remove entries with zeroes in the first 16 bytes !?
	 *
	 *	this is bogus, it shouldn't be possible as far as I can see
	 */
	curpoint = curdir;
	memset(temp,0x00,sizeof(DIRENTRY));
	while (curpoint < endpoint) {
		if (!memcmp(curpoint,temp,16)) {					//MAGNUM
			memmove(curpoint,curpoint+sizeof(DIRENTRY),endpoint-curpoint-sizeof(DIRENTRY));
			endpoint -= sizeof(DIRENTRY);
			memset(endpoint,0x00,sizeof(DIRENTRY));
			assert(0);										//rfb
		} else {
			curpoint += sizeof(DIRENTRY);
		}
	}

	return endpoint;
}

/*
 *	sort root directory into the following order:
 *		volume label (if present)
 *		AUTO folder entry (if present)
 *		all other subdirectory entries
 *		file entries (we preserve the existing entry order)
 *
 *	we remove deleted entries iff 'rmdel' is non-zero
 *
 *	returns pointer to last entry + 1
 */
char *sort_root(char *dirbuf,int rmdel)
{
char temp[sizeof(DIRENTRY)];
char *curpoint;
char *inspoint;
char *endpoint;
int volume_labels_allowed;

	inspoint = dirbuf;	/* no . or .. in root */

	for (endpoint = dirbuf+dpb.rdb-sizeof(DIRENTRY); endpoint >= inspoint; endpoint -= sizeof(DIRENTRY))
		if (*endpoint != 0x00)
			break;

	endpoint += sizeof(DIRENTRY);						/* endpoint is last entry used + 1 */

	/*
	 *	we now scan the entire directory, deleting any extra volume labels
	 *	(early TOS versions didn't check for an existing vol label when creating one)
	 */
	volume_labels_allowed = 1;

	for (curpoint = inspoint; curpoint < endpoint; curpoint += sizeof(DIRENTRY)) {
		if (*curpoint && (fattr(curpoint)&FA_VOLUME)) {
			if (volume_labels_allowed)
				volume_labels_allowed--;
			else *curpoint = DELETION_MARK;
		}
	}

	if (*inspoint && (fattr(inspoint)&FA_VOLUME))		/* skip any initial volume label */
		inspoint += sizeof(DIRENTRY);

	/*
	 *	if removing deleted entries, remove any initial ones
	 */
	if (rmdel) {
		while (*inspoint == DELETION_MARK) {
			memmove(inspoint,inspoint+sizeof(DIRENTRY),endpoint-inspoint-sizeof(DIRENTRY));
			endpoint -= sizeof(DIRENTRY);
			memset(endpoint,0x00,sizeof(DIRENTRY));
		}
	}

	/*
	 *	move insertion point to first NON-subdirectory entry
	 */
	while(*inspoint && (fattr(inspoint)&FA_SUBDIR))
		inspoint += sizeof(DIRENTRY);

	/*
	 *	scan through remainder of directory:
	 *	. if not a deleted entry:
	 *		. if a subdirectory entry, move directory entries up 1 & insert it
	 *		. else just skip it
	 *	. else it's a deleted entry:
	 *		. if removing deleted entries, move directory entries down 1
	 *		. else just skip it
	 */
	curpoint = inspoint + sizeof(DIRENTRY);
	while (curpoint < endpoint) {
		if (*curpoint != DELETION_MARK) {
			if (*curpoint && (fattr(curpoint)&FA_SUBDIR)) {
				memcpy(temp,curpoint,sizeof(DIRENTRY));
				memmove(inspoint+sizeof(DIRENTRY),inspoint,curpoint-inspoint);
				memcpy(inspoint,temp,sizeof(DIRENTRY));
				inspoint += sizeof(DIRENTRY);
			}
			curpoint += sizeof(DIRENTRY);
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

	/*
	 *	now scan directory and remove entries with zeroes in the first 16 bytes !?
	 *
	 *	this is bogus, it shouldn't be possible as far as I can see
	 */
	curpoint = dirbuf;
	memset(temp,0x00,sizeof(DIRENTRY));
	while (curpoint < endpoint) {
		if (!memcmp(curpoint,temp,16)) {					//MAGNUM
			memmove(curpoint,curpoint+sizeof(DIRENTRY),endpoint-curpoint-sizeof(DIRENTRY));
			endpoint -= sizeof(DIRENTRY);
			memset(endpoint,0x00,sizeof(DIRENTRY));
			assert(0);										//rfb
		} else {
			curpoint += sizeof(DIRENTRY);
		}
	}

	/*
	 *	finally, we ensure any folder named AUTO comes immediately
	 *	after the volume label (if present)
	 */
	inspoint = dirbuf;
	if (*inspoint && (fattr(inspoint)&FA_VOLUME))
		inspoint += sizeof(DIRENTRY);
	curpoint = inspoint + sizeof(DIRENTRY);
	while (*curpoint && (fattr(curpoint)&FA_SUBDIR)) {
		if (strncmp(curpoint,"AUTO       ",NAME_LENGTH) == 0) {			//MAGNUM
			memcpy(temp,curpoint,sizeof(DIRENTRY));
			memmove(inspoint+sizeof(DIRENTRY),inspoint,curpoint-inspoint);
			memcpy(inspoint,temp,sizeof(DIRENTRY));
			break;
		}
		curpoint += sizeof(DIRENTRY);
	}

	return endpoint;
}

/*************************************************************************/

/*
 *	creates index array
 *
 *	first, fills indx[1..n] with values 0..n-1
 *	then sorts indx[1..n] so that arrin[indx[n]]<arrin[indx[arrin[n+1]]
 *
 *	this is used e.g. to collect fragmented clusters when reading so that
 *	they can be written out consecutively
 *
 */
PRIVATE void a_index(short n,short arrin[],short indx[])
{
short l, j, i;
short ir, indxt;
long q;

	if (n < 2) {		/* handle degenerate case */
		indx[1] = 0;
		return;
	}

	for (j = 1; j <= n; j++)
		indx[j] = j - 1;

	l = (n >> 1) + 1;
	ir = n;
	for ( ; ; ) {
		if (l > 1)
			q = arrin[(indxt = indx[--l])];
		else {
			q = arrin[(indxt = indx[ir])];
			indx[ir] = indx[1];
			if (--ir == 1) {
				indx[1] = indxt;
				return;
			}
		}
		i = l;
		j = l << 1;
		while (j <= ir) {
			if ((j < ir) && (arrin[indx[j]]) < (arrin[indx[j + 1]]))
				j++;
			if (q < arrin[indx[j]]) {
				indx[i] = indx[j];
				j += (i = j);
			} else
				j = ir + 1;
		}
		indx[i] = indxt;
	}
}

/************************************************************************/
/* the following routines manipulate the optimisation bar display		*/
/************************************************************************/
PRIVATE void init_obar(long max)
{
	obarmax = max;
	objc_offset(obar,OBARBACK,&obarx,&obary);
	ototbar = obar[OBARBACK].ob_width;
}

PRIVATE void start_obar(char *str1)
{
short xdial, ydial, wdial, hdial;

	strcpy(obar[OPTMSG].ob_spec, str1);
	obar[OBARTOP].ob_width = 0;
	xdial = obar->ob_x - 3*rez;
	ydial = obar->ob_y - 3*rez;
	hdial = obar->ob_height + 6*rez;
	wdial = obar->ob_width + 6*rez;
	objc_draw(obar, 0, MAX_DEPTH, xdial, ydial, wdial, hdial);
}

PRIVATE void update_obar(long num)
{
short old, new;

	if (num > obarmax) {
		debug_alert("[1][Opt progress bar > 100%|Please report to Anodyne!][ Fix ]");
		num = obarmax;
	}
	old = obar[OBARTOP].ob_width;
	new = (ototbar * num + obarmax/2) / obarmax;
	if (new > old+2) {
		obar[OBARTOP].ob_width = new;
		objc_draw(obar,OBARTOP,MAX_DEPTH,obarx+old,obary-3*rez,new-old+3,obar[OBARTOP].ob_height+6*rez);
	}
}

PRIVATE void update_obarmsg(char *strng)
{
	strcpy(obar[OPTMSG].ob_spec,strng);
	redraw_object(obar,OPTMSG);
}

PRIVATE void full_obar()
{
short old;

	old = obar[OBARTOP].ob_width;
	obar[OBARTOP].ob_width = ototbar;
	objc_draw(obar,OBARTOP,MAX_DEPTH,obarx+old,obary-3*rez,ototbar-old+3,obar[OBARTOP].ob_height+6*rez);
}

PRIVATE void wrap_obar(void)
{
	refresh_now(obar,0);
}
