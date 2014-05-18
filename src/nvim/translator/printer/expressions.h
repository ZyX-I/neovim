// vim: ts=8 sts=2 sw=2 tw=80
//
// Copyright 2014 Nikolay Pavlov

#ifndef NEOVIM_TRANSLATOR_PRINTER_EXPRESSIONS_H
#define NEOVIM_TRANSLATOR_PRINTER_EXPRESSIONS_H

#include "nvim/translator/parser/expressions.h"
#include "nvim/translator/printer/printer.h"

size_t expr_node_dump_len(const PrinterOptions *const po,
                          const ExpressionNode *const node);
void expr_node_dump(const PrinterOptions *const po,
                    const ExpressionNode *const node,
                    char **pp);

size_t expr_node_repr_len(const PrinterOptions *const po,
                          const ExpressionNode *const node);
void expr_node_repr(const PrinterOptions *const po,
                    const ExpressionNode *const node,
                    char **pp);

#endif  // NEOVIM_TRANSLATOR_PRINTER_EXPRESSIONS_H
