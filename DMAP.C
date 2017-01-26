/*
 *	dmap.c
 *
 *	performs the following major functions:
 *		diskinfo: display basic disk statistics
 *		diskmap:  display disk fragmentation map
 *		utility:  zeros partition / wipes partition
 *
 *	version 2.12 (november/2002)	roger burrows
 *		merge frag.c into this module
 *
 *	version 2.53 (july/2003)		roger burrows
 *		1. modify Zero/Wipe per Bela's suggestion:
 *			a) for Zero/Wipe, preserve partition label (conditional
 *			   compilation at this time, future option)
 *			b) for Wipe, use 0x00 rather than 0xe5 as fill
 *		2. replace call to max_allocatable_clusters() with call to
 *		   allocate_max_units().  see misc.c for explanation/comments.
 *		3. eliminate mapx/mapy (same as gmap.g_x/gmap.g_y)
 *
 *	version 2.54 (august/2003)		roger burrows
 *		. as per 1(a) above, optionally preserve partition label
 *		  (option must be set manually in INF file at this time)
 *		. add DEMO support for Zero & Wipe (Unzero is done in info.c)
 *
 *	version 2.55 (june/2007)		roger burrows
 *		. fixed bug: the search for the partition label, introduced
 *		  in v2.53, went past the end of the in-memory copy of the
 *		  root directory
 *
 *	version 2.56 (august/2016)		roger burrows
 *		. fix bug: if 'bigbad' is set (by frag_loop()), the return code
 *		  from do_diskmap() is now forced to -1 to indicate an error
 *		. fix bug: process_dir() no longer assumes that there will be
 *		  at least one never-used entry in the root directory to mark
 *		  end-of-directory; previously the code could run off the end
 *		  and process garbage entries, usually causing an erroneous alert
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <aes.h>
#include <assert.h>
#include <dos.h>
#include <limits.h>
#include <portab.h>
#include <string.h>
#include <vdi.h>

#include "edge.h"
#include "edgedefs.h"
#include "miscdef.h"
#include "mapi.h"
#include "proto.h"

#define MODULE_NUMBER	3		/* used by error() macro */

extern short dev, win1, windhidden, nplanes, indcolor;
extern short blockheight;
extern short myhandle;
extern MYBPB dpb;
extern short dirlevel;
extern DIRLIST dirlist[MAX_DIR_DEPTH];
extern FILEINFO finfo;
extern OPT opt;
extern STATUS status;
extern GRECT gmap;				/* for cluster map */
extern OBJECT *dinfo, *dmap, *text;


/*
 *	private variables
 */
MLOCAL long sizelimits[FRAGBUCKETS] = { 2000L, 5000L, 10000L, 20000L, 50000L, 100000L, LONG_MAX };
/*
 *	the following array is used to pick the shaded box that corresponds
 *	to the number of fragments/file for a particular range of filesizes.
 *	note that values are actually in 'millifragments', i.e. scaled by 1000
 */
MLOCAL long scale2[] = { 1000, 2000, 3000, 4000, LONG_MAX };
MLOCAL FRAGINFO frag;	/* for communication with frag.c */
/*
 *	the following points to an bitmap that tracks the status of the
 *	blocks in the fragmentation map display.  there are two bits per
 *	block with the following meanings:
 *		bit 0	0 = block is undrawn, 1 = block has been drawn
 *		bit 1	0 = block is 'ok', 1 = block is 'in error'
 */
MLOCAL char *block_bitmap = NULL;

MLOCAL int bigbad;		/* set if severe directory error occurs during diskmap process */


/*
 *	private macros
 */
#define cluster_to_block(cluster)	((dpb.maxc<=MAP_BLOCKS)?cluster:cluster*MAP_BLOCKS/dpb.maxc)
#define getbbm(block)				((block_bitmap[(block)/4]>>(((block)&0x03)*2))&0x03)
#define setbbm(block,n)				(block_bitmap[(block)/4] |= ((n)&0x03)<<(((block)&0x03)*2))


/*
 *	private functions
 */
PRIVATE int diskinfo(void);
PRIVATE int diskmap(void);
PRIVATE int frag_loop(unsigned short *f16,short clus,FRAGINFO *frag);
PRIVATE void free_dir(DIRLIST *dir);
PRIVATE int get_fileinfo(DIRLIST *source);
PRIVATE int load_dir(unsigned short *f16,short clus);
PRIVATE int process_dir(unsigned short *f16,short clus,FRAGINFO *frag);
PRIVATE int zero_data(void);


