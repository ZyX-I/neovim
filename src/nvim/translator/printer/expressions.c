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
#define IS_SPACES OP_SPACES
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
#define TRAILING_COMMA false
#define LIST_TRAILING_COMMA TRAILING_COMMA
#define DICT_TRAILING_COMMA TRAILING_COMMA
#define FUNCTION_CMD_CALL_SPACES FUNCTION_CALL_SPACES
#define CMD_ARGUMENT_SPACES ARGUMENT_SPACES
#define ATTRIBUTE_SPACES 1
#define FUNCTION_SUB_SPACES 0
#define FUNCTION_CMD_SUB_SPACES FUNCTION_SUB_SPACES
#define COMMENT_INLINE_SPACES 2
#define COMMENT_SPACES 0

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
        IS_SPACES,
        IS_SPACES,
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
    },
    {
      COMMENT_INLINE_SPACES,
      COMMENT_SPACES
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
  ";",
};

static char *case_compare_strategy_string[] = {
  "",
  "#",
  "?",
};

#include "nvim/translator/printer/expressions.c.h"

size_t expr_node_dump_len(const PrinterOptions *const po,
                          const ExpressionNode *const node)
  FUNC_ATTR_NONNULL_ALL FUNC_ATTR_CONST
{
  size_t len = node_dump_len(po, (ExpressionNode *) node);
  ExpressionNode *next = node->next;

  while (next != NULL) {
    len++;
    len += node_dump_len(po, (ExpressionNode *) next);
    next = next->next;
  }

  return len;
}

void expr_node_dump(const PrinterOptions *const po,
                    const ExpressionNode *const node,
                    char **pp)
  FUNC_ATTR_NONNULL_ALL
{
  ExpressionNode *next = node->next;

  node_dump(po, (ExpressionNode *) node, pp);

  while (next != NULL) {
    *(*pp)++ = ' ';
    node_dump(po, (ExpressionNode *) next, pp);
    next = next->next;
  }
}

size_t expr_node_repr_len(const PrinterOptions *const po,
                          const ExpressionNode *const node)
  FUNC_ATTR_NONNULL_ALL FUNC_ATTR_CONST
{
  return node_repr_len(po, (ExpressionNode *) node);
}

void expr_node_repr(const PrinterOptions *const po,
                    const ExpressionNode *const node,
                    char **pp)
  FUNC_ATTR_NONNULL_ALL
{
  node_repr(po, (ExpressionNode *) node, pp);
}
