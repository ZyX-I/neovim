#include <stdbool.h>
#include <stddef.h>

#include "nvim/misc2.h"
#include "nvim/memory.h"
#include "nvim/vim.h"
#include "nvim/translator/parser/expressions.h"
#include "nvim/translator/printer/printer.h"

#ifdef INCLUDE_GENERATED_DECLARATIONS
# include "translator/printer/expressios.c.generated.h"
#endif

#define OP_SPACES {1, 1}
#define LOGICAL_OP_SPACES OP_SPACES
#define COMPARISON_OP_SPACES OP_SPACES
#define ARITHMETIC_OP_SPACES OP_SPACES
#define STRING_OP_SPACES ARITHMETIC_OP_SPACES
#define UNARY_OP_SPACES {0, 0}
#define TERNARY_OP_SPACES OP_SPACES
#define COMPLEX_LITERAL_SPACES {0, 0}
#define VALUE_SEPARATOR_SPACES {0, 1}
#define LIST_LITERAL_SPACES COMPLEX_LITERAL_SPACES
#define LIST_VALUE_SPACES VALUE_SEPARATOR_SPACES
#define DICT_LITERAL_SPACES COMPLEX_LITERAL_SPACES
#define DICT_VALUE_SPACES VALUE_SEPARATOR_SPACES
#define DICT_KEY_SPACES VALUE_SEPARATOR_SPACES
#define VARIABLE_SPACES {0, 0}
#define CURLY_NAME_SPACES VARIABLE_SPACES
#define FUNCTION_CALL_SPACES {0, 0}
#define ARGUMENT_SPACES VALUE_SEPARATOR_SPACES
#define INDENT "  "
#define LET_SPACES {1, 1}
#define SLICE_SPACES {1, 1}
#define INDEX_SPACES VARIABLE_SPACES
#define TRAILING_COMMA FALSE
#define LIST_TRAILING_COMMA TRAILING_COMMA
#define DICT_TRAILING_COMMA TRAILING_COMMA
#define FUNCTION_CMD_CALL_SPACES FUNCTION_CALL_SPACES
#define CMD_ARGUMENT_SPACES ARGUMENT_SPACES
#define ATTRIBUTE_SPACES 1
#define FUNCTION_SUB_SPACES 0
#define FUNCTION_CMD_SUB_SPACES FUNCTION_SUB_SPACES

const PrinterOptions default_po = {
  {
    {
      {
        LOGICAL_OP_SPACES,
        LOGICAL_OP_SPACES
      },
      {
        COMPARISON_OP_SPACES,
        COMPARISON_OP_SPACES,
        COMPARISON_OP_SPACES,
        COMPARISON_OP_SPACES,
        COMPARISON_OP_SPACES,
        COMPARISON_OP_SPACES,
        COMPARISON_OP_SPACES,
        COMPARISON_OP_SPACES,
        COMPARISON_OP_SPACES,
        COMPARISON_OP_SPACES
      },
      {
        ARITHMETIC_OP_SPACES,
        ARITHMETIC_OP_SPACES,
        ARITHMETIC_OP_SPACES,
        ARITHMETIC_OP_SPACES,
        ARITHMETIC_OP_SPACES
      },
      {
        STRING_OP_SPACES
      },
      {
        UNARY_OP_SPACES,
        UNARY_OP_SPACES,
        UNARY_OP_SPACES
      },
      {
        TERNARY_OP_SPACES,
        TERNARY_OP_SPACES
      }
    },
    {
      LIST_LITERAL_SPACES,
      LIST_VALUE_SPACES,
      LIST_TRAILING_COMMA
    },
    {
      DICT_LITERAL_SPACES,
      DICT_KEY_SPACES,
      DICT_VALUE_SPACES,
      DICT_TRAILING_COMMA
    },
    CURLY_NAME_SPACES,
    {
      SLICE_SPACES,
      INDEX_SPACES
    },
    {
      FUNCTION_SUB_SPACES,
      FUNCTION_CALL_SPACES,
      ARGUMENT_SPACES
    }
  },
  {
    INDENT,
    {
      LET_SPACES,
      LET_SPACES,
      LET_SPACES,
      LET_SPACES
    },
    {
      FUNCTION_CMD_SUB_SPACES,
      FUNCTION_CMD_CALL_SPACES,
      CMD_ARGUMENT_SPACES,
      ATTRIBUTE_SPACES
    }
  }
};

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

#include "nvim/translator/printer/expressions.c.h"

size_t expr_node_dump_len(const PrinterOptions *const po, ExpressionNode *node)
  FUNC_ATTR_NONNULL_ALL FUNC_ATTR_CONST
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

void expr_node_dump(const PrinterOptions *const po, ExpressionNode *node,
                    char **pp)
  FUNC_ATTR_NONNULL_ALL
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
  FUNC_ATTR_NONNULL_ALL FUNC_ATTR_WARN_UNUSED_RESULT
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