/*
 *	perfom the DISKINFO function
 */
PUBLIC int do_diskinfo()
{
	change_mode(DISKINFO);

	if (dev != status.infodev) {		/* need to get new info */
		if (get_mybpb(&dpb,dev) < 0) {
			xform_alert(1,GEN_BPB_ALERT);
			return -1;
		} else if (diskinfo() < 0) {
			xform_alert(1,GEN_OPFAIL_ALERT);
			return -1;
		}
	}

	if (!windhidden)
		objc_draw(dinfo,0,MAX_DEPTH,dinfo->ob_x,dinfo->ob_y,dinfo->ob_width,dinfo->ob_height);

	status.infodev = dev;
	return 0;
}

PRIVATE int diskinfo()
{
int free_clus, i;
long bfree, tbyte, used;
unsigned short *f16;
char *f12;

	if (dpb.isfat16) {							/* fat16 */
		if ((f16=malloc(dpb.bpf)) == NULL) {
			error(dpb.bpf,MEMALLOC,"");
			return -1;
		}
		if (log_read(dev,dpb.spf,1,f16,FAT)) {
			free(f16);
			return -1;
		}
	} else {									/* fat12 */
		if ((f16=malloc(dpb.bpf*3L)) == NULL) {	/* allow for fat12->fat16 expansion */
			error(dpb.bpf*3,MEMALLOC,"");
			return -1;
		}
		f12 = ((char *)f16) + dpb.bpf*2L;
		if (log_read(dev,dpb.spf,1,f12,FAT)) {
			free(f16);
			return -1;
		}
		fat12_to_fat16(f12,f16);
	}

	for (i = 2, free_clus = 0; i < dpb.maxc; i++)
		if (f16[i] == 0)
			free_clus++;

	tbyte = dpb.tbyte;
	bfree = (long)free_clus * dpb.spc * dpb.bps;
	used = tbyte - bfree;

	sprintf(dinfo[MAPBPS].ob_spec,"%5d",dpb.bps);
	sprintf(dinfo[MAPSPC].ob_spec,"%2d",dpb.spc);
	sprintf(dinfo[MAPBPC].ob_spec,"%5ld",dpb.bpc);
	sprintf(dinfo[MAPSPT].ob_spec,"%3d",dpb.spt);
	sprintf(dinfo[MAPTPC].ob_spec,"%3d",dpb.tpc);
	sprintf(dinfo[MAPTSEC].ob_spec,"%7ld",dpb.tsec);
	sprintf(dinfo[MAPCSEC].ob_spec,"%5d",dpb.firstd - 1);
	sprintf(dinfo[MAPTCLUS].ob_spec,"%6ld",dpb.tsec/dpb.spc);
	sprintf(dinfo[MAPDCLUS].ob_spec,"%6d",dpb.cdata);
	sprintf(dinfo[MAPTBYTE].ob_spec,"%10ld",dpb.tbyte);

	sprintf(dinfo[MAPFBYTE].ob_spec,"%10ld",bfree);
	sprintf(dinfo[MAPUBYTE].ob_spec,"%10ld",used);

	sprintf(dinfo[MAPSPF].ob_spec,"%3d",dpb.spf);
	sprintf(dinfo[MAPNFAT].ob_spec,"%1d",dpb.nfat);
	sprintf(dinfo[MAPFATE].ob_spec,"%6s",dpb.isfat16?"16 bit":"12 bit");
	sprintf(dinfo[MAPFAT2].ob_spec,"%4d",1+dpb.spf);
	sprintf(dinfo[MAPNDIRS].ob_spec,"%4d",dpb.rds);
	sprintf(dinfo[MAPNDIR].ob_spec,"%4d",dpb.ndir);
	sprintf(dinfo[MAPROOTS].ob_spec,"%6d",1+2*dpb.spf);
	sprintf(dinfo[MAPDATAS].ob_spec,"%6d",dpb.firstd);

	if (tbyte > LONG_MAX/1024) {	/* avoid overflow below */
		used /= 1024;
		tbyte /= 1024;
	}
	dinfo[MAPFBAR].ob_width = dinfo[MAPFBAR-1].ob_width * used / tbyte;

	free(f16);
	return 0;
}

