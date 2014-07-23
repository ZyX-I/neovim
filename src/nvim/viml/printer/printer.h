#ifndef NVIM_VIML_PRINTER_PRINTER_H
#define NVIM_VIML_PRINTER_PRINTER_H

#include "nvim/viml/parser/expressions.h"

typedef struct {
  size_t before;
  size_t after;
} _BeforeAfterSpaces;

typedef struct {
  size_t after_start;
  size_t before_end;
} _StartEndSpaces;

// XXX Never defined: not needed: it should crash in case branch with 
//     _error_spaces is reached.
const _BeforeAfterSpaces _error_spaces;

#define _OPERATOR_SPACES(po, op) \
    (LOGICAL_START <= op && op <= LOGICAL_END \
     ? po->expression.operators.logical[op - LOGICAL_START] \
     : (COMPARISON_START <= op && op <= COMPARISON_END \
        ? po->expression.operators.comparison[op - COMPARISON_START] \
        : (ARITHMETIC_START <= op && op <= ARITHMETIC_END \
           ? po->expression.operators.arithmetic[op - ARITHMETIC_START] \
           : (op == kExprStringConcat \
              ? po->expression.operators.string.concat \
              : (UNARY_START <= op && op <= UNARY_END \
                 ? po->expression.operators.unary[op - UNARY_START] \
                 : (assert(false), _error_spaces))))))
typedef struct {
  struct {
    struct {
      _BeforeAfterSpaces logical[LOGICAL_LENGTH];
      _BeforeAfterSpaces comparison[COMPARISON_LENGTH];
      _BeforeAfterSpaces arithmetic[ARITHMETIC_LENGTH];
      struct {
        _BeforeAfterSpaces concat;
      } string;
      _BeforeAfterSpaces unary[UNARY_LENGTH];
      struct {
        _BeforeAfterSpaces condition;
        _BeforeAfterSpaces values;
      } ternary;
    } operators;
    struct {
      _StartEndSpaces braces;
      _BeforeAfterSpaces item;
      bool trailing_comma;
    } list;
    struct {
      _StartEndSpaces curly_braces;
      _BeforeAfterSpaces key;
      _BeforeAfterSpaces item;
      bool trailing_comma;
    } dictionary;
    _StartEndSpaces curly_name;
    struct {
      _BeforeAfterSpaces slice;
      _StartEndSpaces brackets;
    } subscript;
    struct {
      size_t before_subscript;
      _StartEndSpaces call;
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
    struct {
      size_t before_subscript;
      _StartEndSpaces call;
      _BeforeAfterSpaces argument;
      size_t before_attribute;
    } function;
    struct {
      size_t before_inline;
      size_t before_text;
    } comment;
  } command;
} PrinterOptions;

const PrinterOptions default_po;

#endif  // NVIM_VIML_PRINTER_PRINTER_H
