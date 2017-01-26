					/* used by evnt_multi() */
#define LEFT_BUTTON		0x0001
#define RIGHT_BUTTON	0x0002
#define NO_BUTTON		0x0000
#define BUTTON_UP		0x0000
#define BUTTON_DOWN		0x0001
#define M1_ENTER		0x0000
#define M1_EXIT			0x0001
#define NUM_CLICKS		2

					/* standard gadgets for our window */
#define PARTS		NAME|INFO|CLOSER|MOVER

					/* 'type' arg to do_multi() */
#define OPTM		1
#define TESTM		2
#define MBADM		3
#define VALM		4

					/* index to ERROR_TYPE messages */
#define READ		0
#define WRITE		1
#define OPEN		2
#define DISKFULL	3
#define MEMALLOC	4

					/* options for date display */
#define US			0		/* month/day/year (you figure) */
#define UK			1		/* day/month/year */

					/* index to ERROR_LOC messages */
#define BOOT		0
#define FAT			1
#define ROOTDIR		2
#define DIR			3
#define FILEERR		4
#define HARDDISK	5

					/* action for new files in valid.c */
#define ADDFILE		1
#define SKIPFILE	2
#define ASKFILE		3

					/* values for status.action */
#define IDLE		0
#define DISKINFO	1
#define DISKMAP		2
#define ALLINFO		3
#define UNDELETE	4
#define MAKEVAL		5
#define CHECKVAL	6
#define ESCSI		7

                    /* Optimization Methods */
#define DEFRAG		1
#define COMP		2

					/* used in valid.c */
#define CHECKSUM	1
#define CRC			2

					/* flag argument for draw_block() */
#define CLUSTER_ERR	0
#define CLUSTER_OK	1

					/* display modes while checking disk structure in medic.c */
#define INACTIVE_MODE	0
#define ACTIVE_MODE		1
#define ERROR_MODE		2

					/* preference file special codes */
#define PAUSE_FOREVER	99
