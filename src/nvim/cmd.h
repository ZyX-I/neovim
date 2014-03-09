// vim: ts=8 sts=2 sw=2 tw=80
//
// Copyright 2014 Nikolay Pavlov

#ifndef NEOVIM_CMD_H
#define NEOVIM_CMD_H

#ifdef DO_DECLARE_EXCMD
# undef DO_DECLARE_EXCMD
#endif
#ifdef DO_DECLARE_EXCMD
# undef DO_DECLARE_EXCMD
#endif
#include <stdint.h>
#include "nvim/ex_cmds_defs.h"
#include "nvim/types.h"
#include "nvim/expr.h"


// :argadd/:argd
#define ARG_NAME_FILES     0

typedef cmdidx_T CommandType;

// FIXME
typedef char_u * Pattern;
typedef char_u * Glob;
typedef char_u * Regex;
typedef char_u * Address;
typedef int AuEvent;
typedef int CmdCompleteType;

typedef Address* Range[2];

typedef struct menu_item {
  char_u *name;               // NULL for :unmenu *
  size_t shortcut_position;   // Ignored for :unmenu
  struct menu_item *subitem;
} MenuItem;

typedef struct {
  CmdCompleteType type;
  char_u *arg;
} CmdComplete;

typedef struct command_node {
  CommandType type;
  struct command_node *next;
  size_t num_args;
  Range *range;               // sometimes used for count
  union {
    int count;
    int bufnr;
    char_u reg;
  } arg;
  int bang;                   // :cmd!
  struct command_argument {
    CommandArgType type;
    union {
      struct command_node *command;
      // least32 is essential to hold 24-bit color and additional 4 flag bits 
      // for :hi guifg/guibg/guisp
      uint_least32_t flags;
      int number;
      int *numbers;
      char_u ch;
      char_u *str;
      char_u **strs;
      Pattern *pat;
      Glob *glob;
      Regex *reg;
      AuEvent event;
      AuEvent *events;
      MenuItem *menu_item;
      Address *address;
      CmdComplete *complete;
      ExpressionNode *expr;
      struct command_subargs {
        unsigned type;
        size_t num_args;
        struct command_argument *args;
      } args;
    } arg;
  } args[1];
} CommandNode;

typedef struct command_argument CommandArg;
typedef struct command_subargs  CommandSubArgs;

typedef struct {
  int nl_in_cmd;  // Allow commands like :normal to accept nl as their part
} CommandParserOptions;

typedef struct {
  char_u *file;
  size_t linenr;
  size_t offset;
  char_u *message;
} CommandParserError;

typedef char_u *(*line_getter)(int, void *, int);

#ifdef INCLUDE_GENERATED_DECLARATIONS
# include "cmd.h.generated.h"
#endif

#endif  // NEOVIM_CMD_H
