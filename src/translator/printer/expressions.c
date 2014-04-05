#include <stdbool.h>
#include <stddef.h>

#include "vim.h"
#include "memory.h"
#include "translator/parser/expressions.h"

// {{{ Function declarations
static size_t node_dump_len(ExpressionNode *node);
static size_t node_repr_len(ExpressionNode *node);
static void node_dump(ExpressionNode *node, char **pp);
static void node_repr(ExpressionNode *node, char **pp);
// }}}

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

static size_t node_dump_len(ExpressionNode *node)
{
  size_t len = 0;

  switch (node->type) {
    case kTypeGreater:
    case kTypeGreaterThanOrEqualTo:
    case kTypeLess:
    case kTypeLessThanOrEqualTo:
    case kTypeEquals:
    case kTypeNotEquals:
    case kTypeIdentical:
    case kTypeNotIdentical:
    case kTypeMatches:
    case kTypeNotMatches: {
      len += STRLEN(case_compare_strategy_string[node->ignore_case]);
      assert(node->children != NULL);
      assert(node->children->next != NULL);
      assert(node->children->next->next == NULL);
      // fallthrough
    }
    case kTypeLogicalOr:
    case kTypeLogicalAnd:
    case kTypeAdd:
    case kTypeSubtract:
    case kTypeMultiply:
    case kTypeDivide:
    case kTypeModulo:
    case kTypeStringConcat: {
      ExpressionNode *child = node->children;
      size_t operator_len;

      if (node->type == kTypeStringConcat)
        operator_len = 1 + 2;
      else
        operator_len = STRLEN(expression_type_string[node->type]) + 2;

      assert(node->children != NULL);
      assert(node->children->next != NULL);
      child = node->children;
      do {
        len += node_dump_len(child);
        child = child->next;
        if (child != NULL)
          len += operator_len;
      } while (child != NULL);

      break;
    }
    case kTypeTernaryConditional: {
      assert(node->children != NULL);
      assert(node->children->next != NULL);
      assert(node->children->next->next != NULL);
      assert(node->children->next->next->next == NULL);
      len += node_dump_len(node->children);
      len += 3;
      len += node_dump_len(node->children->next);
      len += 3;
      len += node_dump_len(node->children->next->next);
      break;
    }
    case kTypeNot:
    case kTypeMinus:
    case kTypePlus: {
      assert(node->children != NULL);
      assert(node->children->next == NULL);
      len += 1;
      len += node_dump_len(node->children);
      break;
    }
    case kTypeEnvironmentVariable:
    case kTypeOption: {
      len++;
      // fallthrough
    }
    case kTypeDecimalNumber:
    case kTypeOctalNumber:
    case kTypeHexNumber:
    case kTypeFloat:
    case kTypeDoubleQuotedString:
    case kTypeSingleQuotedString:
    case kTypeRegister:
    case kTypeSimpleVariableName:
    case kTypeIdentifier: {
      assert(node->position != NULL);
      assert(node->end_position != NULL);
      assert(node->children == NULL);
      len += node->end_position - node->position + 1;
      break;
    }
    case kTypeVariableName: {
      ExpressionNode *child = node->children;
      assert(child != NULL);
      do {
        len += node_dump_len(child);
        child = child->next;
      } while (child != NULL);
      break;
    }
    case kTypeCurlyName:
    case kTypeExpression: {
      assert(node->children != NULL);
      assert(node->children->next == NULL);
      len += 1;
      len += node_dump_len(node->children);
      len += 1;
      break;
    }
    case kTypeList: {
      ExpressionNode *child = node->children;
      len += 1;
      while (child != NULL) {
        len += node_dump_len(child);
        child = child->next;
        if (child != NULL)
          len += 2;
      }
      len += 1;
      break;
    }
    case kTypeDictionary: {
      ExpressionNode *child = node->children;
      len += 1;
      while (child != NULL) {
        len += node_dump_len(child);
        child = child->next;
        assert(child != NULL);
        len += 3;
        len += node_dump_len(child);
        child = child->next;
        if (child != NULL)
          len += 2;
      }
      len += 1;
      break;
    }
    case kTypeSubscript: {
      assert(node->children != NULL);
      assert(node->children->next != NULL);
      len += node_dump_len(node->children);
      len += 1;
      len += node_dump_len(node->children->next);
      if (node->children->next->next != NULL) {
        assert(node->children->next->next->next == NULL);
        len += 3;
        len += node_dump_len(node->children->next->next);
      }
      len += 1;
      break;
    }
    case kTypeConcatOrSubscript: {
      assert(node->children != NULL);
      assert(node->children->next == NULL);
      len += node_dump_len(node->children);
      len += 1;
      len += node->end_position - node->position + 1;
      break;
    }
    case kTypeCall: {
      ExpressionNode *child;

      assert(node->children != NULL);
      len += node_dump_len(node->children);
      len += 1;
      child = node->children->next;
      while (child != NULL) {
        len += node_dump_len(child);
        child = child->next;
        if (child != NULL)
          len += 2;
      }
      len += 1;
      break;
    }
    case kTypeEmptySubscript: {
      break;
    }
    default: {
      assert(FALSE);
    }
  }
  return len;
}

