/*
 *	edgedefs.h
 *
 *
 *	Copyright (C) 2002-2017 Roger Burrows
 *
 *	This file is distributed under the GPL, version 2 or at your
 *	option any later version.  See DOC\LICENSE.TXT for details.
 */

/*
 *	the main include file for EDGE
 *
 *	includes various structures (& a few defines) that are used throughout EDGE
 */
#include <dos.h>	/* for FNSIZE */
#ifdef MALLOC_DEBUG
	#include "trmalloc\trmalloc.h"			/* malloc debugging ... */
#endif

#ifndef NDEBUG
#define debug_alert(x)	form_alert(1,x)
#else
#define debug_alert(x)	((void)0)
#endif

#ifdef DEMO
#define DEMO_UNAVAIL	"[1][Sorry, this option is not|available in the demo version][ OK ]"
#define DEMO_DRIVEC		"[1][Sorry, this function is only |available for drives A-C|in the demo version][ OK ]"
#endif

/*
 *	definitions for functions
 */
#define PUBLIC
#define PRIVATE	static

/*
 *	various and sundry filenames
 */
#ifdef DEMO
#define PROGRAM_FILE	"EDGEDEMO.PRG"
#define RSC_FILE		"EDGEDEMO.RSC"
#define PREFS_FILE		"EDGEDEMO.INF"		/* stores preferences for Edge */
#else
#define PROGRAM_FILE	"EDGE.PRG"
#define RSC_FILE		"EDGE.RSC"
#define PREFS_FILE		"EDGE.INF"			/* stores preferences for Edge */
#endif
#define IN_PROGRAM_FILE	"INSTALL.PRG"
#define DM_PROGRAM_FILE	"DMIRROR.PRG"
#define DM_PREFS_FILE	"DMIRROR.INF"		/* stores preferences for Diamond Mirror */
#define DM_PATH			"%c:\\AUTO\\"		/* where to find Diamond Mirror program/prefs */

#define FILE_HELP		"HELP\\FILE.HLP"	/* subpaths for help files */
#define MEDIC_HELP		"HELP\\MEDIC.HLP"
#define OPTIMIZE_HELP	"HELP\\OPTIMIZE.HLP"
#define UNDELETE_HELP	"HELP\\UNDELETE.HLP"
#define UTILITY_HELP	"HELP\\UTILITY.HLP"

#define MEDIC_FILE		"MEDIC_%c.ERR"		/* template for error listing file */
#define MIRROR_FILE		"MIRROR_%c.DAT"		/* template for mirror file */
#define MIRROR_MASK		"MIRROR_?.DAT"		/* file mask for above */
#define PART_FILE		"SCSI%d_%d.DAT"		/* template for partition backup file */
#define PART_MASK		"SCSI*.DAT"			/* file mask for above */
#define VALID_FILE		"DRIVE_%c.VAL"		/* template for validation file */
#define VALID_MASK		"DRIVE_?.VAL"		/* file mask for above */
#define VALIDERR_FILE	"VALID_%c.ERR"		/* template for validation error file */

#define LOSTCLUS_FILE	"LOST%04dDAT"		/* lost cluster filename: internal representation! */

/*
 *	some macros to standardise the 'location' part of error messages
 */
#define ERROR_LINENUM			(MODULE_NUMBER*1000+__LINE__%1000)
#define error(a,b,c)			permanent_error(a,b,c,ERROR_LINENUM)
#define must_read(a,b,c,d,e)	critical_read(a,b,c,d,e,ERROR_LINENUM)
#define must_write(a,b,c,d,e)	critical_write(a,b,c,d,e,ERROR_LINENUM)
#define log_read(a,b,c,d,e)		logical_read(a,b,c,d,e,ERROR_LINENUM)
#define log_write(a,b,c,d,e)	logical_write(a,b,c,d,e,ERROR_LINENUM)
#define file_read(a,b,c,d)		gemdos_read(a,b,c,d,ERROR_LINENUM)
#define file_write(a,b,c,d)		gemdos_write(a,b,c,d,ERROR_LINENUM)

