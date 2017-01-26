/*
 *	version.h: version-related stuff for Edge
 */
#ifdef BETA
#define PROGRAM_VERSION	"2.55á1"
#define EXPIRY_DATE		( ((2007-1980)<<9)+(12<<5)+31 )		/* 31 Dec 2007 */
#else
#define PROGRAM_VERSION	"2.55"
#endif

#define RSC_VERSION		"2.55"				/* note: dependency on size of object in EDGE.RSC !!! */
