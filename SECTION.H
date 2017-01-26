/*
 * functions defined in section.c
 */
void close_section(void);
int open_section(char *basename,char *mode,char *sectionname);
void read_section_line(char *pattern,char *fmt,...);
int write_section_line(char *pattern,char *fmt,...);
