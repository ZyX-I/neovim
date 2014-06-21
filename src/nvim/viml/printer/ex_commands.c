#include <stdbool.h>
#include <stddef.h>

#include "nvim/viml/printer/printer.h"

#include "nvim/viml/printer/ex_commands.c.h"

#ifdef INCLUDE_GENERATED_DECLARATIONS
# include "translator/printer/ex_commands.c.generated.h"
#endif

void print_cmd(const PrinterOptions *const po, const CommandNode *node,
               char **pp)
  FUNC_ATTR_NONNULL_ALL
{
  print_node(po, node, 0, false, pp);
}

size_t print_cmd_len(const PrinterOptions *const po, const CommandNode *node)
  FUNC_ATTR_NONNULL_ALL FUNC_ATTR_CONST
{
  return print_node_len(po, node, 0, false);
}
