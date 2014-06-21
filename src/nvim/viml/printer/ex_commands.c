#include <stdbool.h>
#include <stddef.h>

#include "nvim/viml/printer/printer.h"
#include "nvim/viml/printer/ex_commands.c.h"

void sprint_cmd(const PrinterOptions *const po, const CommandNode *node,
              char **pp)
{
  sprint_node(po, node, 0, false, pp);
}

size_t sprint_cmd_len(const PrinterOptions *const po, const CommandNode *node)
{
  return sprint_node_len(po, node, 0, false);
}
