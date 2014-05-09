#ifndef NEOVIM_TERM_H
#define NEOVIM_TERM_H
/* term.c */
int set_termname(char_u *term);
void set_mouse_termcode(int n, char_u *s);
void del_mouse_termcode(int n);
void getlinecol(long *cp, long *rp);
int add_termcap_entry(char_u *name, int force);
int term_is_8bit(char_u *name);
char_u *tltoa(unsigned long i);
void termcapinit(char_u *name);
void out_flush(void);
void out_flush_check(void);
void out_char(unsigned c);
void out_str_nf(char_u *s);
void out_str(char_u *s);
void term_windgoto(int row, int col);
void term_cursor_right(int i);
void term_append_lines(int line_count);
void term_delete_lines(int line_count);
void term_set_winpos(int x, int y);
void term_set_winsize(int width, int height);
void term_fg_color(int n);
void term_bg_color(int n);
void term_settitle(char_u *title);
void ttest(int pairs);
void check_shellsize(void);
void limit_screen_size(void);
void win_new_shellsize(void);
void shell_resized(void);
void shell_resized_check(void);
void set_shellsize(int width, int height, int mustset);
void settmode(int tmode);
void starttermcap(void);
void stoptermcap(void);
void may_req_termresponse(void);
void may_req_ambiguous_char_width(void);
int swapping_screen(void);
void setmouse(void);
int mouse_has(int c);
int mouse_model_popup(void);
void scroll_start(void);
void cursor_on(void);
void cursor_off(void);
void term_cursor_shape(void);
void scroll_region_set(win_T *wp, int off);
void scroll_region_reset(void);
void clear_termcodes(void);
void add_termcode(char_u *name, char_u *string, int flags);
char_u *find_termcode(char_u *name);
char_u *get_termcode(int i);
void del_termcode(char_u *name);
void set_mouse_topline(win_T *wp);
int check_termcode(int max_offset, char_u *buf, int bufsize,
                           int *buflen);
char_u *replace_termcodes(const char_u *from, size_t len, char_u **bufp,
                          int from_part, int do_lt, int special, int cpo_flags);
int find_term_bykeys(const char_u *src, int slen);
void show_termcodes(void);
int show_one_termcode(char_u *name, char_u *code, int printit);
char_u *translate_mapping(char_u *str, int expmap, int cpo_flags);

#define FLAG_CPO_BSLASH    0x01
#define FLAG_CPO_SPECI     0x02
#define FLAG_CPO_KEYCODE   0x04
#define CPO_TO_CPO_FLAGS   (((vim_strchr(p_cpo, CPO_BSLASH) == NULL) \
                             ? 0 \
                             : FLAG_CPO_BSLASH)| \
                            (vim_strchr(p_cpo, CPO_SPECI) == NULL \
                             ? 0 \
                             : FLAG_CPO_SPECI)| \
                            (vim_strchr(p_cpo, CPO_KEYCODE) == NULL \
                             ? 0 \
                             : FLAG_CPO_KEYCODE))
#endif /* NEOVIM_TERM_H */
