#ifndef NEOVIM_TRANSLATOR_TRANSLATOR_TRANSLATOR_H
#define NEOVIM_TRANSLATOR_TRANSLATOR_TRANSLATOR_H

#include "translator/parser/expressions.h"
#include "translator/parser/ex_commands.h"

typedef size_t (*Writer)(const void *ptr, size_t len, size_t nmemb,
                         void *cookie);

int translate_script(const CommandNode *const node, Writer write, void *cookie);

#endif  // NEOVIM_TRANSLATOR_TRANSLATOR_TRANSLATOR_H