/*
 *	boxes used in disk fragmentation map
 */
#define WHITEBOX			((void *)0x11100L)	/* border thickness = 1, text colour = 1, border colour = 1 */
												/* transparent, background colour, interior colour = 0 */
#define GREYBOX				((void *)0x11141L)	/* border thickness = 1, text colour = 1, border colour = 1 */
												/* transparent, dither pattern = 4, interior colour = 1 */
#define BLACKBOX			((void *)0x11171L)	/* border thickness = 1, text colour = 1, border colour = 1 */
												/* transparent, solid, interior colour = 1 */

/*
 *	manifest constants/macros
 */
#define CRIT_ERR_VEC		0x101			/* critical error handler vector */
#define _bootdev			((short *)0x446)/* boot device */
#define _nflops				((short *)0x4a6)/* number of floppies */

#define PHYSICAL_SECSIZE	512				/* size of disk sectors */
#define DELETION_MARK		0xe5			/* marks deleted files in directory */
#define PART_OFFSET			0x1c6			/* offset of first partition entry */
										/* special cluster numbers in FAT */
#define ENDCHAIN16			0xffff			/* 16-bit FAT: end of chain */
#define BADCLUST16			0xfff7				/* bad cluster */
#define BADCLUST16_INTEL	0xf7ff				/* bad cluster (unswab'd) */
#define ENDCHAIN12			0xfff			/* 12-bit FAT: end of chain */
#define BADCLUST12			0xff7				/* bad cluster */
										/* limits for FAT layout */
#define DEFAULT_NRES		1
#define DEFAULT_NFAT		2
#define MIN_SPC				1				/* sectors per cluster */
#define DEFAULT_SPC			2
#define MAX_SPC				2
#define MIN_RDE				112				/* root directory entries */
#define DEFAULT_RDE			256
#define MAX_RDE				1024
#define DEFAULT_MEDIA		0xf8			/* media type */
											/* logical sector size */
#define MIN_LOGSECSIZE		512					/* for everyone :-) */
#define MAX_LOGSECSIZE		8192				/* for ST, TT */
#define MAX_LOGSECSIZE_TOS4	16384				/* for Falcon, Magic */
#define MAX_FL_LOGSECSIZE	1024				/* for floppy: could probably be 512 ... */
#define MAX_HD_LOGSECSIZE	32768				/* for hard disk: could probably be 16384 ... */
											/* maximum number of logical sectors */
#define MAX_LOGSECS_TOS10_A	16383				/* TOS 1.00/1.02, Atari-formatted (sez Luneski) */
#define MAX_LOGSECS_TOS10	32766				/* ditto, not Atari-formatted */
#define MAX_LOGSECS_TOS14_A	32766				/* TOS 1.04 and later, Atari-formatted (sez Luneski) */
#define MAX_LOGSECS_TOS14	65532				/* ditto, not Atari-formatted */

										/* physical sector stuff */
											/* physical sectors per logical sector */
#define PHYS_PER_LOG_GEM		(MIN_LOGSECSIZE/PHYSICAL_SECSIZE)
#define PHYS_PER_LOG_BGM		(MAX_LOGSECSIZE/PHYSICAL_SECSIZE)
#define PHYS_PER_LOG_BGM_TOS4	(MAX_LOGSECSIZE_TOS4/PHYSICAL_SECSIZE)
											/* maximum number of physical sectors */
#define MAX_PHYS_SECS_GEM_TOS10	(MAX_LOGSECS_TOS10*PHYS_PER_LOG_GEM)
#define MAX_PHYS_SECS_GEM_TOS14	(MAX_LOGSECS_TOS14*PHYS_PER_LOG_GEM)
#define MAX_PHYS_SECS_BGM_TOS10	(MAX_LOGSECS_TOS10*PHYS_PER_LOG_BGM)
#define MAX_PHYS_SECS_BGM_TOS14	(MAX_LOGSECS_TOS14*PHYS_PER_LOG_BGM)
#define MAX_PHYS_SECS_BGM_TOS4	(MAX_LOGSECS_TOS14*PHYS_PER_LOG_BGM_TOS4)

