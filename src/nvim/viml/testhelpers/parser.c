#include "nvim/vim.h"
#include "nvim/memory.h"
#include "nvim/misc2.h"

#include "nvim/viml/parser/expressions.h"
#include "nvim/viml/parser/ex_commands.h"
#include "nvim/viml/printer/expressions.h"
#include "nvim/viml/printer/ex_commands.h"
#include "nvim/viml/printer/printer.h"
#include "nvim/viml/testhelpers/fgetline.h"

#ifdef INCLUDE_GENERATED_DECLARATIONS
# include "translator/testhelpers/parser.c.generated.h"
#endif

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
  FUNC_ATTR_NONNULL_ALL FUNC_ATTR_WARN_UNUSED_RESULT
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
    if (parse_one_cmd((const char_u **) &p, &node, o, &position,
                      (LineGetter) fgetline_string, (void *) pp) == FAIL)
      return NULL;
    vim_free(line);
  } else {
    if ((node = parse_cmd_sequence(o, position, (LineGetter) fgetline_string,
                                   (void *) pp)) == NULL)
      return NULL;
  }

  len = cmd_repr_len(&default_po, node);

  repr = XCALLOC_NEW(char, len + 1);

  r = repr;

  cmd_repr(&default_po, node, &r);

  free_cmd(node);
  return repr;
}

/// Represent parsed expression
///
/// @param[in]  arg           Expression to parse.
/// @param[in]  dump_as_expr  Determines whether dumped output should look as 
///                           a VimL expression or as a syntax tree.
///
/// @return Represented string or NULL in case of error (*not* parsing error).
char *parse0_repr(const char *arg, const bool dump_as_expr)
  FUNC_ATTR_NONNULL_ALL FUNC_ATTR_WARN_UNUSED_RESULT
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
    len = (dump_as_expr ? expr_node_dump_len : expr_node_repr_len)(&po, expr);

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
    (dump_as_expr ? expr_node_dump : expr_node_repr)(&po, expr, &p);
  }

  return result;
}
