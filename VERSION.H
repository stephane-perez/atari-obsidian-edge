/*
 *	version.h: version-related stuff for Edge
 */
#ifdef BETA
#define PROGRAM_VERSION	"2.50á2"
#define EXPIRY_DATE		( ((2003-1980)<<9)+(6<<5)+30 )		/* 30 Jun 2003 */
#else
#define PROGRAM_VERSION	"2.50"
#endif

#define RSC_VERSION		"2.50"				/* note: dependency on size of object in EDGE.RSC !!! */
