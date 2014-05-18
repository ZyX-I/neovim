#ifndef NEOVIM_TRANSLATOR_TESTHELPERS_TRANSLATOR_H
#define NEOVIM_TRANSLATOR_TESTHELPERS_TRANSLATOR_H

int translate_script_std(void);
int translate_script_str_to_file(const char_u *const str,
                                 const char *const fname);

#endif  // NEOVIM_TRANSLATOR_TESTHELPERS_TRANSLATOR_H