/*
 *	the following routine is called:
 *		. whenever we switch away from DISKINFO
 *		. at termination
 *
 *	for now it does nothing
 */
PUBLIC void cleanup_diskinfo()
{
}

/*
 *	perform the DISKMAP function
 */
PUBLIC int do_diskmap(void)
{
int rc;

	bigbad = 0;

	change_mode(DISKMAP);
	if (dev == status.mapdev) {
		objc_draw(dmap,0,MAX_DEPTH,dmap->ob_x,dmap->ob_y,dmap->ob_width,dmap->ob_height);
		restore_map(gmap);
		return 0;
	}

	if (get_mybpb(&dpb,dev) < 0) {
		xform_alert(1,GEN_BPB_ALERT);
		return -1;
	}

	wind_update(BEG_UPDATE);
	mouse_off();
	rc = diskmap();
	mouse_on();
	wind_update(END_UPDATE);
	if (bigbad) {
		xform_alert(1,DM_DISKERR_ALERT);
		rc = -1;
	}

	if (rc == 0) {
		save_map();
		status.mapdev = dev;
	}

	return rc;
}

PRIVATE int diskmap()
{
static short ref_y = 0;
static short ref_h = 0;

short ref_xpix = 0;
short ref_ypix = 0;
short ref_wpix = 0;
short ref_hpix = 0;
int i, j;
int n, phigh, plow;
int percent[FRAGBUCKETS], maxpercent, scalepercent;
TEDINFO *ted;
unsigned short *f16;
char *f12;

	vsf_interior(myhandle,FIS_PATTERN);
	vsf_style(myhandle,8);
	vsf_color(myhandle,GREEN);
	if (nplanes == 1) {
		vsf_color(myhandle,BLACK);
		for (i = 0; i < FRAGBUCKETS; i++)
			dmap[BAR1+i].ob_spec = WHITEBOX;
	}

	if (!ref_y) {
		ref_y = dmap[BAR1].ob_y;
		ref_h = dmap[BAR1].ob_height;
	}

	objc_offset(dmap,S4,&ref_xpix,&ref_ypix);
	ref_hpix = dmap[S0].ob_y + dmap[S4].ob_y + dmap[S0].ob_height;
	ref_wpix = dmap[BAR7].ob_x - dmap[S4].ob_x + dmap[BAR7].ob_width + 2;

	if (dpb.isfat16) {							/* fat16 */
		if ((f16=malloc(dpb.bpf)) == NULL) {
			error(dpb.bpf,MEMALLOC,"");
			return -1;
		}
		if (log_read(dev,dpb.spf,1,f16,FAT)) {
			free(f16);
			return -1;
		}
		swab(f16,f16,dpb.bpf);
	} else {									/* fat12 */
		if ((f16=malloc(dpb.bpf*3L)) == NULL) {	/* allow for fat12->fat16 expansion */
			error(dpb.bpf*3,MEMALLOC,"");
			return -1;
		}
		f12 = ((char *)f16) + dpb.bpf*2L;
		if (log_read(dev,dpb.spf,1,f12,FAT)) {
			free(f16);
			return -1;
		}
		fat12_to_fat16(f12,f16);
	}

	ted = (TEDINFO *) (dmap + FMAPBPC)->ob_spec;
	format_cpb(ted->te_ptext);					/* display clusters/block as m.n */

	sprintf(dmap[FOLDER].ob_spec,"%4d",0);
	sprintf(dmap[FILES].ob_spec,"%5d",0);
	sprintf(dmap[FRAG].ob_spec,"%5d",0);
	sprintf(dmap[FRAGPFF].ob_spec,"%3d.%03d",0,0);
	sprintf(dmap[FRAG100].ob_spec,"%3d.%03d",0,0);
	for (i = 0; i < FRAGBUCKETS; i++) {
		dmap[BAR1+i].ob_height = 1;
		dmap[BAR1+i].ob_y = ref_y + ref_h;
		dmap[BAR1+i].ob_spec = (void *)B0;	/* BOX */
	}

	ted = (TEDINFO *) (dmap+S4)->ob_spec;
	strcpy(ted->te_ptext,"100");
	ted = (TEDINFO *) (dmap+S3)->ob_spec;
	strcpy(ted->te_ptext,"75");
	ted = (TEDINFO *) (dmap+S2)->ob_spec;
	strcpy(ted->te_ptext,"50");
	ted = (TEDINFO *) (dmap+S1)->ob_spec;
	strcpy(ted->te_ptext,"25");

	objc_draw(dmap,0,MAX_DEPTH,dmap->ob_x,dmap->ob_y,dmap->ob_width,dmap->ob_height);

//	objc_offset(dmap,MAPAREA,&mapx,&mapy);		//rfb: why do we need this? isn't this the same as gmap.g_x, g_y?
//	assert((mapx==gmap.g_x)&&(mapy==gmap.g_y));	//rfb: test theory
	dirlevel = 0;
	memset(&dirlist[0],0x00,MAX_DIR_DEPTH*sizeof(DIRLIST));

	frag.totfolders = frag.totfiles = frag.totfragfiles = frag.totfragments = frag.totfragsize = 0L;
	for (i = 0; i < FRAGBUCKETS; i++)
		frag.bin[i].files = frag.bin[i].fragfiles = frag.bin[i].fragments = 0L;

	set_clip();
	draw_block(0,CLUSTER_OK);
	draw_block(1,CLUSTER_OK);

	if (frag_loop(f16,0,&frag) > 0) {	/* exit immediately if insufficient memory */
		free(f16);
		return -1;
	}

	/*
	 *	determine %age of files in each bucket
	 */
	for (i = 0; i < FRAGBUCKETS; i++)
		percent[i] = frag.totfiles ? (frag.bin[i].files*100+frag.totfiles/2)/frag.totfiles : 0;

	/*
	 *	determine the biggest percentage in any bucket
	 */
	maxpercent = -1;
	for (i = 0; i < FRAGBUCKETS; i++)
		if (percent[i] > maxpercent)
			maxpercent = percent[i];

	/*
	 *	choose the first percentage (of 20,40,60,80,100) higher than
	 * 'maxpercent' as the max y-axis value
	 */
	scalepercent = ((max(maxpercent,1)+19) / 20) * 20;

	for (i = 0; i < FRAGBUCKETS; i++) {
		dmap[BAR1+i].ob_height = ref_h * percent[i] / scalepercent;
		dmap[BAR1+i].ob_y = ref_y + ref_h - dmap[BAR1+i].ob_height;
		if (frag.bin[i].fragfiles)
			n = frag.bin[i].fragments * 1000 / frag.bin[i].fragfiles;
		else n = 0;
		for (j = 0; n > scale2[j]; j++)
			;
		switch(j) {
		case 0:
			dmap[BAR1+i].ob_spec = dmap[B0].ob_spec;
			break;
		case 1:
			dmap[BAR1+i].ob_spec = dmap[B1].ob_spec;
			break;
		case 2:
			dmap[BAR1+i].ob_spec = dmap[B2].ob_spec;
			break;
		case 3:
			dmap[BAR1+i].ob_spec = dmap[B3].ob_spec;
			break;
		case 4:
			dmap[BAR1+i].ob_spec = dmap[B4].ob_spec;
			break;
		}
	}

	sprintf(dmap[FOLDER].ob_spec,"%4d",frag.totfolders);
	sprintf(dmap[FILES].ob_spec,"%5d",frag.totfiles);
	sprintf(dmap[FRAG].ob_spec,"%5d",frag.totfragfiles);

	if (frag.totfragfiles == 0) {
		phigh = plow = 0;
	} else {
		phigh = frag.totfragments / frag.totfragfiles;
		plow = (frag.totfragments-phigh*frag.totfragfiles) * 1000 / frag.totfragfiles;
	}
	sprintf(dmap[FRAGPFF].ob_spec,"%3d.%03d",phigh,plow);

	if (frag.totfragsize == 0) {
		phigh = plow = 0;
	} else {
		frag.totfragments *= 100000;		/* scale, assume less than about 21000 fragments (!) */
		phigh = frag.totfragments / frag.totfragsize;
		n = frag.totfragments - phigh * frag.totfragsize;
		if (n > LONG_MAX/1000)
			plow = n / (frag.totfragsize/1000);
		else plow = n * 1000 / frag.totfragsize;
	}
	sprintf(dmap[FRAG100].ob_spec,"%3d.%03d",phigh,plow);

	ted = (TEDINFO *) (dmap+S4)->ob_spec;
	sprintf(ted->te_ptext,"%3d",scalepercent);
	ted = (TEDINFO *) (dmap+S3)->ob_spec;
	sprintf(ted->te_ptext,"%2d",3*scalepercent/4);
	ted = (TEDINFO *) (dmap+S2)->ob_spec;
	sprintf(ted->te_ptext,"%2d",scalepercent/2);
	ted = (TEDINFO *) (dmap+S1)->ob_spec;
	sprintf(ted->te_ptext,"%2d",scalepercent/4);

	redraw_object(dmap,FSTAT);
	objc_draw(dmap,0,MAX_DEPTH,ref_xpix,ref_ypix,ref_wpix,ref_hpix);

	free(f16);
	return 0;
}

