#ifndef NEOVIM_TRANSLATOR_TESTHELPERS_FGETLINE_H
#define NEOVIM_TRANSLATOR_TESTHELPERS_FGETLINE_H

char *fgetline_file(int _, FILE *file, int indent);
char *fgetline_string(int c, char **arg, int indent);

#endif  // NEOVIM_TRANSLATOR_TESTHELPERS_FGETLINE_H
