/* vi:set ts=8 sts=4 sw=4:
 *
 * VIM - Vi IMproved	by Bram Moolenaar
 *
 * Do ":help uganda"  in Vim to read copying and usage conditions.
 * Do ":help credits" in Vim to see a list of people who contributed.
 */

#ifndef NEOVIM_TYPES_H
#define NEOVIM_TYPES_H

#include <stdint.h>

/*
 * Shorthand for unsigned variables. Many systems, but not all, have u_char
 * already defined, so we use char_u to avoid trouble.
 */
typedef unsigned char char_u;
typedef unsigned short short_u;
typedef unsigned int int_u;

typedef long linenr_T;                  /* line number type */
typedef int colnr_T;                    /* column number type */
typedef unsigned short disptick_T;      /* display tick type */

#endif /* NEOVIM_TYPES_H */
