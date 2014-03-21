// vim: ts=8 sts=2 sw=2 tw=80
//
// Copyright 2014 Nikolay Pavlov

#ifndef NEOVIM_EXPR_H
#define NEOVIM_EXPR_H

#include "nvim/types.h"

typedef enum {
  kTypeUnknown = 0,

  // Ternary operators
  kTypeTernaryConditional,    // ? :

  // Binary operators
  kTypeLogicalOr,             // ||
  kTypeLogicalAnd,            // &&
  kTypeGreater,               // >
  kTypeGreaterThanOrEqualTo,  // >=
  kTypeLess,                  // <
  kTypeLessThanOrEqualTo,     // <=
  kTypeEquals,                // ==
  kTypeNotEquals,             // !=
  kTypeIdentical,             // is
  kTypeNotIdentical,          // isnot
  kTypeMatches,               // =~
  kTypeNotMatches,            // !~
  kTypeAdd,                   // +
  kTypeSubtract,              // -
  kTypeStringConcat,          // .
  kTypeMultiply,              // *
  kTypeDivide,                // /
  kTypeModulo,                // %
  // 19

  // Unary operators
  kTypeNot,                   // !
  kTypeMinus,                 // -
  kTypePlus,                  // +
  // 22

  // Simple value nodes
  kTypeDecimalNumber,         // 0
  kTypeOctalNumber,           // 0123
  kTypeHexNumber,             // 0x1C
  kTypeFloat,                 // 0.0, 0.0e0
  kTypeDoubleQuotedString,    // "abc"
  kTypeSingleQuotedString,    // 'abc'
  kTypeOption,                // &option
  kTypeRegister,              // @r
  kTypeEnvironmentVariable,   // $VAR
  // 31

  // Curly braces names parts
  kTypeVariableName,          // Top-level part
  kTypeSimpleVariableName,    // Variable name without curly braces
  kTypeIdentifier,            // plain string part
  kTypeCurlyName,             // curly brace name
  // 35

  // Complex value nodes
  kTypeExpression,            // (expr)
  kTypeList,                  // [expr, ]
  kTypeDictionary,            // {expr : expr, }
  // 38

  // Subscripts
  kTypeSubscript,             // expr[expr:expr]
  kTypeConcatOrSubscript,     // expr.name
  kTypeCall,                  // expr(expr, )

  kTypeEmptySubscript,        // empty lhs or rhs in [lhs:rhs]
} ExpressionType;

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
  // Only valid for value nodes and kTypeConcatOrSubscript.
  char_u *end_position;
  // Only valid for kType(Greater|Less)*, kType[Not]Matches, kType[Not]Equals: 
  // determines whether case should be ignored
  CaseCompareStrategy ignore_case;
  // Only valid for operators, subscripts (except for kTypeConcatOrSubscript) 
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

#ifdef INCLUDE_GENERATED_DECLARATIONS
# include "expr.h.generated.h"
#endif

#endif  // NEOVIM_EXPR_H