#define NODE_LENGTH			8				/* the 'ABCDE' part of name ABCDE.FGH */
#define EXT_LENGTH			3				/* the 'FGH' part of the same */
#define NAME_LENGTH			(NODE_LENGTH+EXT_LENGTH)
									/* the following will need changing for LFN support */
#define MAX_FILENAME		13				/* max filename length */
#define MAX_PATHNAME		256				/* max pathname length */

									/* the following will need changing for MagiC/MiNT support */
#define NUM_DEVICES			16				/* max partition number */
#define FIRST_HD_DEVICE		2				/* C: */
#define NUM_HD_DEVICES		(NUM_DEVICES-FIRST_HD_DEVICE)

#define MAX_PARTS_ATARI		4				/* max partitions/disk in Atari mode */
#define MAX_PARTS_ICD		12				/* max partitions/disk in ICD/Supra mode */

#define MAX_ALERT_SIZE		200				/* max length of alert string (sez everyone) */
#define MAX_ALERTLINE_SIZE	30				/* max length of line in alert */

#define DATALITE_ID			"DL03"
#define DATALITE_ID_OFFSET	64				/* in sector 0 */
#define DATALITE_SEC_OFFSET	106				/* total sectors, unsigned short */

#define fattr(a)			(*(a+11))
#define form_feed()			(Cprnout(0x0c))
#define ring_bell()			(Cconout(0x07))
#define valid_cluster(clus)	(((clus)>=2)&&((clus)<dpb.maxc))
#define IS_COLOUR_AES		(_AESglobal[0] >= 0x330)
#define MAGX_COOKIE			0x4d616758L		/* MagX */


/*
 *	values that must match internal structures etc (take GREAT care when changing)
 */
#define MAX_WINDOW_WIDTH	640				/* max width of any window in RSC file */
#define MAX_DEVS			8				/* displayable in SELSCSI dialog */
										/* the following lengths include the terminating null */
#define MAX_ERRFILEMSGSIZE	(MAX_PATHNAME+40)	/* max string written to error file by medic */
												/* (allows for msg string + spaces) */
#define MAX_DISPLAYSIZE		80				/* max string to be displayed in window title/info/contents */
#define MAX_PRINTSIZE		80				/* max string to be printed */

#define DISPLAY_PARTITIONS	12				/* that we display at one time, see part1.c and rsc */

#define MAP_BLKS_PER_ROW	184				/* for disk fragmentation map */	//rfb: should this be dynamically determined from ob_width/2 ?
#define MAP_ROWS			54													//rfb: ob_height / 4 ??
#define MAP_BLOCKS			(MAP_BLKS_PER_ROW*MAP_ROWS)							//rfb: also dynamic?

#define MAPBADLIST_ROWS		8				/* for bad cluster/sector report */
#define MAPBADLIST_COLS		5

#define UNREGISTERED		"Unregistered"	/* must match string in "About" */

/*
 *	EDGE preferences file
 */
#define GENERAL_SECTION		"General"
	#define INF_NOVICE		"Novice"
	#define INF_WARN		"Always warn"
	#define INF_MULTI		"Multidevice"
	#define INF_FILEPATH	"File path"
#define OPTIMIZE_SECTION	"Optimize"
	#define INF_OPTFULL		"Full optimization"
	#define INF_OPTREAD		"Prioritize reading"
	#define INF_OPTRMDEL	"Remove deleted"
#define MEDIC_SECTION		"Medic"
	#define INF_MEDAUTOFIX	"Auto-fix errors"
	#define INF_MEDIGNORE	"Ignore illegal names"
	#define INF_MEDSAVE		"Save lost clusters"
	#define INF_MEDVISUAL	"Visual update"
	#define INF_MEDPAUSE	"Pause"
	#define INF_MEDNOHDR	"Ignore header"
