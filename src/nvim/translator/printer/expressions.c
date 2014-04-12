#include <stdbool.h>
#include <stddef.h>

#include "nvim/misc2.h"
#include "nvim/translator/parser/expressions.h"
#include "nvim/vim.h"

#ifdef INCLUDE_GENERATED_DECLARATIONS
# include "translator/printer/expressios.c.generated.h"
#endif

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
  "..",
  "*",
  "/",
  "%",
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

#include "nvim/translator/printer/expressions.c.h"

size_t expr_node_dump_len(ExpressionNode *node)
  FUNC_ATTR_NONNULL_ALL FUNC_ATTR_PURE
{
  size_t len = node_dump_len(node);
  ExpressionNode *next = node->next;

  while (next != NULL) {
    len++;
    len += node_dump_len(next);
    next = next->next;
  }

  return len;
}

static size_t node_repr_len(ExpressionNode *node)
  FUNC_ATTR_NONNULL_ALL FUNC_ATTR_CONST
{
  size_t len = 0;

  len += STRLEN(expression_type_string[node->type]);
  len += STRLEN(case_compare_strategy_string[node->ignore_case]);

  if (node->position != NULL) {
    // 4 for [++] or [!!]
    len += 4;
    if (node->end_position != NULL)
      len += node->end_position - node->position + 1;
    else
      len++;
  }

  if (node->children != NULL)
    // 2 for parenthesis
    len += 2 + node_repr_len(node->children);

  if (node->next != NULL)
    // 2 for ", "
    len += 2 + node_repr_len(node->next);

  return len;
}

void expr_node_dump(ExpressionNode *node, char **pp)
{
  ExpressionNode *next = node->next;

  node_dump(node, pp);

  while (next != NULL) {
    *(*pp)++ = ' ';
    node_dump(next, pp);
    next = next->next;
  }
}

static void node_repr(ExpressionNode *node, char **pp)
  FUNC_ATTR_NONNULL_ALL
{
  char *p = *pp;

  STRCPY(p, expression_type_string[node->type]);
  p += STRLEN(expression_type_string[node->type]);
  STRCPY(p, case_compare_strategy_string[node->ignore_case]);
  p += STRLEN(case_compare_strategy_string[node->ignore_case]);

  if (node->position != NULL) {
    *p++ = '[';
    if (node->end_position != NULL) {
      size_t len = node->end_position - node->position + 1;

      *p++ = '+';

      if (node->type == kTypeRegister && *(node->end_position) == NUL)
        len--;

      memcpy((void *) p, node->position, len);
      p += len;

      *p++ = '+';
    } else {
      *p++ = '!';
      *p++ = *(node->position);
      *p++ = '!';
    }
    *p++ = ']';
  }

  if (node->children != NULL) {
    *p++ = '(';
    node_repr(node->children, &p);
    *p++ = ')';
  }

  if (node->next != NULL) {
    *p++ = ',';
    *p++ = ' ';
    node_repr(node->next, &p);
  }

  *pp = p;
}

char *parse0_repr(char_u *arg, bool dump_as_expr)
  FUNC_ATTR_NONNULL_ALL FUNC_ATTR_WARN_UNUSED_RESULT
{
  TestExprResult *p0_result;
  size_t len = 0;
  size_t shift = 0;
  size_t offset = 0;
  size_t i;
  char *result = NULL;
  char *p;

  if ((p0_result = parse0_test(arg)) == NULL)
    goto theend;

  if (p0_result->error.message != NULL)
    len = 6 + STRLEN(p0_result->error.message);
  else
    len = (dump_as_expr ? expr_node_dump_len : node_repr_len)(p0_result->node);

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
    (dump_as_expr ? expr_node_dump : node_repr)(p0_result->node, &p);
  }

theend:
  free_test_expr_result(p0_result);
  return result;
}
