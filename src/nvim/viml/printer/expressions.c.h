#ifndef NVIM_VIML_PRINTER_EXPRESSIONS_C_H

#include <stdbool.h>
#include <stddef.h>

#include "nvim/vim.h"
#include "nvim/memory.h"

#include "nvim/viml/parser/expressions.h"
#include "nvim/viml/printer/printer.h"
#include "nvim/viml/dumpers/dumpers.h"

#ifdef INCLUDE_GENERATED_DECLARATIONS
# include "viml/printer/expressions.c.h.generated.h"
#endif

#if !defined(CH_MACROS_DEFINE_LENGTH) && !defined(CH_MACROS_DEFINE_FWRITE)
# define CH_MACROS_DEFINE_LENGTH
# include "nvim/viml/printer/expressions.c.h"
# undef CH_MACROS_DEFINE_LENGTH
#elif !defined(CH_MACROS_DEFINE_FWRITE)
# undef CH_MACROS_DEFINE_LENGTH
# define CH_MACROS_DEFINE_FWRITE
# include "nvim/viml/printer/expressions.c.h"
# undef CH_MACROS_DEFINE_FWRITE
# define CH_MACROS_DEFINE_LENGTH
#endif
#define NVIM_VIML_PRINTER_EXPRESSIONS_C_H

#define CH_MACROS_OPTIONS_TYPE const PrinterOptions *const
#include "nvim/viml/dumpers/ch_macros.h"