#define UNDELETE_SECTION	"Undelete"
	#define INF_UNDMIRROR	"Mirror undelete"
	#define INF_UNDDATE		"Date format"
#define UTILITY_SECTION		"Utility"
	#define INF_UTILZERO	"Disable zeroing"
	#define INF_UTILKEEP	"Keep label"


/*
 *	DMIRROR preferences file
 */
#define TEST_SECTION		"Test"
	#define INF_PARTITIONS	"Partitions"	/* the first two also apply to MIRROR_SECTION */
	#define INF_FREQUENCY	"Frequency"
	#define INF_DETAIL		"Detail"
	#define INF_ROTATE		"Rotate"
#define MIRROR_SECTION		"Mirror"
	#define INF_PATH		"Path"
#define LASTRUN_SECTION		"Last run"
	#define INF_TESTRUN		"Test"
	#define INF_MIRRORRUN	"Mirror"


/*
 *	program parameters (changeable after due consideration :-))
 */
#define KBYTE				1000			/* could be redefined as 1024 ... */
#define MBYTE				(KBYTE*KBYTE)
#define GBYTE				(KBYTE*MBYTE)

#define MAX_TRIES			5				/* for critical_read()/_write() */

#define FOLDER_INDICATOR	0x07			/* used in undel.c */
#define CHECK_MARK			0x08			/* used in part1.c */

#define MAX_DIR_DEPTH		20
#define PARTCOPY_START_DELAY (1*200)		/* one second */
#define VALIDATION_END_DELAY (2*200)		/* two seconds */
#define	MAX_PRINT_WAIT		(5*200)			/* max time before we declare printer offline (5 secs) */

#define MAX_PARTITIONS		64				/* that we're allowed to build on one drive ... */
#define MAX_ICD_XGMPARTS	((MAX_PARTITIONS-2)/(MAX_PARTS_ICD-1))	/* this needs a diagram :-) */
#define MAX_TOPPART			(MAX_PARTITIONS-DISPLAY_PARTITIONS)
#define ROOTSEC_BUFSIZE		(MAX_PARTITIONS*PHYSICAL_SECSIZE)		/* root sector + max # extended root sectors */

											/* min & max partition size (in sectors) */
#define MIN_PARTSIZE		((MBYTE+PHYSICAL_SECSIZE-1)/PHYSICAL_SECSIZE)		/* 1MB */
#define MAX_PARTSIZE		( (9999*(MBYTE/64)/(PHYSICAL_SECSIZE/64)) + (99*MBYTE/(PHYSICAL_SECSIZE*100)) )
																				/* 9999.99MB */

#define BOOT_NAME			"EDGE  "		/* for bytes 2-7 of boot sector */

										/* file ids at the start of validation files */
#define VALIDATION_MAGIC	0x15641616		/* Wm. Shakespear birth/death */
#define VALIDATION_START	32				/* starting point within file for crc/checksum calculation, */
											/* when the 'ignore program header' option is selected      */
										/* for medic: */
#define INACTIVE_CHAR		' '				/* activity indicators */
#define ACTIVE_CHAR			0xf9
#define BADATTR_MASK		0xc0			/* attribute bits which shouldn't be set */
#define REPLACEMENT_CHAR	'X'				/* replacement character for garbage char in filename */
#define MAPBADSIZE			(64*1024L)		/* size of each buffer used for map bad function */
											/* (must be >= max cluster size) */

#define MAX_EDGEPATH		128				/* max length of paths for files used by Edge */

/*
 *	typedefs ...
 */
typedef struct {				/* added by rfb */
	char b[2];
} IWORD;

typedef struct {
	char b[4];
} ILONG;

typedef struct {
	void *next_val;
	short vdate;
	short vtime;
	long vsize;
	unsigned short crc;
	char method;
	char vname[FNSIZE+1];			//rfb: modify for LFN
} VAL;

typedef struct dta {
	char junk[21];
	char fattr;
	short ftime;
	short fdate;
	long fsize;
	char name[14];
} DTA;

