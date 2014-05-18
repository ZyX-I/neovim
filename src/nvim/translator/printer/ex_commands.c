#include <stdbool.h>
#include <stddef.h>

#include "nvim/translator/printer/printer.h"
#include "nvim/translator/printer/ex_commands.c.h"

void cmd_repr(const PrinterOptions *const po, const CommandNode *node,
              char **pp)
{
  node_repr(po, node, 0, false, pp);
}

size_t cmd_repr_len(const PrinterOptions *const po, const CommandNode *node)
{
  return node_repr_len(po, node, 0, false);
}
