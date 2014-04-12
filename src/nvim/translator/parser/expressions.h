// vim: ts=8 sts=2 sw=2 tw=80
//
// Copyright 2014 Nikolay Pavlov

#ifndef NEOVIM_TRANSLATOR_PARSER_EXPRESSIONS_H
#define NEOVIM_TRANSLATOR_PARSER_EXPRESSIONS_H

#include "nvim/types.h"

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
  kCCStrategyUseOption = 0,  // 0 for alloc_clear
  kCCStrategyMatchCase,
  kCCStrategyIgnoreCase,
} CaseCompareStrategy;

typedef struct expression_node {
  ExpressionType type;
  // Position of start inside a line.
  char_u *position;
  // Only valid for value nodes and kExprConcatOrSubscript.
  char_u *end_position;
  // Only valid for kExpr(Greater|Less)*, kExpr[Not]Matches, kExpr[Not]Equals: 
  // determines whether case should be ignored
  CaseCompareStrategy ignore_case;
  // Only valid for operators, subscripts (except for kExprConcatOrSubscript) 
  // and complex value nodes: represents operator arguments, nodes inside 
  // a list, …
  struct expression_node *children;
  // Subnodes are arranged in a linked list.
  struct expression_node *next;
} ExpressionNode;

typedef struct error {
  char *message;
  char_u *position;
} ExpressionParserError;

typedef struct {
  ExpressionNode *node;
  ExpressionParserError error;
  char_u *end;
} TestExprResult;

typedef ExpressionNode *(*ExpressionParser)(char_u **, ExpressionParserError *);

#ifdef INCLUDE_GENERATED_DECLARATIONS
# include "translator/parser/expressions.h.generated.h"
#endif

#endif  // NEOVIM_TRANSLATOR_PARSER_EXPRESSIONS_H
