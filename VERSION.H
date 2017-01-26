/*
 *	version.h: version-related stuff for Edge
 */
#ifdef BETA
#define PROGRAM_VERSION	"2.54á1"
#define EXPIRY_DATE		( ((2003-1980)<<9)+(12<<5)+31 )		/* 31 Dec 2003 */
#else
#define PROGRAM_VERSION	"2.54"
#endif

#define RSC_VERSION		"2.50"				/* note: dependency on size of object in EDGE.RSC !!! */