struct dir_list {
	char *dirbuf;
	long offset;
	unsigned short size;			/* in clusters */
	short firstcluster;
	struct dir_list *xtradir;		/* rfb: was char * ... */
};
typedef struct dir_list DIRLIST;

typedef struct direntry {
	char name[8];
	char ext[3];
	char attr;
	char reserved[10];
	IWORD time;
	IWORD date;
	IWORD start;
	ILONG size;
} DIRENTRY;

typedef struct fileinfo {
	char name[14];					//rfb: modify for LFN
	char attr;
	short time;
	short date;
	short start;
	long size;
} FILEINFO;

typedef struct options {
							/* general options */
	int novice;
	int warn;
	unsigned long multi;		/* bitmap for 'multi' actions */
	char filepath[MAX_EDGEPATH];	/* info files, validation files & error logs */
							/* medic */
	int autofix;
	int ignore;
	int savelost;
	int visual;
	int mpause;					/* pause after medic pass w/ no errors */
	int noheader;				/* TRUE => ignore first 28 bytes in checksum/crc calcs */
							/* optimize */
	int optfull;
	int priority;
	int rmdel;
							/* undelete */
	int usemirror;
	int datemode;				/* UK (dd/mm/yy) or US (mm/dd/yy) */
							/* utility */
	int disablezero;			/* TRUE => disable zeroing options in menu */
	int keeplabel;				/* TRUE => retain volume label across zero/wipe */
} OPT;

typedef struct status {
	short action;
	short mapdev;
	short infodev;
} STATUS;

struct run_list {
	short num;
	short start;
	struct run_list *nextrun;		/* rfb: was void * ... */
};
typedef struct run_list RUN;

#define FRAGBUCKETS	7
typedef struct {
	long totfolders;		/* total folders */					//was nfolders
	long totfiles;			/* total files */					//was nfiles
	long totfragfiles;		/* total fragmented files */		//was nfragfile
	long totfragments;		/* total fragments */				//was nfrags
	long totfragsize;		/* total size of fragmented files */	//was fbyte
	struct {			/* buckets for files of various sizes (see sizelimits[]) */
		long files;			/* # of files  */					//was sizebin[]
		long fragfiles;		/* # of fragmented files */			//was sizefbin[]
		long fragments;		/* # of fragments */				//was fragbin[]
	} bin[FRAGBUCKETS];
} FRAGINFO;

/*
 *	the following used to be in bpb.h ...
 */
typedef struct {
	char isfat16;		/* TRUE iff fat16 */
	char datalite;		/* TRUE iff datalite partition */
	char media;
	unsigned short bps;	/* bytes per sector */
	short spc;			/* sec per clus */
	short spt;			/* sec per track */
	short tpc;			/* track per cylinder */
	short spf;			/* sec per fat */
	short nfat;			/* number of fat */
	short ndir;			/* max dir root entries */
	short rds;			/* # sectors in root dir */
	short nhide;		/* hidden sectors */
	short nres;			/* number reserved */
	short firstd;		/* sector number of first data */
	short cdata;		/* number of data clusters */
	short maxc;			/* maximum cluster number + 1 (always = cdata+2) */
	long tsec;			/* total number of sectors */
	long bpf;			/* bytes per FAT */
	long rdb;			/* # bytes in root dir */
	long bpc;	  		/* bytes per cluster */
	long tbyte;			/* total data space (bytes) */
	short sector_shift;	/* bits to shift to convert bytes->sectors */
	short cluster_shift;/* bits to shift to convert bytes->clusters */
	unsigned short bitmask;
	unsigned short badmask;
} MYBPB;

typedef struct {
	 char exists;
	 char type[3];
	 long firstsector;
	 long size;
} PART;

typedef struct xgminfo {
	short exists;				/* TRUE => XGM partition exists */
	long rootsec;				/* starting sector of XGM partition */
	char flag[MAX_PARTITIONS];	/* flag[n] is non-zero iff partition n is a member of the XGM partition */
} XGMINFO;