/*
 *	frag_loop(): check all files in directory for fragmentation
 *	returns:	0	check completed OK
 *				1	insufficient memory
 *				-1	serious directory error (also sets 'bigbad')
 */
PRIVATE int frag_loop(unsigned short *f16,short clus,FRAGINFO *frag)
{
int rc;

	if ((rc=load_dir(f16,clus)) == 0)	/* both -1 and 1 are errors */
		rc = process_dir(f16,clus,frag);

	free_dir(&dirlist[dirlevel]);
	dirlevel--;

	if (rc < 0)
		bigbad = 1;

	return rc;
}

/*
 *	read in directory
 *		returns:	0	all OK
 *					1	insufficient memory
 *					-1	error doing read, or invalid cluster
 */
PRIVATE int load_dir(unsigned short *f16,short clus)
{
DIRLIST *curdir;
short st;
unsigned short ls;

	st = clus;
	if (st) {
		if (!valid_cluster(st))
			return -1;
		curdir = &dirlist[dirlevel];
		curdir->offset = 0;
		curdir->firstcluster = st;
		curdir->xtradir = NULL;
		if ((dirlist[dirlevel].dirbuf=calloc(1,dpb.bpc)) == NULL) {
			error(dpb.bpc,MEMALLOC,"");
			return 1;
		}
		while (TRUE) {
			ls = logsector(st);
			if (log_read(dev,dpb.spc,ls,curdir->dirbuf,DIR))
				return -1;

			st = f16[st];
			if (st == dpb.bitmask)
				break;

			if (!valid_cluster(st))
				return -1;

			if ((curdir->xtradir=calloc(1,sizeof(DIRLIST))) == NULL) {
				error(sizeof(DIRLIST),MEMALLOC,"");
				return 1;
			}
			curdir = curdir->xtradir;
			curdir->offset = 0;
			curdir->firstcluster = st;
			curdir->xtradir = NULL;
			if ((curdir->dirbuf=calloc(1,dpb.bpc)) == NULL) {
				error(dpb.bpc,MEMALLOC,"");
				return 1;
			}
		}
		return 0;
	}

	if (dirlevel != 0)	/* cluster = 0 only allowed for root! */
		return -1;

	/*
	 *	handle level 0 directory (root, I think)
	 */
	dirlist[0].offset = 0;
	dirlist[0].xtradir = NULL;
	if ((dirlist[0].dirbuf=malloc(dpb.rdb)) == NULL) {
		error(dpb.rdb,MEMALLOC,"");
		return 1;
	}
	if (log_read(dev,dpb.rds,1+2*dpb.spf,dirlist[0].dirbuf,DIR))
		return -1;

	return 0;
}