size_t expr_node_dump_len(ExpressionNode *node)
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

static void node_dump(ExpressionNode *node, char **pp)
{
  char *p = *pp;
  bool add_ccs = FALSE;

  switch (node->type) {
    case kTypeGreater:
    case kTypeGreaterThanOrEqualTo:
    case kTypeLess:
    case kTypeLessThanOrEqualTo:
    case kTypeEquals:
    case kTypeNotEquals:
    case kTypeIdentical:
    case kTypeNotIdentical:
    case kTypeMatches:
    case kTypeNotMatches: {
      add_ccs = TRUE;
      // fallthrough
    }
    case kTypeLogicalOr:
    case kTypeLogicalAnd:
    case kTypeAdd:
    case kTypeSubtract:
    case kTypeMultiply:
    case kTypeDivide:
    case kTypeModulo:
    case kTypeStringConcat: {
      ExpressionNode *child = node->children;
      size_t operator_len;
      char *operator;

      if (node->type == kTypeStringConcat)
        operator = ".";
      else
        operator = expression_type_string[node->type];
      operator_len = STRLEN(operator);

      child = node->children;
      do {
        node_dump(child, &p);
        child = child->next;
        if (child != NULL) {
          *p++ = ' ';
          memcpy(p, operator, operator_len);
          p += operator_len;
          *p++ = ' ';
          if (add_ccs)
            if (node->ignore_case)
              *p++ = *(case_compare_strategy_string[node->ignore_case]);
        }
      } while (child != NULL);

      break;
    }
    case kTypeTernaryConditional: {
      node_dump(node->children, &p);
      *p++ = ' ';
      *p++ = '?';
      *p++ = ' ';
      node_dump(node->children->next, &p);
      *p++ = ' ';
      *p++ = ':';
      *p++ = ' ';
      node_dump(node->children->next->next, &p);
      break;
    }
    case kTypeNot:
    case kTypeMinus:
    case kTypePlus: {
      *p++ = *(expression_type_string[node->type]);
      node_dump(node->children, &p);
      break;
    }
    case kTypeEnvironmentVariable:
    case kTypeOption: {
      *p++ = (node->type == kTypeEnvironmentVariable ? '$' : '&');
      // fallthrough
    }
    case kTypeDecimalNumber:
    case kTypeOctalNumber:
    case kTypeHexNumber:
    case kTypeFloat:
    case kTypeDoubleQuotedString:
    case kTypeSingleQuotedString:
    case kTypeRegister:
    case kTypeSimpleVariableName:
    case kTypeIdentifier: {
      size_t len = node->end_position - node->position + 1;
      memcpy(p, node->position, len);
      p += len;
      break;
    }
    case kTypeVariableName: {
      ExpressionNode *child = node->children;
      do {
        node_dump(child, &p);
        child = child->next;
      } while (child != NULL);
      break;
    }
    case kTypeCurlyName:
    case kTypeExpression: {
      *p++ = (node->type == kTypeExpression ? '(' : '{');
      node_dump(node->children, &p);
      *p++ = (node->type == kTypeExpression ? ')' : '}');
      break;
    }
    case kTypeList: {
      ExpressionNode *child = node->children;
      *p++ = '[';
      while (child != NULL) {
        node_dump(child, &p);
        child = child->next;
        if (child != NULL) {
          *p++ = ',';
          *p++ = ' ';
        }
      }
      *p++ = ']';
      break;
    }
    case kTypeDictionary: {
      ExpressionNode *child = node->children;
      *p++ = '{';
      while (child != NULL) {
        node_dump(child, &p);
        child = child->next;
        *p++ = ' ';
        *p++ = ':';
        *p++ = ' ';
        node_dump(child, &p);
        child = child->next;
        if (child != NULL) {
          *p++ = ',';
          *p++ = ' ';
        }
      }
      *p++ = '}';
      break;
    }
    case kTypeSubscript: {
      node_dump(node->children, &p);
      *p++ = '[';
      node_dump(node->children->next, &p);
      if (node->children->next->next != NULL) {
        *p++ = ' ';
        *p++ = ':';
        *p++ = ' ';
        node_dump(node->children->next->next, &p);
      }
      *p++ = ']';
      break;
    }
    case kTypeConcatOrSubscript: {
      size_t len = node->end_position - node->position + 1;
      node_dump(node->children, &p);
      *p++ = '.';
      memcpy(p, node->position, len);
      p += len;
      break;
    }
    case kTypeCall: {
      ExpressionNode *child;

      node_dump(node->children, &p);
      *p++ = '(';
      child = node->children->next;
      while (child != NULL) {
        node_dump(child, &p);
        child = child->next;
        if (child != NULL) {
          *p++ = ',';
          *p++ = ' ';
        }
      }
      *p++ = ')';
      break;
    }
    case kTypeEmptySubscript: {
      break;
    }
    default: {
      assert(FALSE);
    }
  }

  *pp = p;
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
