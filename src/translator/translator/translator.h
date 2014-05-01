#ifndef NEOVIM_TRANSLATOR_TRANSLATOR_TRANSLATOR_H
#define NEOVIM_TRANSLATOR_TRANSLATOR_TRANSLATOR_H

#include "translator/parser/expressions.h"
#include "translator/parser/ex_commands.h"

typedef size_t (*Writer)(const void *ptr, size_t len, size_t nmemb,
                         void *cookie);

int translate_script(const CommandNode *const node, Writer write, void *cookie);
int translate_script_std(void);
int translate_script_str_to_file(const char_u *const str,
                                 const char *const fname);

#endif  // NEOVIM_TRANSLATOR_TRANSLATOR_TRANSLATOR_H
