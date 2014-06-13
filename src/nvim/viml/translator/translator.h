#ifndef NEOVIM_VIML_TRANSLATOR_TRANSLATOR_H
#define NEOVIM_VIML_TRANSLATOR_TRANSLATOR_H

#include "nvim/viml/parser/expressions.h"
#include "nvim/viml/parser/ex_commands.h"

typedef size_t (*Writer)(const void *ptr, size_t len, size_t nmemb,
                         void *cookie);

#ifdef INCLUDE_GENERATED_DECLARATIONS
# include "viml/translator/translator.h.generated.h"
#endif
#endif  // NEOVIM_VIML_TRANSLATOR_TRANSLATOR_H
