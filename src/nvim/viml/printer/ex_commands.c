#include <stdbool.h>
#include <stddef.h>

#include "nvim/viml/printer/printer.h"

#include "nvim/viml/printer/ex_commands.c.h"

#ifdef INCLUDE_GENERATED_DECLARATIONS
# include "translator/printer/ex_commands.c.generated.h"
#endif

void sprint_cmd(const PrinterOptions *const po, const CommandNode *node,
              char **pp)
  FUNC_ATTR_NONNULL_ALL
{
  sprint_node(po, node, 0, false, pp);
}

size_t sprint_cmd_len(const PrinterOptions *const po, const CommandNode *node)
  FUNC_ATTR_NONNULL_ALL FUNC_ATTR_CONST
{
  return sprint_node_len(po, node, 0, false);
}
