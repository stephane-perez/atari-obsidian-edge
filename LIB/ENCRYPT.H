/*
 *	encrypt.h
 */
struct pgm_protect {
	unsigned long protect_magic1;
	unsigned long registration_code;
	unsigned long encrypted_name;
	unsigned long protect_magic2;
};
#define PROTECT_MAGIC1	0xfe0311a5		/* quasi-random */
#define PROTECT_MAGIC2	0x268104dc

unsigned long encrypt(char *s,char *init);
void hex_to_external(char *s,unsigned long hex);
unsigned long external_to_hex(char *s);
