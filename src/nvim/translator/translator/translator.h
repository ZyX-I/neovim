#ifndef NEOVIM_TRANSLATOR_TRANSLATOR_TRANSLATOR_H
#define NEOVIM_TRANSLATOR_TRANSLATOR_TRANSLATOR_H

#include "nvim/translator/parser/expressions.h"
#include "nvim/translator/parser/ex_commands.h"

typedef size_t (*Writer)(const void *ptr, size_t len, size_t nmemb,
                         void *cookie);

#ifdef INCLUDE_GENERATED_DECLARATIONS
# include "translator/translator/translator.h.generated.h"
#endif
#endif  // NEOVIM_TRANSLATOR_TRANSLATOR_TRANSLATOR_H
