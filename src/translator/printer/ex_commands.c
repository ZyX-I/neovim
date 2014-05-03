#include <stddef.h>
#include <stdbool.h>

#include "vim.h"
#include "memory.h"
#include "strings.h"
#include "misc2.h"

#include "translator/parser/ex_commands.h"

char_u *fgetline_string(int c, char_u **arg, int indent)
{
  size_t len = 0;
  char_u *result;

  if (**arg == '\0')
    return NULL;

  while ((*arg)[len] != '\n' && (*arg)[len] != '\0')
    len++;

  result = vim_strnsave(*arg, len);

  if ((*arg)[len] == '\0')
    *arg += len;
  else
    *arg += len + 1;

  return result;
}

#include "translator/printer/ex_commands.c.h"

char *parse_cmd_test(char_u *arg, uint_least8_t flags, bool one)
{
  CommandNode *node = NULL;
  CommandPosition position = {1, 1, (char_u *) "<test input>"};
  CommandParserOptions o = {flags, false};
  char *repr;
  char *r;
  size_t len;
  char_u **pp;

  pp = &arg;

  if (one) {
    char_u *p;
    char_u *line;
    line = fgetline_string(0, pp, 0);
    p = line;
    if (parse_one_cmd(&p, &node, o, &position, (LineGetter) fgetline_string,
                      pp) == FAIL)
      return NULL;
    vim_free(line);
  } else {
    if ((node = parse_cmd_sequence(o, position, (LineGetter) fgetline_string,
                                   pp)) == NULL)
      return NULL;
  }

  len = node_repr_len(&default_po, node, 0, false);

  repr = XCALLOC_NEW(char, len + 1);

  r = repr;

  node_repr(&default_po, node, 0, false, &r);

  free_cmd(node);
  return repr;
}
