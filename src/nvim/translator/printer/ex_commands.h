// Copyright 2014 Nikolay Pavlov

#ifndef NEOVIM_TRANSLATOR_PRINTER_EX_COMMANDS_H
#define NEOVIM_TRANSLATOR_PRINTER_EX_COMMANDS_H

#include "nvim/translator/printer/printer.h"

void cmd_repr(const PrinterOptions *const po, const CommandNode *node,
              char **pp);
size_t cmd_repr_len(const PrinterOptions *const po, const CommandNode *node);

#endif  // NEOVIM_TRANSLATOR_PRINTER_EX_COMMANDS_H
