#include <stdio.h>

#include "nvim/vim.h"
#include "nvim/types.h"
#include "nvim/memory.h"
#include "nvim/misc2.h"

#include "nvim/viml/parser/expressions.h"
#include "nvim/viml/parser/ex_commands.h"
#include "nvim/viml/printer/expressions.h"
#include "nvim/viml/printer/ex_commands.h"
#include "nvim/viml/printer/printer.h"
#include "nvim/viml/testhelpers/fgetline.h"

#ifdef INCLUDE_GENERATED_DECLARATIONS
# include "viml/testhelpers/parser.c.generated.h"
#endif

static const char *const out_str = "<written to stdout>";

/// Parse Ex command(s) and represent the result as valid VimL for testing
///
/// @param[in]  arg    Parsed string.
/// @param[in]  flags  Flags for setting CommandParserOptions.flags.
/// @param[in]  one    Determines whether to parse one Ex command or all 
///                    commands in given string.
/// @param[in]  out    Determines whether result should be output to stdout.
///
/// @return NULL in case of error, non-NULL pointer to some string if out 
///         argument is true and represented result in allocated memory 
///         otherwise.
char *parse_cmd_test(const char *arg, const uint_least8_t flags,
                     const bool one, const bool out)
  FUNC_ATTR_NONNULL_ALL FUNC_ATTR_WARN_UNUSED_RESULT
{
  CommandNode *node = NULL;
  CommandPosition position = {1, 1, "<test input>"};
  CommandParserOptions o = {flags, false};
  char *repr;
  char *r;
  size_t len;
  char **pp;

  pp = (char **) &arg;

  if (one) {
    char *p;
    char *line;
    line = fgetline_string(0, pp, 0);
    p = line;
    if (parse_one_cmd((const char **) &p, &node, o, position,
                      (VimlLineGetter) fgetline_string, (void *) pp) == FAIL)
      return NULL;
    free(line);
  } else {
    if ((node = parse_cmd_sequence(o, position,
                                   (VimlLineGetter) fgetline_string,
                                   (void *) pp, true)) == NULL) {
      return NULL;
    }
  }

  if (out) {
    print_cmd(&default_po, node, (Writer) fwrite, stdout);
    repr = (char *) out_str;
  } else {
    len = sprint_cmd_len(&default_po, node);

    repr = xcalloc(len + 1, sizeof(char));

    r = repr;

    sprint_cmd(&default_po, node, &r);
  }

  free_cmd(node);
  return repr;
}

/// Parse and then represent parsed expression
///
/// @param[in]  arg            Expression to parse.
/// @param[in]  print_as_expr  Determines whether dumped output should look as 
///                            a VimL expression or as a syntax tree.
///
/// @return Represented string or NULL in case of error (*not* parsing error).
char *srepresent_parse0(const char *arg, const bool print_as_expr)
  FUNC_ATTR_NONNULL_ALL FUNC_ATTR_WARN_UNUSED_RESULT
{
  ExpressionParserError error = {NULL, NULL};
  Expression *expr;
  size_t len = 0;
  size_t shift = 0;
  size_t offset = 0;
  size_t i;
  char *result = NULL;
  char *p;
  const char *e = arg;
  StyleOptions po;

  memset(&po, 0, sizeof(StyleOptions));

  if ((expr = parse_one((const char **) &e, &error, &parse0_err)) == NULL)
    if (error.message == NULL)
      return NULL;

  if (error.message != NULL)
    len = 6 + STRLEN(error.message);
  else
    len = (print_as_expr
           ? sprint_expr_len
           : srepresent_expr_len)(&po, expr);

  offset = e - arg;
  i = offset;
  do {
    shift++;
    i = i >> 4;
  } while (i);

  len += shift + 1;

  result = xcalloc(len + 1, sizeof(char));

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
    (print_as_expr ? sprint_expr : srepresent_expr)(&po, expr, &p);
  }

  return result;
}

/// Parse and then represent parsed expression, dumping it to stdout
///
/// @param[in]  arg            Expression to parse.
/// @param[in]  print_as_expr  Determines whether dumped output should look as 
///                            a VimL expression or as a syntax tree.
///
/// @return OK in case of success, FAIL otherwise.
int represent_parse0(const char *arg, const bool print_as_expr)
{
  ExpressionParserError error = {NULL, NULL};
  Writer write = (Writer) &fwrite;
  StyleOptions po;
  void *const cookie = (void *)stdout;

  memset(&po, 0, sizeof(StyleOptions));

  const char *e = arg;
  const Expression *expr = parse_one(&e, &error, &parse0_err);
  if (expr == NULL) {
    if (error.message == NULL) {
      return FAIL;
    }
  }

  size_t offset = e - arg;
  size_t i = offset;
  size_t shift = 0;
  do {
    shift++;
    i = i >> 4;
  } while (i);

  i = shift;
  do {
    size_t digit = (offset >> ((i - 1) * 4)) & 0xF;
    const char s[] = {(digit < 0xA ? ('0' + digit) : ('A' + (digit - 0xA)))};
    if (write(s, 1, 1, cookie) != 1) {
      return FAIL;
    }
  } while (--i);

  const char s[] = {':'};
  if (write(s, 1, 1, cookie) != 1) {
    return FAIL;
  }

  if (error.message == NULL) {
    return (print_as_expr ? print_expr : represent_expr)(&po, expr, write,
                                                         cookie);
  } else {
    size_t msglen = STRLEN(error.message);
    if (write(error.message, 1, msglen, cookie) != msglen) {
      return FAIL;
    }
  }
  return OK;
}
