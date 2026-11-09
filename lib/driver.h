/*
 *	header for edge's driver.lib
 *
 *	The following functions implemented:
 *		int init_driver(void)
 *			must be called before any others to initialise stuff.
 *			returns 0 if ok, -1 if error
 *
 *		void close_driver(void)
 *			call when finished
 *
 *		int bus_exists(int bus)
 *			returns TRUE if specified bus exists, else returns FALSE
 *
 *		int device_exists(short busid,short lun,char *buf)
 *			test if specified busid+lun exists (busid is the standard
 *			atari device number).  if so, returns the device type and
 *			copies the SCSI Inquiry manufacturer+model (24 bytes) to
 *			buf, else returns -ve
 *
 *		int get_capacity(short busid,short lun,unsigned long *capacity)
 *			gets the capacity (in blocks) of the specified busid+lun.
 *			returns 0 if ok, and copies the capacity to '*capacity',
 *			else returns -ve
 *
 *		int phys_read(short busid,short lun,short numsec,long start,char *buf)
 *			reads 'numsec' blocks starting at block 'start' into 'buf'
 *			from device specified by busid+lun. returns 0 if ok, else -ve
 *
 *		int phys_write(short busid,short lun,short numsec,long start,char *buf)
 *			writes 'numsec' blocks starting at block 'start' from 'buf'
 *			to device specified by busid+lun. returns 0 if ok, else -ve
 *
 *		void rescan_buses(void)
 *			causes rescan of all buses
 */
#define NUM_BUSES	32
#define IDS_PER_BUS	8
#define HD_DEVICE	0x00	/* returned by device_exists() */

int init_driver(void);
void close_driver(void);
int bus_exists(int bus);
int device_exists(short busid,short lun,char *buf);
int get_capacity(short busid,short lun,unsigned long *capacity);
int phys_read(short busid,short lun,short numsec,long start,char *buf);
int phys_write(short busid,short lun,short numsec,long start,char *buf);
void rescan_buses(void);
