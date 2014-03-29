// vim: ts=8 sts=2 sw=2 tw=80
//
// Copyright 2014 Nikolay Pavlov

#ifndef NEOVIM_TRANSLATOR_PARSER_EX_COMMANDS_H
#define NEOVIM_TRANSLATOR_PARSER_EX_COMMANDS_H

#include <stdint.h>

#include "nvim/types.h"

#ifdef DO_DECLARE_EXCMD
# undef DO_DECLARE_EXCMD
#endif
#include "nvim/translator/parser/command_definitions.h"
#include "nvim/translator/parser/expressions.h"

// :argadd/:argd
#define ARG_NAME_FILES     0

// FIXME
typedef char_u Pattern;
typedef char_u Glob;
typedef char_u Regex;
typedef int AuEvent;
typedef int CmdCompleteType;

/// Address followup type
typedef enum {
  kAddressFollowupMissing = 0,      // No folowup
  kAddressFollowupForwardPattern,   // 10/abc/
  kAddressFollowupBackwardPattern,  // 10/abc/
  kAddressFollowupShift,            // /abc/+10
} AddressFollowupType;

/// A structure to represent Ex address followup
///
/// Ex address followup is a part of address that follows the first token: e.g. 
/// "+1" in "/foo/+1", "?bar?" in "/foo/?bar?", etc.
typedef struct address_followup {
  AddressFollowupType type;       ///< Type of the followup
  union {
    int shift;                    ///< Exact offset: "+1", "-1" (for
                                  ///< kAddressFollowupShift)
    Regex *regex;                 ///< Regular expression (for
                                  ///< kAddressFollowupForwardPattern and 
                                  ///< kAddressFollowupBackwardPattern)
  } data;                         ///< Address followup data
  struct address_followup *next;  ///< Next followup in a sequence
} AddressFollowup;

/// Ex address types
typedef enum {
  kAddrMissing = 0,             ///< No address
  kAddrFixed,                   ///< Fixed line: 1, 10, …
  kAddrEnd,                     ///< End of file: $
  kAddrCurrent,                 ///< Current line: .
  kAddrMark,                    ///< Mark: 't, 'T
  kAddrForwardSearch,           ///< Forward search: /pattern/
  kAddrBackwardSearch,          ///< Backward search: ?pattern?
  kAddrForwardPreviousSearch,   ///< Forward search with old pattern: \/
  kAddrBackwardPreviousSearch,  ///< Backward search with old pattern: \?
  kAddrSubstituteSearch,        ///< Search with old substitute pattern: \&
} AddressType;

/// A structure to represent Ex address
typedef struct {
  AddressType type;  ///< Ex address type
  union {
    Regex *regex;              ///< Regular expression (for kAddrForwardSearch 
                               ///< and kAddrBackwardSearch address types)
    char_u mark;               ///< Address mark (for kAddrMark type)
    linenr_T lnr;              ///< Exact line (for kAddrFixed type)
  } data;                      ///< Address data (not for kAddrMissing, 
                               ///< kAddrEnd, kAddrCurrent, 
                               ///< kAddrForwardPreviousSearch, 
                               ///< kAddrBackwardPreviousSearch and 
                               ///< kAddrSubstituteSearch)
  AddressFollowup *followups;  ///< Ex address followups data
} Address;

/// A structure to represent line range
typedef struct range {
  Address address;     ///< Ex address
  bool setpos;         ///< TRUE if next address in range was separated from 
                       ///< this address by ';'
  struct range *next;  ///< Next address in range. There is no limit in a number 
                       ///< of consequent addresses
} Range;

/// A structure to represent GUI menu item
typedef struct menu_item {
  char_u *name;               ///< Menu name
  struct menu_item *subitem;  ///< Sub menu name
} MenuItem;

typedef struct {
  CmdCompleteType type;
  char_u *arg;
} CmdComplete;

/// A structure for holding command position
///
/// Intended for debugging purposes later
typedef struct {
  linenr_T lnr;
  colnr_T col;
  char_u *fname;
} CommandPosition;

/// Counter type: type of a simple additional argument
typedef enum {
  kCntMissing = 0,  ///< No additional argument was specified
  kCntCount,        ///< Unsigned integer
  kCntBuffer,
  kCntReg,
  kCntExprReg,
} CountType;

