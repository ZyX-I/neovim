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

      add_ccs = TRUE;
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
    case kExprTernaryConditional: {
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
    case kExprNot:
    case kExprMinus:
    case kExprPlus: {
      assert(node->children != NULL);
      assert(node->children->next == NULL);
      SPACES(OPERATOR_SPACES(node->type).before)
      ADD_CHAR(*(expression_type_string[node->type]));
      SPACES(OPERATOR_SPACES(node->type).after)
      F(node_dump, node->children);
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
        F(node_dump, child);
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
      SPACES((node->type == kExprCurlyName
              ? po->expression.curly_name
              : po->expression.function_call.call).after_start)
      F(node_dump, node->children);
      if (node->type == kExprCurlyName)
      SPACES((node->type == kExprCurlyName
              ? po->expression.curly_name
              : po->expression.function_call.call).before_end)
      ADD_CHAR((node->type == kExprExpression ? ')' : '}'));
      break;
    }
    case kExprList: {
      ExpressionNode *child = node->children;

      ADD_CHAR('[');
      SPACES(po->expression.list.braces.after_start)
      while (child != NULL) {
        F(node_dump, child);
        child = child->next;
        if (child != NULL || po->expression.list.trailing_comma) {
          SPACES(po->expression.list.item.before)
          ADD_CHAR(',');
          SPACES(po->expression.list.item.after)
        }
      }
      SPACES(po->expression.list.braces.before_end)
      ADD_CHAR(']');
      break;
    }
    case kExprDictionary: {
      ExpressionNode *child = node->children;
      ADD_CHAR('{');
      SPACES(po->expression.dictionary.curly_braces.after_start)
      while (child != NULL) {
        F(node_dump, child);
        child = child->next;
        assert(child != NULL);
        SPACES(po->expression.dictionary.key.before)
        ADD_CHAR(':');
        SPACES(po->expression.dictionary.key.after)
        F(node_dump, child);
        child = child->next;
        if (child != NULL || po->expression.dictionary.trailing_comma) {
          SPACES(po->expression.dictionary.item.before)
          ADD_CHAR(',');
          SPACES(po->expression.dictionary.item.after)
        }
      }
      SPACES(po->expression.dictionary.curly_braces.before_end)
      ADD_CHAR('}');
      break;
    }
    case kExprSubscript: {
      assert(node->children != NULL);
      assert(node->children->next != NULL);

      F(node_dump, node->children);
      ADD_CHAR('[');
      SPACES(po->expression.subscript.brackets.after_start)
      F(node_dump, node->children->next);
      if (node->children->next->next != NULL) {
        assert(node->children->next->next->next == NULL);
        SPACES(po->expression.subscript.slice.before)
        ADD_CHAR(':');
        SPACES(po->expression.subscript.slice.after)
        F(node_dump, node->children->next->next);
      }
      SPACES(po->expression.subscript.brackets.before_end)
      ADD_CHAR(']');
      break;
    }
    case kExprConcatOrSubscript: {
      size_t node_len = node->end_position - node->position + 1;

      assert(node->children != NULL);
      assert(node->children->next == NULL);

      F(node_dump, node->children);
      ADD_CHAR('.');
      ADD_STRING_LEN(node->position, node_len);
      break;
    }
    case kExprCall: {
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
    case kExprEmptySubscript: {
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
