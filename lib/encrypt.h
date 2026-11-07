/*
 *	encrypt.h
 *
 *	Modified for Obsidian Edge (November 2026): GCC port: encrypt() renamed to avoid the MiNTLib function of the same name
 */
struct pgm_protect {
	unsigned long protect_magic1;
	unsigned long registration_code;
	unsigned long encrypted_name;
	unsigned long protect_magic2;
};
#define PROTECT_MAGIC1	0xfe0311a5		/* quasi-random */
#define PROTECT_MAGIC2	0x268104dc

#ifdef __GNUC__
#define encrypt	pgm_encrypt		/* MiNTLib's <unistd.h> has an encrypt() too */
#endif
unsigned long encrypt(char *s,char *init);
void hex_to_external(char *s,unsigned long hex);
unsigned long external_to_hex(char *s);