#define FLAG_EX_LIST  0x01
#define FLAG_EX_LNR   0x02
#define FLAG_EX_PRINT 0x04

/// Structure for representing one command
typedef struct command_node {
  CommandType type;               ///< Command type. For built-in commands it 
                                  ///< replaces name
  char_u *name;                   ///< Name of the user command, unresolved
  struct command_node *prev;      ///< Previous command of the same level
  struct command_node *next;      ///< Next command of the same level
  struct command_node *children;  ///< Block (if/while/for/try/function), 
                                  ///< modifier (like leftabove or silent) or 
                                  ///< iterator (tabdo/windo/bufdo/argdo etc)
                                  ///< subcommands
  Range range;                    ///< Ex address range, if any
  CommandPosition position;       ///< Position of the start of the command
  colnr_T end_col;                ///< Last column occupied by this command
  CountType cnt_type;             ///< Type of the argument in the next union
  union {
    int count;                    ///< Count (for kCntCount)
    int bufnr;
    char_u reg;
    ExpressionNode *expr;
  } cnt;                          ///< First simple argument
  uint_least8_t exflags;          ///< Ex flags (for :print command and like)
  bool bang;                      ///< TRUE if command was used with a bang
  struct command_argument {
    union {
      // least32 is essential to hold 24-bit color and additional 4 flag bits 
      // for :hi guifg/guibg/guisp
      uint_least32_t flags;       ///< Command flags
      unsigned unumber;           ///< Unsigned integer
      colnr_T col;                ///< Column number (for syntax error)
      int number;                 ///< Signed integer
      int *numbers;               ///< A sequence of numbers in allocated memory
      char_u ch;                  ///< A single character
      char_u *str;                ///< String in allocated memory
      garray_T strs;              ///< Growarray
      Pattern *pat;               ///< Pattern (like in :au)
      Glob *glob;                 ///< Glob (like in :e)
      Regex *reg;                 ///< Regular expression (like in :catch)
      AuEvent event;              ///< A autocmd event
      AuEvent *events;            ///< A sequence of autocommand events
      MenuItem *menu_item;        ///< Menu item
      Address *address;           ///< Ex mode address
      CmdComplete *complete;
      ExpressionNode *expr;       ///< Expression (:if) or a list of expressions 
                                  ///< (:echo) (uses expr->next to build 
                                  ///< a linked list)
      struct command_subargs {
        unsigned type;
        size_t num_args;
        CommandArgType *types;
        struct command_argument *args;
      } args;
    } arg;
  } args[1];                      ///< Command arguments
} CommandNode;

typedef struct command_argument CommandArg;
typedef struct command_subargs  CommandSubArgs;

typedef struct {
  uint_least16_t flags;
  bool early_return;
} CommandParserOptions;

typedef struct {
  char_u *position;
  char *message;
} CommandParserError;

typedef char_u *(*line_getter)(int, void *, int);

typedef int (*args_parser)(char_u **,
                           CommandNode *,
                           CommandParserError *,
                           CommandParserOptions,
                           CommandPosition *,
                           line_getter,
                           void *);

// flags for parse_one_cmd
#define FLAG_POC_EXMODE      0x01
#define FLAG_POC_CPO_STAR    0x02
#define FLAG_POC_CPO_BSLASH  0x04
#define FLAG_POC_CPO_SPECI   0x08
#define FLAG_POC_CPO_KEYCODE 0x10
#define FLAG_POC_CPO_BAR     0x20
#define FLAG_POC_ALTKEYMAP   0x40
#define FLAG_POC_RL          0x80

#define FLAG_POC_TO_FLAG_CPO(flags) ((flags >> 3)&0x06)

#define MENU_DEFAULT_PRI_VALUE 500
#define MENU_DEFAULT_PRI        -1

#define MAX_NEST_BLOCKS   CSTACK_LEN * 3

#ifdef INCLUDE_GENERATED_DECLARATIONS
# include "nvim/translator/parser/ex_commands.h.generated.h"
#endif

#endif  // NEOVIM_TRANSLATOR_PARSER_EX_COMMANDS_H