static FDEC(print_node, const ExpressionNode *const node)
{
  FUNCTION_START;
  bool add_ccs = false;

  switch (node->type) {
    case kExprGreater:
    case kExprGreaterThanOrEqualTo:
    case kExprLess:
    case kExprLessThanOrEqualTo:
    case kExprEquals:
    case kExprNotEquals:
    case kExprIdentical:
    case kExprNotIdentical:
    case kExprMatches:
    case kExprNotMatches: {
      assert(node->children->next->next == NULL);

      add_ccs = true;
      // fallthrough
    }
    case kExprLogicalOr:
    case kExprLogicalAnd:
    case kExprAdd:
    case kExprSubtract:
    case kExprMultiply:
    case kExprDivide:
    case kExprModulo:
    case kExprStringConcat: {
      ExpressionNode *child = node->children;
      char *operator;

      assert(node->children != NULL);
      assert(node->children->next != NULL);

      if (node->type == kExprStringConcat)
        operator = ".";
      else
        operator = expression_type_string[node->type];

      child = node->children;
      do {
        F(print_node, child);
        child = child->next;
        if (child != NULL) {
          SPACES(OPERATOR_SPACES(node->type).before);
          ADD_STRING(operator);
          if (add_ccs)
            if (node->ignore_case)
              ADD_CHAR(*(case_compare_strategy_string[node->ignore_case]));
          SPACES(OPERATOR_SPACES(node->type).after);
        }
      } while (child != NULL);

      break;
    }
    case kExprTernaryConditional: {
      assert(node->children != NULL);
      assert(node->children->next != NULL);
      assert(node->children->next->next != NULL);
      assert(node->children->next->next->next == NULL);

      F(print_node, node->children);
      SPACES_BEFORE4(expression, operators, ternary, condition);
      ADD_CHAR('?');
      SPACES_AFTER4(expression, operators, ternary, condition);
      F(print_node, node->children->next);
      SPACES_BEFORE4(expression, operators, ternary, values);
      ADD_CHAR(':');
      SPACES_AFTER4(expression, operators, ternary, values);
      F(print_node, node->children->next->next);
      break;
    }
    case kExprNot:
    case kExprMinus:
    case kExprPlus: {
      assert(node->children != NULL);
      assert(node->children->next == NULL);
      SPACES(OPERATOR_SPACES(node->type).before);
      ADD_CHAR(*(expression_type_string[node->type]));
      SPACES(OPERATOR_SPACES(node->type).after);
      F(print_node, node->children);
      break;
    }
    case kExprEnvironmentVariable:
    case kExprOption: {
      ADD_CHAR((node->type == kExprEnvironmentVariable ? '$' : '&'));
      // fallthrough
    }
    case kExprDecimalNumber:
    case kExprOctalNumber:
    case kExprHexNumber:
    case kExprFloat:
    case kExprDoubleQuotedString:
    case kExprSingleQuotedString:
    case kExprRegister:
    case kExprSimpleVariableName:
    case kExprIdentifier: {
      size_t node_len = node->end_position - node->position + 1;

      assert(node->position != NULL);
      assert(node->end_position != NULL);
      assert(node->children == NULL);

      ADD_STRING_LEN(node->position, node_len);
      break;
    }
    case kExprVariableName: {
      ExpressionNode *child = node->children;

      assert(child != NULL);

      do {
        F(print_node, child);
        child = child->next;
      } while (child != NULL);
      break;
    }
    case kExprCurlyName:
    case kExprExpression: {
      assert(node->children != NULL);
      assert(node->children->next == NULL);

      ADD_CHAR((node->type == kExprExpression ? '(' : '{'));
      if (node->type == kExprCurlyName)
        SPACES_AFTER_START2(expression, curly_name);
      else
        SPACES_AFTER_START3(expression, function_call, call);
      F(print_node, node->children);
      if (node->type == kExprCurlyName)
        SPACES_BEFORE_END2(expression, curly_name);
      else
        SPACES_BEFORE_END3(expression, function_call, call);
      ADD_CHAR((node->type == kExprExpression ? ')' : '}'));
      break;
    }
    case kExprList: {
      ExpressionNode *child = node->children;

      ADD_CHAR('[');
      SPACES_AFTER_START3(expression, list, braces);
      while (child != NULL) {
        F(print_node, child);
        child = child->next;
        if (child != NULL || ADD_TRAILING_COMMA2(expression, list)) {
          SPACES_BEFORE3(expression, list, item);
          if (child != NULL && child->type == kExprListRest) {
            assert(child->children != NULL);
            assert(child->next == NULL);
            assert(child->children->next == NULL);
            child = child->children;
            ADD_CHAR(';');
          } else {
            ADD_CHAR(',');
          }
          SPACES_AFTER3(expression, list, item);
        }
      }
      SPACES_BEFORE_END3(expression, list, braces);
      ADD_CHAR(']');
      break;
    }
    case kExprDictionary: {
      ExpressionNode *child = node->children;
      ADD_CHAR('{');
      SPACES_AFTER_START3(expression, dictionary, curly_braces);
      while (child != NULL) {
        F(print_node, child);
        child = child->next;
        assert(child != NULL);
        SPACES_BEFORE3(expression, dictionary, key);
        ADD_CHAR(':');
        SPACES_AFTER3(expression, dictionary, key);
        F(print_node, child);
        child = child->next;
        if (child != NULL || ADD_TRAILING_COMMA2(expression, dictionary)) {
          SPACES_BEFORE3(expression, dictionary, item);
          ADD_CHAR(',');
          SPACES_AFTER3(expression, dictionary, item);
        }
      }
      SPACES_BEFORE_END3(expression, dictionary, curly_braces);
      ADD_CHAR('}');
      break;
    }
    case kExprSubscript: {
      assert(node->children != NULL);
      assert(node->children->next != NULL);

      F(print_node, node->children);
      ADD_CHAR('[');
      SPACES_AFTER_START3(expression, subscript, brackets);
      F(print_node, node->children->next);
      if (node->children->next->next != NULL) {
        assert(node->children->next->next->next == NULL);
        SPACES_BEFORE3(expression, subscript, slice);
        ADD_CHAR(':');
        SPACES_AFTER3(expression, subscript, slice);
        F(print_node, node->children->next->next);
      }
      SPACES_BEFORE_END3(expression, subscript, brackets);
      ADD_CHAR(']');
      break;
    }
    case kExprConcatOrSubscript: {
      size_t node_len = node->end_position - node->position + 1;

      assert(node->children != NULL);
      assert(node->children->next == NULL);

      F(print_node, node->children);
      ADD_CHAR('.');
      ADD_STRING_LEN(node->position, node_len);
      break;
    }
    case kExprCall: {
      ExpressionNode *child;

      assert(node->children != NULL);

      F(print_node, node->children);
      ADD_CHAR('(');
      child = node->children->next;
      while (child != NULL) {
        F(print_node, child);
        child = child->next;
        if (child != NULL) {
          ADD_CHAR(',');
          ADD_CHAR(' ');
        }
      }
      ADD_CHAR(')');
      break;
    }
    case kExprEmptySubscript: {
      break;
    }
    default: {
      assert(false);
    }
  }

  FUNCTION_END;
}

static FDEC(represent_node, const ExpressionNode *const node)
{
  FUNCTION_START;

  ADD_STRING(expression_type_string[node->type]);
  ADD_STRING(case_compare_strategy_string[node->ignore_case]);

  if (node->position != NULL) {
    ADD_CHAR('[');
    if (node->end_position != NULL) {
      size_t node_len = node->end_position - node->position + 1;

      ADD_CHAR('+');

      if (node->type == kExprRegister && *(node->end_position) == NUL)
        node_len--;

      ADD_STRING_LEN(node->position, node_len);

      ADD_CHAR('+');
    } else {
      ADD_CHAR('!');
      ADD_CHAR(*(node->position));
      ADD_CHAR('!');
    }
    ADD_CHAR(']');
  }

  if (node->children != NULL) {
    ADD_CHAR('(');
    F(represent_node, node->children);
    ADD_CHAR(')');
  }

  if (node->next != NULL) {
    ADD_STATIC_STRING(", ");
    F(represent_node, node->next);
  }

  FUNCTION_END;
}

#undef CH_MACROS_OPTIONS_TYPE

#endif  // NVIM_VIML_PRINTER_EXPRESSIONS_C_H