/*
 *	process the directory loaded by load_dir()
 *		return -1 iff a severe directory error is detected
 */
PRIVATE int process_dir(unsigned short *f16,short clus,FRAGINFO *frag)
{
int numclus, rc = 0;
DIRLIST *curdir, *next;
short prev, st, numfrags, sz, fill;

	for (curdir = &dirlist[dirlevel]; curdir; curdir = next) {
		next = curdir->xtradir;
		draw_block(curdir->firstcluster,CLUSTER_OK);
	}
	curdir = &dirlist[dirlevel];

	while(get_fileinfo(curdir)) {
		curdir->offset += sizeof(DIRENTRY);
		if (dirlevel > 0) {
			if (curdir->offset == dpb.bpc)		/* chain to next directory cluster for next time */
				curdir = curdir->xtradir;
		} else {
			if (curdir->offset == dpb.rdb)		/* end of root dir */
				curdir = NULL;
		}

		/*
		 *	ignore uninteresting entries
		 */
		if ((finfo.attr & FA_VOLUME) || (finfo.name[0] == DELETION_MARK) || (finfo.name[0] == '.'))
			continue;

		/*
		 *	process subdirectories
		 */
		if (finfo.attr & FA_SUBDIR) {
			dirlevel++;
			frag->totfolders++;
			if (frag_loop(f16,finfo.start,frag) > 0)
				break;
			continue;
		}

		/*
		 *	process files
		 *
		 *	first we add to counts (total, and by size)
		 */
		frag->totfiles++;
		for (sz = 0; finfo.size > sizelimits[sz]; sz++)	/* find right bucket */
			;
		frag->bin[sz].files++;

		if ((finfo.size == 0L) && (finfo.start == 0))	/* zero-length files can (will?) have starting cluster = 0 */
			continue;

		/*
		 *	check cluster chain for no loop and valid contents
		 */
		for (numclus = 0, st = finfo.start, numfrags = 0; numclus <= dpb.cdata; numclus++) {
			if (!valid_cluster(st)) {
				numclus = dpb.cdata + 1;			/* error indicator */
				break;
			}
			prev = st;
			st = f16[prev];
			if (st == dpb.bitmask)					/* end of chain */
				break;
			if (st != prev+1)						/* fragmented ? */
				if (f16[prev+1] != dpb.badmask)		/* only counts if NOT due to bad sector */
					numfrags++;
		}
		if (numclus > dpb.cdata) {
			rc = -1;
			continue;
		}

		/*
		 *	file is OK: add to frag counts if applicable
		 */
		if (numfrags) {					/* file is fragmented */
			frag->totfragfiles++;				/* total # fragmented files */
			frag->totfragments += numfrags;		/* total # fragments */
			frag->totfragsize += finfo.size;	/* accumulate total size of fragmented files */
			frag->bin[sz].fragfiles++;			/* count files by filesize */
			frag->bin[sz].fragments += numfrags;/* count fragments by filesize */
		}

		/*
		 *	fill in blocks corresponding to file clusters
		 */
		fill = numfrags?CLUSTER_ERR:CLUSTER_OK; /* set fill mode: CLUSTER_ERR = frag / CLUSTER_OK = filled */
		for (st = finfo.start; f16[st] != dpb.bitmask; st = f16[st])
			draw_block(st,fill);
		draw_block(st,fill);					/* don't forget final block! */
	}

	return rc;
}

