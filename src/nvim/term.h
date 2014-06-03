#ifndef NVIM_TERM_H
#define NVIM_TERM_H

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

#ifdef INCLUDE_GENERATED_DECLARATIONS
# include "term.h.generated.h"
#endif
#endif  // NVIM_TERM_H
