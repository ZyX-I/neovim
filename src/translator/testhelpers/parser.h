#ifndef NEOVIM_TRANSLATOR_TESTHELPERS_PARSER_H
#define NEOVIM_TRANSLATOR_TESTHELPERS_PARSER_H

char *parse_cmd_test(const char *arg, const uint_least8_t flags,
                     const bool one);
char *parse0_repr(const char *arg, const bool dump_as_expr);

#endif  // NEOVIM_TRANSLATOR_TESTHELPERS_PARSER_H
