#include <stdbool.h>
#include <stddef.h>

#include "translator/printer/printer.h"
#include "translator/printer/ex_commands.c.h"

void cmd_repr(const PrinterOptions *const po, const CommandNode *node,
              char **pp)
{
  node_repr(po, (CommandNode *) node, 0, false, pp);
}

size_t cmd_repr_len(const PrinterOptions *const po, const CommandNode *node)
{
  return node_repr_len(po, (CommandNode *) node, 0, false);
}
