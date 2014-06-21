#include <stdbool.h>
#include <stddef.h>

#include "nvim/viml/printer/printer.h"
#include "nvim/viml/printer/ex_commands.c.h"

void print_cmd(const PrinterOptions *const po, const CommandNode *node,
              char **pp)
{
  print_node(po, node, 0, false, pp);
}

size_t print_cmd_len(const PrinterOptions *const po, const CommandNode *node)
{
  return print_node_len(po, node, 0, false);
}
