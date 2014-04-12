#ifndef NEOVIM_TRANSLATOR_PRINTER_PRINTER_H
#define NEOVIM_TRANSLATOR_PRINTER_PRINTER_H

#include "translator/parser/expressions.h"

typedef struct {
  size_t before;
  size_t after;
} _BeforeAfterSpaces;

typedef struct {
  size_t after_start;
  size_t before_end;
} _StartEndSpaces;

#define OPERATOR_SPACES(po, op) \
    (LOGICAL_START <= op && op <= LOGICAL_END \
     ? po->expression.operators.logical[op - LOGICAL_START] \
     : (COMPARISON_START <= op && op <= COMPARISON_END \
        ? po->expression.operators.comparison[op - COMPARISON_START] \
        : (ARITHMETIC_START <= op && op <= ARITHMETIC_END \
           ? po->expression.operators.arithmetic[op - ARITHMETIC_START] \
           : (op == kTypeStringConcat \
              ? po->expression.operators.string.concat \
              : assert(FALSE)))))
typedef struct {
  struct {
    struct {
      _BeforeAfterSpaces logical[LOGICAL_LENGTH];
      _BeforeAfterSpaces comparison[COMPARISON_LENGTH];
      _BeforeAfterSpaces arithmetic[ARITHMETIC_LENGTH];
      struct {
        _BeforeAfterSpaces concat;
      } string;
      struct {
        _BeforeAfterSpaces not;
        _BeforeAfterSpaces minus;
        _BeforeAfterSpaces plus;
      } unary;
      struct {
        _BeforeAfterSpaces condition;
        _BeforeAfterSpaces values_separator;
      } ternary;
    } operators;
    struct {
      _StartEndSpaces braces;
      _BeforeAfterSpaces item_separator;
    } list;
    struct {
      _StartEndSpaces curly_braces;
      _BeforeAfterSpaces key_value_separator;
      _BeforeAfterSpaces item_separator;
    } dictionary;
    _StartEndSpaces curly_name;
    struct {
      _BeforeAfterSpaces slice;
      _StartEndSpaces brackets;
    } subscript;
    struct {
      size_t simple_call_separator;
      _BeforeAfterSpaces argument;
    } function_call;
  } expression;
  struct {
    char *indent;
    struct {
      _BeforeAfterSpaces assign;
      _BeforeAfterSpaces add;
      _BeforeAfterSpaces subtract;
      _BeforeAfterSpaces concat;
    } let;
  } command;
} PrinterOptions;

const PrinterOptions default_po;

#endif  // NEOVIM_TRANSLATOR_PRINTER_PRINTER_H
