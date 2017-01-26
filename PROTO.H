/*
 *	global function prototypes
 */
/* Prototypes for functions defined in E:\SOURCE\EDGE204\COMPILE.RFB\COPYD.C */
PUBLIC void do_copydefrag(void);

/* Prototypes for functions defined in E:\SOURCE\EDGE204\COMPILE.RFB\DISK.C */
PUBLIC void cleanup_allinfo(void);
PUBLIC int critical_read(short dev,short numsec,long start,char *buf,int errmsgno,int pgmloc);
PUBLIC int critical_write(short dev,short numsec,long start,char *buf,int errmsgno,int pgmloc);
PUBLIC void do_chart(int device);
PUBLIC void do_copyimage(void);
PUBLIC void fat12_to_fat16(char *f12,unsigned short *f16);
PUBLIC void fat16_to_fat12(unsigned short *f16,char *f12);
PUBLIC long gemdos_read(short fh,long count,void *buf,char *name,int loc);
PUBLIC long gemdos_write(short fh,long count,void *buf,char *name,int loc);
PUBLIC int get_mybpb(MYBPB *mybpb,short device);
PUBLIC int get_name(DIRENTRY *dir,char *fname);
PUBLIC void loadboot(unsigned char *buff,MYBPB *mybpb);
PUBLIC int load_mybpb(unsigned char *buff,MYBPB *mybpb,BPB *tbpb);
PUBLIC long logical_read(short dev,short numsec,long start,void *buf,int errmsgno,int loc);
PUBLIC long logical_write(short dev,short numsec,long start,void *buf,int errmsgno,int loc);
PUBLIC unsigned short logsector(short cluster);
PUBLIC void mark_chart(int device);
PUBLIC void permanent_error(long errnum,int type,unsigned char *msg,int loc);
PUBLIC int read_no_retry(short dev,short numsec,long start,char *buf);
PUBLIC int write_no_retry(short dev,short numsec,long start,char *buf);

/* Prototypes for functions defined in E:\SOURCE\EDGE204\COMPILE.RFB\DMAP.C */
PUBLIC void cleanup_diskinfo(void);
PUBLIC void cleanup_diskmap(void);
PUBLIC int do_diskinfo(void);
PUBLIC int do_diskmap(void);
PUBLIC void do_unzero(void);
PUBLIC void do_wipe(void);
PUBLIC void do_zero(void);
PUBLIC void draw_block(short cluster,short mode);
PUBLIC void draw_opt(short cluster,short mode);
PUBLIC void format_cpb(char *text);
PUBLIC int zero_fats_and_root(void);

/* Prototypes for functions defined in E:\SOURCE\EDGE204\COMPILE.RFB\EDGEASM.S */
PUBLIC void cold_boot(void);
PUBLIC void fast_copy(void *to,void *from,long size);

/* Prototypes for functions defined in E:\SOURCE\EDGE204\COMPILE.RFB\FILEIO.C */
PUBLIC int check_path(char *path);
PUBLIC void copy_to_edgepath(char *t,char *s);
PUBLIC void copy_to_tedinfo(TEDINFO *t,char *s);
PUBLIC void do_infoprint(void);
PUBLIC void do_prefs(OPT *opt);
PUBLIC void init_prefs(void);
PUBLIC int get_existing_fname(char *pfname,const char *path,const char *fmask,const char *prompt);
PUBLIC int get_path(char *pfname,const char *path,const char *fmask,const char *prompt);
PUBLIC int get_writeable_fname(char *pfname,const char *path,const char *fmask,const char *prompt);
PUBLIC void makedir(char *todir);
PUBLIC void print_file(void);
PUBLIC int print_string(char *str);
PUBLIC void save_prefs(OPT *opt);

/* Prototypes for functions defined in E:\SOURCE\EDGE204\COMPILE.RFB\INFO.C */
PUBLIC void handle_scsiinfo_dialog(void(*)(short,short));
PUBLIC long read_info_file(char *buf,long maxsize);
PUBLIC void restore_disk_info(int unzero);
PUBLIC void restore_scsi_info(void);
PUBLIC void save_disk_info(void);
PUBLIC int save_info(short savedev,unsigned char *path);
PUBLIC void save_scsi_info(void);

/* Prototypes for functions defined in E:\SOURCE\EDGE204\COMPILE.RFB\MAIN.C */
PUBLIC void handle_messages(void);
PUBLIC int main(void);

/* Prototypes for functions defined in E:\SOURCE\EDGE204\COMPILE.RFB\MEDIC1.C */
PUBLIC int bad_prefs(void);
PUBLIC int do_bad(int autofix);
PUBLIC int do_tdisk(int called);

/* Prototypes for functions defined in E:\SOURCE\EDGE204\COMPILE.RFB\MIRROR.C */
PUBLIC void config_mirror(void);
PUBLIC int init_mirror_prefs(void);

/* Prototypes for functions defined in E:\SOURCE\EDGE204\COMPILE.RFB\MISC.C */
PUBLIC long allocate_max_units(int min_units,long unit_size,char **ptr);
PUBLIC void blank(short color);
PUBLIC void browse(void);
PUBLIC unsigned long CDmap(void);
PUBLIC void ce_off(void);
PUBLIC void ce_on(void);
PUBLIC void change_mode(int new_mode);
PUBLIC int decode_date_time(char *s,unsigned short date,unsigned short time,int format);
PUBLIC void delay(long time);
PUBLIC void force_update(int device);
PUBLIC void full_bar(void);
PUBLIC int get_key(void);
PUBLIC long get_long(unsigned char *in);
PUBLIC int get_newname(unsigned char *fname,unsigned char *path,unsigned char *name);
PUBLIC int get_src_dest(void);
PUBLIC short get_sys(short *p);
PUBLIC unsigned short get_word(unsigned char *in);
PUBLIC void init_bar(long max);
PUBLIC int mouse_off(void);
PUBLIC int mouse_on(void);
PUBLIC short nflop(void);
PUBLIC void putword(IWORD *w,unsigned short val);
PUBLIC void putlong(ILONG *l,long val);
PUBLIC void refresh_screen(OBJECT *tree);
PUBLIC void start_bar(unsigned char *str1);
PUBLIC void update_bar(long num);
PUBLIC void update_barmsg(unsigned char *strng);
PUBLIC void wrap_bar(void);

