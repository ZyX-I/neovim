#include <stdbool.h>
#include <stddef.h>

#include "nvim/viml/printer/printer.h"
#include "nvim/viml/printer/ex_commands.c.h"
#include "nvim/viml/dumpers/dumpers.h"

void sprint_cmd(const PrinterOptions *const po, const CommandNode *node,
              char **pp)
{
  sprint_node(po, node, 0, false, pp);
}

size_t sprint_cmd_len(const PrinterOptions *const po, const CommandNode *node)
{
  return sprint_node_len(po, node, 0, false);
}

int print_cmd(const PrinterOptions *const po, const CommandNode *node,
              Writer write, void *cookie)
{
  return print_node(po, node, 0, false, write, cookie);
}
