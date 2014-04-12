#include <stdbool.h>
#include <stddef.h>

#include "vim.h"
#include "memory.h"
#include "translator/parser/expressions.h"
#include "translator/printer/printer.h"

static char *expression_type_string[] = {
  "Unknown",
  "?:",
  "||",
  "&&",
  ">",
  ">=",
  "<",
  "<=",
  "==",
  "!=",
  "is",
  "isnot",
  "=~",
  "!~",
  "+",
  "-",
  "*",
  "/",
  "%",
  "..",
  "!",
  "-!",
  "+!",
  "N",
  "O",
  "X",
  "F",
  "\"",
  "'",
  "&",
  "@",
  "$",
  "cvar",
  "var",
  "id",
  "curly",
  "expr",
  "[]",
  "{}",
  "index",
  ".",
  "call",
  "empty",
};

static char *case_compare_strategy_string[] = {
  "",
  "#",
  "?",
};

#include "translator/printer/expressions.c.h"

size_t expr_node_dump_len(PrinterOptions *po, ExpressionNode *node)
{
  size_t len = node_dump_len(po, node);
  ExpressionNode *next = node->next;

  while (next != NULL) {
    len++;
    len += node_dump_len(po, next);
    next = next->next;
  }

  return len;
}

void expr_node_dump(PrinterOptions *po, ExpressionNode *node, char **pp)
{
  ExpressionNode *next = node->next;

  node_dump(po, node, pp);

  while (next != NULL) {
    *(*pp)++ = ' ';
    node_dump(po, next, pp);
    next = next->next;
  }
}

char *parse0_repr(char_u *arg, bool dump_as_expr)
{
  TestExprResult *p0_result;
  size_t len = 0;
  size_t shift = 0;
  size_t offset = 0;
  size_t i;
  char *result = NULL;
  char *p;
  PrinterOptions po;

  memset(&po, 0, sizeof(PrinterOptions));

  if ((p0_result = parse0_test(arg)) == NULL)
    goto theend;

  if (p0_result->error.message != NULL)
    len = 6 + STRLEN(p0_result->error.message);
  else
    len = (dump_as_expr ? expr_node_dump_len : node_repr_len)(&po,
                                                              p0_result->node);

  offset = p0_result->end - arg;
  i = offset;
  do {
    shift++;
    i = i >> 4;
  } while (i);

  len += shift + 1;

  if (!(result = ALLOC_CLEAR_NEW(char, len + 1)))
    goto theend;

  p = result;

  i = shift;
  do {
    size_t digit = (offset >> ((i - 1) * 4)) & 0xF;
    *p++ = (digit < 0xA ? ('0' + digit) : ('A' + (digit - 0xA)));
  } while (--i);

  *p++ = ':';

  if (p0_result->error.message != NULL) {
    memcpy(p, "error:", 6);
    p += 6;
    STRCPY(p, p0_result->error.message);
  } else {
    (dump_as_expr ? expr_node_dump : node_repr)(&po, p0_result->node, &p);
  }

theend:
  free_test_expr_result(p0_result);
  return result;
}
