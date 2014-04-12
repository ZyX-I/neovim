// vim: ts=8 sts=2 sw=2 tw=80
//
// Copyright 2014 Nikolay Pavlov

#ifndef NEOVIM_TRANSLATOR_PRINTER_EXPRESSIONS_H
#define NEOVIM_TRANSLATOR_PRINTER_EXPRESSIONS_H

#include "translator/parser/expressions.h"
#include "translator/printer/printer.h"

char *parse0_repr(char_u *arg, bool dump_as_expr);
size_t expr_node_dump_len(const PrinterOptions *const po, ExpressionNode *node);
void expr_node_dump(const PrinterOptions *const po, ExpressionNode *node,
                    char **pp);

#endif  // NEOVIM_TRANSLATOR_PRINTER_EXPRESSIONS_H