/* Prototypes for functions defined in E:\SOURCE\EDGE204\COMPILE.RFB\OPT2.C */
PUBLIC short do_opt(void);
PUBLIC short do_opt_compress(short display);
PUBLIC unsigned char *sort_dir(unsigned char *curdir,int ncl,int rmdel);
PUBLIC unsigned char *sort_root(unsigned char *dirbuf,int rmdel);

/* Prototypes for functions defined in E:\SOURCE\EDGE204\COMPILE.RFB\PART1.C */
PUBLIC void do_down_one(void);
PUBLIC void do_down_page(void);
PUBLIC void do_edisksize(void);
PUBLIC void do_part(void);
PUBLIC void do_random_page(short pos);
PUBLIC void do_scsi_edit(short id,short lun,char *buf,long bufsize);
PUBLIC void do_up_one(void);
PUBLIC void do_up_page(void);
PUBLIC PART *find_xgm(char *buf);
PUBLIC void init_scsi_editobject(void);
PUBLIC long load_root_sectors(short id,short lun,char *buf);
PUBLIC int scsi_op_failed(short id,short lun);
PUBLIC void write_partinfo_file(char *path,short id,short lun,char *buf,long size);

/* Prototypes for functions defined in E:\SOURCE\EDGE204\COMPILE.RFB\PROTECT.C */
PUBLIC int protect(void);

/* Prototypes for functions defined in E:\SOURCE\EDGE204\COMPILE.RFB\RSC.C */
PUBLIC void cleanup_rsrc(void);
PUBLIC int do_dial(OBJECT *tree,short txt,short start,short dohot);
PUBLIC void draw_dial(OBJECT *tree,short start);
PUBLIC int edit_form_do(OBJECT *tree,short start_fld);
PUBLIC void fix_rsrc(void);
PUBLIC void grect_to_array(GRECT *area,WORD *array);
PUBLIC int handle_dial(OBJECT *tree,short txt,short start,short dohot);
PUBLIC void init_forms(void);
PUBLIC int load_rsrc(void);
PUBLIC void myform_center(OBJECT *tree);
PUBLIC int myform_do(OBJECT *tree,short *start_fld,short do_hot);
PUBLIC int mywatchbox(OBJECT *tree,short idex,short instate,short outstate);
PUBLIC void new_icon(void);
PUBLIC void obj_head(OBJECT *tree);
PUBLIC void objc_dsel(OBJECT *tree,short obj);
PUBLIC void objc_sel(OBJECT *tree,short obj);
PUBLIC void objc_xdsel(OBJECT *tree,short obj);
PUBLIC void objc_xsel(OBJECT *tree,short obj);
PUBLIC void redraw_object(OBJECT *tree,short idex);
PUBLIC void redraw_xobject(OBJECT *tree,short idex);
PUBLIC void refresh_now(OBJECT *tree,short obj);
PUBLIC void restore_map(GRECT rect);
PUBLIC void restore_screen(void);
PUBLIC void save_map(void);
PUBLIC void save_screen(void);
PUBLIC int scsi_form_do(OBJECT *tree,WORD *start_fld);
PUBLIC void set_clip(void);
PUBLIC void toggle_bottom(short obj);
PUBLIC void wait_for_redraw(short win);
PUBLIC int xform_alert(int dft,int index, ...);

/* Prototypes for functions defined in E:\SOURCE\EDGE204\COMPILE.RFB\SECTION.C */
PUBLIC void close_section(void);
PUBLIC int open_section(char *basename,char *mode,char *sectionname);
PUBLIC void read_section_line(char *pattern,char *fmt,...);
PUBLIC int write_section_line(char *pattern,char *fmt,...);

/* Prototypes for functions defined in E:\SOURCE\EDGE204\COMPILE.RFB\UNDEL.C */
PUBLIC void blit(GRECT *src,GRECT *dst);
PUBLIC void cleanup_undelete(void);
PUBLIC void init_ud_form(void);
PUBLIC int init_undel(void);
PUBLIC int init_uroot(short device,int draw);
PUBLIC void load_ulines(short start);
PUBLIC void udo_down_page(void);
PUBLIC void udo_down1(void);
PUBLIC void udo_find(int fnext);
PUBLIC void udo_line(short obj,short clicks,short shift);
PUBLIC void udo_parent(void);
PUBLIC void udo_random_page(short pos);
PUBLIC void udo_up_page(void);
PUBLIC void udo_up1(void);
PUBLIC void undelete_files(void);

/* Prototypes for functions defined in E:\SOURCE\EDGE204\COMPILE.RFB\VALID.C */
PUBLIC unsigned short addchk(unsigned short chksum,unsigned short *p,long count);
PUBLIC unsigned short addcrc(unsigned short crc,unsigned char *p,long count);
PUBLIC void do_valid(short type,short val);

/* Prototypes for functions defined in E:\SOURCE\EDGE204\COMPILE.RFB\FILEIO.C */
PUBLIC void view_file(char *namein,short draw);
