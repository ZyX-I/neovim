#include "nvim/vim.h"
#include "nvim/memory.h"
#include "nvim/misc2.h"

#include "nvim/viml/parser/expressions.h"
#include "nvim/viml/parser/ex_commands.h"
#include "nvim/viml/printer/expressions.h"
#include "nvim/viml/printer/ex_commands.h"
#include "nvim/viml/printer/printer.h"
#include "nvim/viml/testhelpers/fgetline.h"

/// Parse Ex command(s) and represent the result as valid VimL for testing
///
/// @param[in]  arg    Parsed string.
/// @param[in]  flags  Flags for setting CommandParserOptions.flags.
/// @param[in]  one    Determines whether to parse one Ex command or all 
///                    commands in given string.
///
/// @return Represented result or NULL in case of error.
char *parse_cmd_test(const char *arg, const uint_least8_t flags,
                     const bool one)
{
  CommandNode *node = NULL;
  CommandPosition position = {1, 1, (char_u *) "<test input>"};
  CommandParserOptions o = {flags, false};
  char *repr;
  char *r;
  size_t len;
  char **pp;

  pp = (char **) &arg;

  if (one) {
    char_u *p;
    char_u *line;
    line = (char_u *) fgetline_string(0, pp, 0);
    p = line;
    if (parse_one_cmd((const char_u **) &p, &node, o, position,
                      (LineGetter) fgetline_string, (void *) pp) == FAIL)
      return NULL;
    free(line);
  } else {
    if ((node = parse_cmd_sequence(o, position, (LineGetter) fgetline_string,
                                   (void *) pp)) == NULL)
      return NULL;
  }

  len = print_cmd_len(&default_po, node);

  repr = XCALLOC_NEW(char, len + 1);

  r = repr;

  print_cmd(&default_po, node, &r);

  free_cmd(node);
  return repr;
}

/// Represent parsed expression
///
/// @param[in]  arg            Expression to parse.
/// @param[in]  print_as_expr  Determines whether dumped output should look as 
///                            a VimL expression or as a syntax tree.
///
/// @return Represented string or NULL in case of error (*not* parsing error).
char *represent_parse0(const char *arg, const bool print_as_expr)
{
  ExpressionParserError error = {NULL, NULL};
  ExpressionNode *expr;
  size_t len = 0;
  size_t shift = 0;
  size_t offset = 0;
  size_t i;
  char *result = NULL;
  char *p;
  const char *e = arg;
  PrinterOptions po;

  memset(&po, 0, sizeof(PrinterOptions));

  if ((expr = parse0_err((const char_u **) &e, &error)) == NULL)
    if (error.message == NULL)
      return NULL;

  if (error.message != NULL)
    len = 6 + STRLEN(error.message);
  else
    len = (print_as_expr ? print_expr_node_len : represent_expr_node_len)(&po,
                                                                          expr);

  offset = e - arg;
  i = offset;
  do {
    shift++;
    i = i >> 4;
  } while (i);

  len += shift + 1;

  result = XCALLOC_NEW(char, len + 1);

  p = result;

  i = shift;
  do {
    size_t digit = (offset >> ((i - 1) * 4)) & 0xF;
    *p++ = (digit < 0xA ? ('0' + digit) : ('A' + (digit - 0xA)));
  } while (--i);

  *p++ = ':';

  if (error.message != NULL) {
    memcpy(p, "error:", 6);
    p += 6;
    STRCPY(p, error.message);
  } else {
    (print_as_expr ? print_expr_node : represent_expr_node)(&po, expr, &p);
  }

  return result;
}