/*
 *	extract file info from in-memory copy of directory
 */
PRIVATE int get_fileinfo(DIRLIST *source)
{
DIRENTRY dir;

	if (!source)
		return 0;

	memcpy(&dir,source->dirbuf+source->offset,sizeof(DIRENTRY));
	if (!get_name(&dir,finfo.name))
		return 0;

	finfo.attr = dir.attr;
	finfo.time = get_word((char *)&dir.time);
	finfo.date = get_word((char *)&dir.date);
	finfo.start = get_word((char *)&dir.start);
	finfo.size = get_long((char *)&dir.size);

	return 1;
}

/*
 *	free malloc'd DIRLIST structures
 */
PRIVATE void free_dir(DIRLIST *dir)
{
char firsttime;
DIRLIST *temp;

	for (firsttime = TRUE; dir; dir = temp, firsttime = FALSE) {
		free(dir->dirbuf);
		temp = (DIRLIST *)dir->xtradir;
		if (firsttime)
			memset(dir,0x00,sizeof(DIRLIST));
		else free(dir);
	}
}

/*
 *	the following routine is called:
 *		. whenever we switch away from DISKMAP
 *		. at the end of do_bad(), do_tdisk()
 *		. at termination
 */
PUBLIC void cleanup_diskmap()
{
	if (block_bitmap)
		free(block_bitmap);
	block_bitmap = NULL;
}

/*
 *	draw a block representing one or more clusters
 *
 *	the input 'mode' defines the colour needed for the block
 */
