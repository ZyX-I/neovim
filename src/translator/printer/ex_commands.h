// vim: ts=8 sts=2 sw=2 tw=80
//
// Copyright 2014 Nikolay Pavlov

#ifndef NEOVIM_TRANSLATOR_PRINTER_EX_COMMANDS_H
#define NEOVIM_TRANSLATOR_PRINTER_EX_COMMANDS_H

char *parse_cmd_test(char_u *arg, uint_least8_t flags, bool one);
char_u *fgetline_string(int c, char_u **arg, int indent);

#endif  // NEOVIM_TRANSLATOR_PRINTER_EX_COMMANDS_H
