// vim: ts=8 sts=2 sw=2 tw=80
//
// Copyright 2014 Nikolay Pavlov

#ifndef NEOVIM_CMD_H
#define NEOVIM_CMD_H

#include <stdint.h>
#ifdef DO_DECLARE_EXCMD
# undef DO_DECLARE_EXCMD
#endif
<<<<<<< HEAD:src/nvim/cmd.h
#include <stdint.h>
#include "nvim/cmd_def.h"
#include "nvim/types.h"
#include "nvim/expr.h"
=======
#include "cmd_def.h"
#include "types.h"
#include "expr.h"
#include "vim.h"
>>>>>>> 5e1964f... Export parse_one_cmd:src/cmd.h


// :argadd/:argd
#define ARG_NAME_FILES     0

// FIXME
typedef char_u Pattern;
typedef char_u Glob;
typedef char_u Regex;
typedef int AuEvent;
typedef int CmdCompleteType;

typedef enum {
  kAddrMissing = 0,             // No address
  kAddrFixed,                   // Fixed line: 1, 10, …
  kAddrEnd,                     // End of file: $
  kAddrCurrent,                 // Current line: .
  kAddrMark,                    // Mark: 't, 'T
  kAddrForwardSearch,           // Forward search: /pattern/
  kAddrBackwardSearch,          // Backward search: ?pattern?
  kAddrForwardPreviousSearch,   // Forward search with old pattern: \/
  kAddrBackwardPreviousSearch,  // Backward search with old pattern: \?
  kAddrSubstituteSearch,        // Search with old substitute pattern: \&
} AddressType;

typedef struct {
  AddressType type;
  union {
    Regex *regex;
    char_u mark;
    linenr_T lnr;
  } data;
} Address;

typedef enum {
  kAddressFollowupMissing = 0,      // No folowup
  kAddressFollowupForwardPattern,   // 10/abc/
  kAddressFollowupBackwardPattern,  // 10/abc/
  kAddressFollowupShift,            // /abc/+10
} AddressFollowupType;

typedef struct address_followup {
  AddressFollowupType type;
  union {
    int shift;
    Regex *regex;
  } data;
  struct address_followup *next;
} AddressFollowup;

typedef struct {
  Address start;
  Address end;
  AddressFollowup *start_followups;
  AddressFollowup *end_followups;
  int setpos;                        // , vs ;
} Range;

typedef struct menu_item {
  char_u *name;               // NULL for :unmenu *
  size_t shortcut_position;   // Ignored for :unmenu
  struct menu_item *subitem;
} MenuItem;

typedef struct {
  CmdCompleteType type;
  char_u *arg;
} CmdComplete;

typedef struct {
  linenr_T lnr;
  colnr_T col;
  char_u *fname;
} CommandPosition;

typedef struct command_node {
  CommandType type;
  char_u *name;              // Only valid for user commands
  struct command_node *next;
  Range range;               // sometimes used for count
  union {
    int count;
    int bufnr;
    char_u reg;
  } arg;
  int bang;                   // :cmd!
  struct command_argument {
    union {
      struct command_node *cmd;
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
        CommandArgType *types;
        struct command_argument *args;
      } args;
      CommandPosition position;
    } arg;
  } args[1];
} CommandNode;

typedef struct command_argument CommandArg;
typedef struct command_subargs  CommandSubArgs;

typedef struct {
  int nl_in_cmd;  // Allow commands like :normal to accept nl as their part
} CommandParserOptions;

typedef struct {
  char_u *position;
  char_u *message;
} CommandParserError;

typedef char_u *(*line_getter)(int, void *, int);

typedef int (*args_parser)(char_u **,
                           CommandNode *,
                           uint_least8_t,
                           CommandPosition *,
                           line_getter,
                           void *);

/* flags for parse_one_cmd */
#define FLAG_POC_SOURCING  0x01
#define FLAG_POC_EXMODE    0x02

#ifdef INCLUDE_GENERATED_DECLARATIONS
# include "cmd.h.generated.h"
#endif

#endif  // NEOVIM_CMD_H