PUBLIC void draw_block(short cluster,short mode)
{
long n;
int block, row;
short xoff, yoff;
short rect[4];

	if (cluster == 0) {					/* start of pass */
		n = (MAP_BLOCKS+3) / 4;
		if (!block_bitmap)
			block_bitmap = malloc(n);
		if (block_bitmap)
			memset(block_bitmap,0,n);
		else error(n,MEMALLOC,"");
	}
	if (!block_bitmap)
		return;

	block = cluster_to_block(cluster);
	switch(getbbm(block)) {
	case 0x01:					/* already marked ok? */
		if (mode == CLUSTER_OK)	/* no change, nothing to do */
			return;
		break;
	case 0x03:					/* already marked in error? */
		return;					/* never anything to do */
	}

	/*
	 *	not marked, or marked as ok but we have an error:
	 *	need to mark the block!
	 */
	row = block / MAP_BLKS_PER_ROW;
	yoff = row * blockheight;
	xoff = (block - row * MAP_BLKS_PER_ROW) * 2;
	rect[0] = gmap.g_x + xoff;
	rect[1] = gmap.g_y + yoff;
	rect[2] = rect[0] + 1;
	rect[3] = rect[1] + blockheight - 1;

	if (mode == CLUSTER_OK) {
		setbbm(block,0x01);				/* remember we had a 'good' cluster */
		vsf_color(myhandle,GREEN);
		vr_recfl(myhandle,rect);
	} else {
		setbbm(block,0x03);				/* remember we had a 'bad' cluster */
		if (nplanes == 1)
			vsf_style(myhandle,4);
		vsf_color(myhandle,RED);
		vr_recfl(myhandle,rect);
		vsf_style(myhandle,8);
	}
}

/*************************************************************************/
PUBLIC void draw_opt(short cluster,short mode)
{
short block;
short xoff, yoff, row;
short rect[4];

	block = cluster_to_block(cluster);

	row = block / MAP_BLKS_PER_ROW;
	yoff = row * blockheight;
	xoff = (block - row * MAP_BLKS_PER_ROW) * 2;
	rect[0] = gmap.g_x + xoff;
	rect[1] = gmap.g_y + yoff;
	rect[2] = rect[0] + 1;
	rect[3] = rect[1] + blockheight - 1;

	switch(mode) {
	case 2:								/* written out, defragged */
		vsf_color(myhandle,GREEN);
		vr_recfl(myhandle,rect);
		break;
	case 1:								/* read into memory */
		vsf_color(myhandle,indcolor);
		vr_recfl(myhandle,rect);
		break;
	default:							/* fragmented */
		if (nplanes == 1)
			vsf_style(myhandle,4);
		vsf_color(myhandle,RED);
		vr_recfl(myhandle,rect);
		vsf_style(myhandle,8);
	}
}

/*
 *	convert the number of clusters that each block of the disk map
 *	represents, into a displayable string with one decimal place
 */
PUBLIC void format_cpb(char *text)
{
int phigh, plow;

	if (dpb.maxc <= MAP_BLOCKS) {
		phigh = 1;
		plow = 0;
	} else {
		phigh = dpb.maxc / MAP_BLOCKS;
		plow = ((dpb.maxc-phigh*MAP_BLOCKS)*10 + MAP_BLOCKS/2) / MAP_BLOCKS;
	}
	sprintf(text,"%d.%d",phigh,plow);
}

/*
 *	perform partition zeroing function
 *
 *	actually just zeroes FATs & root, after saving Unzero info ...
 */
PUBLIC void do_zero()
{
char path[MAX_PATHNAME];

#ifdef DEMO
	if (dev > FIRST_HD_DEVICE) {
		form_alert(1,DEMO_DRIVEC);
		return;
	}
#endif

	/*
	 *	make sure we don't save the Unzero info on the drive
	 *	that we're about to Zero ...
	 */
	if (opt.filepath[0] == dev+'A') {
		xform_alert(1,ZR_WRONG_DRIVE);
		while(TRUE) {
			if (get_path(path,opt.filepath,MIRROR_MASK,text[DINFO_FSEL_SAVE].ob_spec) == 0)
				return;										/* she gave up */
			if (path[0] != dev+'A')							/* good choice! */
				break;
			xform_alert(1,FI_INVPATH_ALERT);
		}
	} else strcpy(path,opt.filepath);

	if (save_info(dev,path) < 0)
		if (xform_alert(2,ZR_SAVE_FAILED) != 1)
			return;

	if (get_mybpb(&dpb,dev) < 0)
		xform_alert(1,GEN_BPB_ALERT);
	else if (zero_fats_and_root() < 0)
		xform_alert(1,GEN_OPFAIL_ALERT);

	_mediach(dev);
	force_update(dev);
}

/*
 *	undo a ZERO operation from a backup of the FATs & root
 */
PUBLIC void do_unzero()
{
	restore_disk_info(1);
}

