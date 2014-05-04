#include <stdbool.h>
#include <stddef.h>

#include "nvim/translator/printer/printer.h"

#include "nvim/translator/printer/ex_commands.c.h"

#ifdef INCLUDE_GENERATED_DECLARATIONS
# include "translator/printer/ex_commands.c.generated.h"
#endif

void cmd_repr(const PrinterOptions *const po, const CommandNode *node,
              char **pp)
  FUNC_ATTR_NONNULL_ALL
{
  node_repr(po, node, 0, false, pp);
}

size_t cmd_repr_len(const PrinterOptions *const po, const CommandNode *node)
  FUNC_ATTR_NONNULL_ALL FUNC_ATTR_CONST
{
  return node_repr_len(po, node, 0, false);
}
