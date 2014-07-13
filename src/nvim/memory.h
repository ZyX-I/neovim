#ifndef NVIM_MEMORY_H
#define NVIM_MEMORY_H

#include <stddef.h>  // for size_t

#define XMALLOC_NEW(type, number) (xmalloc(sizeof(type) * number))
#define XCALLOC_NEW(type, number) (xcalloc(sizeof(type), number))

#ifdef INCLUDE_GENERATED_DECLARATIONS
# include "memory.h.generated.h"
#endif
#endif  // NVIM_MEMORY_H