/*
 *	perform partition wipe function
 *
 *	zeroes FATs & root, then overwrites ALL data!
 */
PUBLIC void do_wipe()
{
int rc;
char temp[MAX_DISPLAYSIZE];

#ifdef DEMO
	if (dev > FIRST_HD_DEVICE) {
		form_alert(1,DEMO_DRIVEC);
		return;
	}
#endif

	if (get_mybpb(&dpb,dev) < 0)
		xform_alert(1,GEN_BPB_ALERT);
	else if (zero_fats_and_root() < 0)
		xform_alert(1,GEN_OPFAIL_ALERT);
	else {
		sprintf(temp,text[PROG_WIPING].ob_spec,dev+'A');
		start_bar(temp);
		init_bar(dpb.maxc);
		rc = zero_data();
		wrap_bar();
		if (rc < 0)
			xform_alert(1,GEN_OPFAIL_ALERT);
	}

	_mediach(dev);
	force_update(dev);
}

/*
 *	overwrite partition FATs and root directory
 */
PUBLIC int zero_fats_and_root(void)
{
int i, rc = 0;
long nbytes;
char *fatrootbuf, *root;
unsigned short *f16;
char *p;
DIRENTRY label = { 0x00 };

	if (dpb.isfat16)
		nbytes = dpb.bpf * 2L + dpb.rdb;
	else nbytes = dpb.bpf * 3L + dpb.rdb;		/* allow space to expand 12-bit to 16-bit FAT (see below) */

	if ((fatrootbuf=malloc(nbytes)) == NULL) {
		error(nbytes,MEMALLOC,"");
		return -1;
	}

	if (log_read(dev,dpb.spf,1,fatrootbuf,FAT)) {
		free(fatrootbuf);
		return -1;
	}

	if (dpb.isfat16) {							/* fat16 */
		f16 = (unsigned short *)fatrootbuf;
		for (i = 2; i < dpb.maxc; i++)
			if (f16[i] != BADCLUST16_INTEL)		/* because we haven't swabbed */
				f16[i] = 0;
	} else {									/* fat12 */
		f16 = (unsigned short *)(fatrootbuf + dpb.bpf);
		fat12_to_fat16(fatrootbuf,f16);
		for (i = 2; i < dpb.maxc; i++)
			if (f16[i] != dpb.badmask)
				f16[i] = 0;
		fat16_to_fat12(f16,fatrootbuf);
	}
	fast_copy(fatrootbuf+dpb.bpf,fatrootbuf,dpb.bpf);	/* make second copy of FAT */

	root = fatrootbuf + (dpb.bpf * 2L);			/* point to start of root */

	if (opt.keeplabel) {
		if (log_read(dev,dpb.rds,1+2*dpb.spf,root,ROOTDIR)) {
			free(fatrootbuf);
			return -1;
		}
		for (p = root; p < root + dpb.rdb; p += sizeof(DIRENTRY)) {
			if (((DIRENTRY *)p)->attr == FA_VOLUME) {
				memcpy(&label,p,sizeof(DIRENTRY));
				break;							/* always use first label if > 1 */
			}
		}
	}

	memset(root,0x00,dpb.rdb);					/* ensure root is zeroed */

	if (opt.keeplabel && label.name[0])
		memcpy(root,&label,sizeof(DIRENTRY));	/* restore label if present */

	if (log_write(dev,2*dpb.spf+dpb.rds,1,fatrootbuf,BOOT))
		rc = -1;

	free(fatrootbuf);
	return rc;
}

/*
 *	overwrite all partition data
 */
PRIVATE int zero_data()
{
short nout, ndo, nclus;
char *readbuf = NULL;

	nclus = allocate_max_units(1,dpb.bpc,&readbuf);
	if (!nclus) {
		error(dpb.bpc,MEMALLOC,"");
		return -1;
	}

	memset(readbuf,0x00,nclus*dpb.bpc);		/* we used to use 0xe5 ... */

	for (nout = 2; nout < dpb.maxc; nout += ndo) {
		ndo = min(dpb.maxc-nout,nclus);
		if (log_write(dev,ndo*dpb.spc,logsector(nout),readbuf,HARDDISK)) {
			free(readbuf);
			return -1;
		}
		update_bar(nout+ndo);
	}

	ring_bell();
	ring_bell();

	free(readbuf);
	return 0;
}
