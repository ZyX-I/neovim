#ifndef NEOVIM_TRANSLATOR_PRINTER_EXPRESSIONS_C_H

#include "nvim/translator/parser/expressions.h"

#include "nvim/translator/printer/ch_macros.h"

// {{{ Function declarations
static FDEC(node_dump, ExpressionNode *node);
static FDEC(node_repr, ExpressionNode *node);
// }}}

#ifndef DEFINE_LENGTH
# define DEFINE_LENGTH
# include "nvim/translator/printer/expressions.c.h"
# undef DEFINE_LENGTH
#endif
#define NEOVIM_TRANSLATOR_PRINTER_EXPRESSIONS_C_H

#include "nvim/translator/printer/ch_macros.h"

static FDEC(node_dump, ExpressionNode *node)
{
  FUNCTION_START;
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
      assert(node->children->next->next == NULL);

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
      char *operator;

      assert(node->children != NULL);
      assert(node->children->next != NULL);

      if (node->type == kTypeStringConcat)
        operator = ".";
      else
        operator = expression_type_string[node->type];

      child = node->children;
      do {
        F(node_dump, child);
        child = child->next;
        if (child != NULL) {
          SPACES(OPERATOR_SPACES(node->type).before)
          ADD_STRING(operator);
          if (add_ccs)
            if (node->ignore_case)
              ADD_CHAR(*(case_compare_strategy_string[node->ignore_case]));
          SPACES(OPERATOR_SPACES(node->type).after)
        }
      } while (child != NULL);

      break;
    }
    case kTypeTernaryConditional: {
      assert(node->children != NULL);
      assert(node->children->next != NULL);
      assert(node->children->next->next != NULL);
      assert(node->children->next->next->next == NULL);

      F(node_dump, node->children);
      SPACES(po->expression.operators.ternary.condition.before)
      ADD_CHAR('?');
      SPACES(po->expression.operators.ternary.condition.after)
      F(node_dump, node->children->next);
      SPACES(po->expression.operators.ternary.values.before)
      ADD_CHAR(':');
      SPACES(po->expression.operators.ternary.values.after)
      F(node_dump, node->children->next->next);
      break;
    }
    case kTypeNot:
    case kTypeMinus:
    case kTypePlus: {
      assert(node->children != NULL);
      assert(node->children->next == NULL);
      SPACES(OPERATOR_SPACES(node->type).before)
      ADD_CHAR(*(expression_type_string[node->type]));
      SPACES(OPERATOR_SPACES(node->type).after)
      F(node_dump, node->children);
      break;
    }
    case kTypeEnvironmentVariable:
    case kTypeOption: {
      ADD_CHAR((node->type == kTypeEnvironmentVariable ? '$' : '&'));
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
      size_t node_len = node->end_position - node->position + 1;

      assert(node->position != NULL);
      assert(node->end_position != NULL);
      assert(node->children == NULL);

      ADD_STRING_LEN(node->position, node_len);
      break;
    }
    case kTypeVariableName: {
      ExpressionNode *child = node->children;

      assert(child != NULL);

      do {
        F(node_dump, child);
        child = child->next;
      } while (child != NULL);
      break;
    }
    case kTypeCurlyName:
    case kTypeExpression: {
      assert(node->children != NULL);
      assert(node->children->next == NULL);

      ADD_CHAR((node->type == kTypeExpression ? '(' : '{'));
      if (node->type == kTypeCurlyName)
        SPACES(po->expression.curly_name.after_start)
      F(node_dump, node->children);
      if (node->type == kTypeCurlyName)
        SPACES(po->expression.curly_name.before_end)
      ADD_CHAR((node->type == kTypeExpression ? ')' : '}'));
      break;
    }
    case kTypeList: {
      ExpressionNode *child = node->children;

      ADD_CHAR('[');
      while (child != NULL) {
        F(node_dump, child);
        child = child->next;
        if (child != NULL) {
          ADD_CHAR(',');
          ADD_CHAR(' ');
        }
      }
      ADD_CHAR(']');
      break;
    }
    case kTypeDictionary: {
      ExpressionNode *child = node->children;
      ADD_CHAR('{');
      while (child != NULL) {
        F(node_dump, child);
        child = child->next;
        assert(child != NULL);
        ADD_CHAR(' ');
        ADD_CHAR(':');
        ADD_CHAR(' ');
        F(node_dump, child);
        child = child->next;
        if (child != NULL) {
          ADD_CHAR(',');
          ADD_CHAR(' ');
        }
      }
      ADD_CHAR('}');
      break;
    }
    case kTypeSubscript: {
      assert(node->children != NULL);
      assert(node->children->next != NULL);

      F(node_dump, node->children);
      ADD_CHAR('[');
      F(node_dump, node->children->next);
      if (node->children->next->next != NULL) {
        assert(node->children->next->next->next == NULL);
        ADD_CHAR(' ');
        ADD_CHAR(':');
        ADD_CHAR(' ');
        F(node_dump, node->children->next->next);
      }
      ADD_CHAR(']');
      break;
    }
    case kTypeConcatOrSubscript: {
      size_t node_len = node->end_position - node->position + 1;

      assert(node->children != NULL);
      assert(node->children->next == NULL);

      F(node_dump, node->children);
      ADD_CHAR('.');
      ADD_STRING_LEN(node->position, node_len);
      break;
    }
    case kTypeCall: {
      ExpressionNode *child;

      assert(node->children != NULL);

      F(node_dump, node->children);
      ADD_CHAR('(');
      child = node->children->next;
      while (child != NULL) {
        F(node_dump, child);
        child = child->next;
        if (child != NULL) {
          ADD_CHAR(',');
          ADD_CHAR(' ');
        }
      }
      ADD_CHAR(')');
      break;
    }
    case kTypeEmptySubscript: {
      break;
    }
    default: {
      assert(FALSE);
    }
  }

  FUNCTION_END;
}

static FDEC(node_repr, ExpressionNode *node)
{
  FUNCTION_START;

  ADD_STRING(expression_type_string[node->type]);
  ADD_STRING(case_compare_strategy_string[node->ignore_case]);

  if (node->position != NULL) {
    ADD_CHAR('[');
    if (node->end_position != NULL) {
      size_t node_len = node->end_position - node->position + 1;

      ADD_CHAR('+');

      if (node->type == kTypeRegister && *(node->end_position) == NUL)
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
    F(node_repr, node->children);
    ADD_CHAR(')');
  }

  if (node->next != NULL) {
    ADD_STATIC_STRING(", ");
    F(node_repr, node->next);
  }

  FUNCTION_END;
}

#endif  // NEOVIM_TRANSLATOR_PRINTER_EXPRESSIONS_C_H
