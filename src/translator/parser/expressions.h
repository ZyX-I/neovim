// vim: ts=8 sts=2 sw=2 tw=80
//
// Copyright 2014 Nikolay Pavlov

#ifndef NEOVIM_TRANSLATOR_PARSER_EXPRESSIONS_H
#define NEOVIM_TRANSLATOR_PARSER_EXPRESSIONS_H

#include "types.h"

typedef enum {
  kExprUnknown = 0,

  // Ternary operators
  kExprTernaryConditional,    // ? :

  // Binary operators
#define LOGICAL_START kExprLogicalOr
  kExprLogicalOr,             // ||
  kExprLogicalAnd,            // &&
#define LOGICAL_END kExprLogicalAnd
#define COMPARISON_START kExprGreater
  kExprGreater,               // >
  kExprGreaterThanOrEqualTo,  // >=
  kExprLess,                  // <
  kExprLessThanOrEqualTo,     // <=
  kExprEquals,                // ==
  kExprNotEquals,             // !=
  kExprIdentical,             // is
  kExprNotIdentical,          // isnot
  kExprMatches,               // =~
  kExprNotMatches,            // !~
#define COMPARISON_END kExprNotMatches
#define ARITHMETIC_START kExprAdd
  kExprAdd,                   // +
  kExprSubtract,              // -
  kExprMultiply,              // *
  kExprDivide,                // /
  kExprModulo,                // %
#define ARITHMETIC_END kExprModulo
  kExprStringConcat,          // .
  // 19

  // Unary operators
#define UNARY_START kExprNot
  kExprNot,                   // !
  kExprMinus,                 // -
  kExprPlus,                  // +
#define UNARY_END kExprPlus
  // 22

  // Simple value nodes
  kExprDecimalNumber,         // 0
  kExprOctalNumber,           // 0123
  kExprHexNumber,             // 0x1C
  kExprFloat,                 // 0.0, 0.0e0
  kExprDoubleQuotedString,    // "abc"
  kExprSingleQuotedString,    // 'abc'
  kExprOption,                // &option
  kExprRegister,              // @r
  kExprEnvironmentVariable,   // $VAR
  // 31

  // Curly braces names parts
  kExprVariableName,          // Top-level part
  kExprSimpleVariableName,    // Variable name without curly braces
  kExprIdentifier,            // plain string part
  kExprCurlyName,             // curly brace name
  // 35

  // Complex value nodes
  kExprExpression,            // (expr)
  kExprList,                  // [expr, ]
  kExprDictionary,            // {expr : expr, }
  // 38

  // Subscripts
  kExprSubscript,             // expr[expr:expr]
  kExprConcatOrSubscript,     // expr.name
  kExprCall,                  // expr(expr, )

  kExprEmptySubscript,        // empty lhs or rhs in [lhs:rhs]

  kExprListRest,              // Node after ";" in lval lists
} ExpressionType;

#define LOGICAL_LENGTH (LOGICAL_END - LOGICAL_START + 1)
#define COMPARISON_LENGTH (COMPARISON_END - COMPARISON_START + 1)
#define ARITHMETIC_LENGTH (ARITHMETIC_END - ARITHMETIC_START + 1)
#define UNARY_LENGTH (UNARY_END - UNARY_START + 1)

// Defines whether to ignore case:
//    ==   kCCStrategyUseOption
//    ==#  kCCStrategyMatchCase
//    ==?  kCCStrategyIgnoreCase
typedef enum {
  kCCStrategyUseOption = 0,  // 0 for xcalloc
  kCCStrategyMatchCase,
  kCCStrategyIgnoreCase,
} CaseCompareStrategy;

/// Structure to represent VimL expressions
typedef struct expression_node {
  ExpressionType type;   ///< Node type.
  char_u *position;      ///< Position of expression token start inside
                         ///< a parsed string.
  char_u *end_position;  ///< Position of last character of expression token.
  CaseCompareStrategy ignore_case;  ///< Determines whether case should be 
                                    ///< ignored while comparing. Only valid 
                                    ///< for comparison operators: 
                                    ///< kExpr(Greater|Less)*, 
                                    ///< kExpr[Not]Matches, kExpr[Not]Equals.
  struct expression_node *children;  ///< Subexpressions: valid for operators,
                                     ///< subscripts (including kExprCall), 
                                     ///< complex variable names.
  struct expression_node *next;  ///< Next node: expression nodes are arranged 
                                 ///< as a linked list.
} ExpressionNode;

typedef struct error {
  char *message;
  char_u *position;
} ExpressionParserError;

typedef ExpressionNode *(*ExpressionParser)(char_u **, ExpressionParserError *);

ExpressionNode *parse0_err(char_u **arg, ExpressionParserError *error);
void free_expr(ExpressionNode *node);
ExpressionNode *parse7_nofunc(char_u **arg, ExpressionParserError *error);
ExpressionNode *parse_mult(char_u **arg, ExpressionParserError *error,
                           ExpressionParser parse, bool listends,
                           char_u *endwith);

#endif  // NEOVIM_TRANSLATOR_PARSER_EXPRESSIONS_H
