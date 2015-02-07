#ifndef NVIM_VIML_PARSER_EX_COMMANDS_H
#define NVIM_VIML_PARSER_EX_COMMANDS_H

#include <stdint.h>

#include "nvim/types.h"

#ifdef DO_DECLARE_EXCMD
# undef DO_DECLARE_EXCMD
#endif
// FIXME Remove the two following includes: only regexp_defs is needed.
#include "nvim/vim.h"
#include "nvim/buffer_defs.h"
#include "nvim/viml/parser/command_definitions.h"
#include "nvim/viml/parser/command_arguments.h"
#include "nvim/viml/parser/expressions.h"
// FIXME Remove the two following includes: only regexp_defs is needed.
#include "nvim/vim.h"
#include "nvim/buffer_defs.h"
#include "nvim/regexp_defs.h"
#include "nvim/ex_docmd.h"

// FIXME
typedef int AuEvent;

/// Regular expression
typedef struct {
  regprog_T *prog;  ///< Compiled pattern
  char string[1];   ///< Original string
} Regex;

typedef enum {
  kPatMissing = 0,
  kPatLiteral,      ///< Literal string (char *str)
  kPatHome,         ///< Tilde expansion (no data)
  kPatEnviron,      ///< Environment variable (char *str)
  kPatCurrent,      ///< Current file name (no data)
  kPatAlternate,    ///< Alternate file name (no data)
  kPatBufname,      ///< Buffer name ("#N") expansion (int number)
  kPatOldFile,      ///< Old file expansion ("#<N") (int number)
  kPatArguments,    ///< Arguments list expansion ("##") (no data)
  kPatCharacter,    ///< "?" expansion (no data)
  kPatAnything,     ///< "*" expansion (no data)
  kPatAnyRecurse,   ///< "**" expansion (no data)
  kPatCollection,   ///< [abc] expansion (char *str)
  kPatBranch,       ///< {a,b} expansion (Glob *glob)
  // The following arguments are only valid for Glob and not for Pattern
  kGlobShell,       ///< Shell backtick expansion (char *str)
  kGlobExpression,  ///< "`=expr`" expansion (Expression *expr)
  // The following arguments are only valid for :autocmd pattern list
  kPatAuList        ///< :autocmd pattern list
} PatternType;

/// Structure representing glob pattern
typedef struct pattern {
  PatternType type;        ///< Type of the pattern chunk
  union {
    char *str;             ///< String argument (for most types)
    Expression *expr;      ///< Expression (for "`=expr`")
    int number;            ///< Buffer name or old file number
    struct patterns {
      struct pattern *pat;
      struct patterns *next;
    } pats;                ///< Pattern used for branch ({a,b,c})
  } data;                  ///< Pattern data
  struct pattern *next;    ///< Next chunk
} Pattern;

typedef struct patterns Patterns;

typedef struct glob {
  Pattern pat;
  struct glob *next;
} Glob;

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
    char mark;                 ///< Address mark (for kAddrMark type)
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
  bool setpos;         ///< true if next address in range was separated from 
                       ///< this address by ';'
  struct range *next;  ///< Next address in range. There is no limit in 
                       ///< a number of consequent addresses
} Range;

/// A structure to represent GUI menu item
typedef struct menu_item {
  char *name;                 ///< Menu name
  struct menu_item *subitem;  ///< Sub menu name
} MenuItem;

typedef struct {
  CmdCompleteType type;
  char *arg;
} CmdComplete;

/// A structure for holding command position
///
/// Intended for debugging purposes later
typedef struct {
  size_t lnr;
  size_t col;
} CommandPosition;

/// Structure for representing a register.
typedef struct {
  char name;         ///< Register name.
  Expression *expr;  ///< Expression (for "=" register only).
} Register;

#define FLAG_EX_LIST  0x01
#define FLAG_EX_LNR   0x02
#define FLAG_EX_PRINT 0x04

/// Possible replacement types
///
/// @note Special "%" is not listed below because it is the only atom that works 
///       depending on an option.
typedef enum {
  kRepLiteral,       ///< Literal string.
  kRepExpr,          ///< "\\=" expression.
  kRepEscLiteral,    ///< Escaped character.
  kRepEscaped,       ///< Special character in escaped form (e.g. "\\t").
  kRepMatched,       ///< Whole matched pattern ("\\0", "&").
  kRepGroup,         ///< Capturing group index ("\\1"…"\\9").
  kRepPrevSub,       ///< Previous :s replacement string ("~").
  kRepCharUpCase,    ///< Next character made uppercase ("\\u").
  kRepUpCase,        ///< Following characters made uppercase ("\\U").
  kRepCharDownCase,  ///< Next character made lowercase ("\\l").
  kRepDownCase,      ///< Following characters made lowercase ("\\L").
  kRepCaseEnd,       ///< End of kRep*Case ("\\e", "\\E").
  kRepNewLine,       ///< New line ("\\r").
} ReplacementType;

/// Structure representing :s replacement string
typedef struct replacement {
  ReplacementType type;  ///< Item type.
  size_t start_col;      ///< Offset of the first character of the item.
  size_t end_col;        ///< Offset of the last character of the item.
  union {
    uint8_t group;       ///< Group number, for kRepGroup. Is always in [1, 9].
    uint32_t ch;         ///< Escaped character, kRepEscaped and kRepEscLiteral.
    char *str;           ///< Literal replacement string, for kRepLiteral.
    Expression *expr;    ///< Replacement expression, for kRepExpr.
  } data;
  struct replacement *next;  ///< Next parsed item.
} Replacement;

/// Possible ways to define highlight color
typedef enum {
  kHiColorName,  ///< Name of the color (string).
  kHiColorRGB,   ///< 24-bit RGB color.
  kHiColorIdx,   ///< Terminal color index.
  kHiColorFg,    ///< Same as foreground color.
  kHiColorBg,    ///< Same as background color.
  kHiColorNone,  ///< No color needed.
} HighlightColorType;

/// Structure that contains color definition
typedef struct {
  HighlightColorType type;  ///< Type of the definition.
  union {
    char *name;             ///< Human-readable name of the color.
    struct {
      unsigned red : 8;     ///< Red component of RGB color.
      unsigned green : 8;   ///< Green component of RGB color.
      unsigned blue : 8;    ///< Blue component of RGB color.
    } rgb;                  ///< 24-bit RGB color.
    uint8_t idx;            ///< Terminal color index.
  } data;
} HighlightColor;

/// Structure for representing one command
typedef struct command_node {
  CommandType type;               ///< Command type. For built-in commands it 
                                  ///< replaces name
  char *name;                     ///< Name of the user command, unresolved
  struct command_node *prev;      ///< Previous command of the same level
  struct command_node *next;      ///< Next command of the same level
  struct command_node *children;  ///< Block (if/while/for/try/function), 
                                  ///< modifier (like leftabove or silent) or 
                                  ///< iterator (tabdo/windo/bufdo/argdo etc)
                                  ///< subcommands
  Range range;                    ///< Ex address range, if any
  size_t *skips;                  ///< Array of positions where characters from 
                                  ///< “real” string were removed (shows the 
                                  ///< former positions of <C-v> characters and 
                                  ///< backslashes, useful for computing real 
                                  ///< column number)
  size_t skips_count;             ///< Number of items in the above array.
  CommandPosition position;       ///< Position of the start of the command
  size_t end_col;                 ///< Last column occupied by this command
  bool has_count;                 ///< True if there is a count.
  int count;                      ///< Count.
  Register reg;                   ///< Register.
  uint_least8_t exflags;          ///< Ex flags (for :print command and like)
  uint_least32_t optflags;        ///< ++opt flags
  char *enc;                      ///< Encoding from ++enc
  Glob glob;                      ///< File name(s)
  bool bang;                      ///< true if command was used with a bang
  struct command_argument {
    union {
      // least32 is essential to hold 24-bit color and additional 4 flag bits 
      // for :hi guifg/guibg/guisp
      uint_least32_t flags;       ///< Command flags
      unsigned unumber;           ///< Unsigned integer
      size_t col;                 ///< Column number (for syntax error)
      int number;                 ///< Signed integer
      int *numbers;               ///< An array of signed integers.
      uint_least32_t *unumbers;   ///< An array of unsigned integers in 
                                  ///< allocated memory. At least 32 bits are 
                                  ///< needed to hold any unicode codepoint.
      char ch;                    ///< A single character
      char *str;                  ///< String in allocated memory
      char **strs;                ///< NULL-terminated strings array.
      garray_T ga_strs;           ///< Growarray containing char * strings.
      Pattern *pat;               ///< Pattern (like in :au)
      Glob *glob;                 ///< Glob (like in :e)
      Regex *reg;                 ///< Regular expression (like in :catch)
      AuEvent *events;            ///< A sequence of autocommand events
      MenuItem *menu_item;        ///< Menu item
      Range *range;               ///< Ex mode address
      CmdComplete *complete;
      Expression *expr;           ///< Expression (:if) or a list of 
                                  ///< expressions (:echo) (uses expr->next to 
                                  ///< build a linked list)
      Replacement *rep;           ///< Replacement string, parsed.
      HighlightColor color;       ///< Color definition.
      struct command_subargs {
        unsigned type;
        size_t num_args;
        const CommandArgType *types;
        struct command_argument *args;
      } args;
    } arg;
  } args[1];                      ///< Command arguments
} CommandNode;

typedef struct {
  CommandNode *node;
  char **lines;
  size_t lines_size;
  char *fname;
} ParserResult;

typedef struct command_argument CommandArg;
typedef struct command_subargs  CommandSubArgs;

typedef struct {
  uint_least16_t flags;
  bool early_return;
} CommandParserOptions;

typedef struct {
  const char *position;
  const char *message;
} CommandParserError;

typedef char *(*VimlLineGetter)(int, void *, int);

typedef int (*CommandArgsParser)(const char **,
                                 CommandNode *,
                                 CommandParserError *,
                                 CommandParserOptions,
                                 CommandPosition,
                                 VimlLineGetter,
                                 void *);

/// Definition of a single highlight attribute
///
/// Used for parsing `term=`, `gui=` and `cterm=` attribute lists.
typedef struct {
  const char *hl_attr_name;           ///< Attribute name.
  const size_t hl_attr_name_len;      ///< Cached strlen() of attribute name.
  const uint_least32_t hl_attr_flag;  ///< Attribute flag.
} HighlightAttrDef;

/// Table that maps attribute names to corresponding binary flags
extern const HighlightAttrDef hl_attr_table[];

// flags for parse_one_cmd
#define FLAG_POC_EXMODE      0x01
#define FLAG_POC_CPO_STAR    0x02
#define FLAG_POC_CPO_BSLASH  0x04
#define FLAG_POC_CPO_SPECI   0x08
#define FLAG_POC_CPO_KEYCODE 0x10
#define FLAG_POC_CPO_BAR     0x20
#define FLAG_POC_CPO_SUBPC   0x40
#define FLAG_POC_ALTKEYMAP   0x80
#define FLAG_POC_RL         0x100
#define FLAG_POC_MAGIC      0x200
#define FLAG_POC_ED         0x400

#define FLAG_POC_TO_FLAG_CPO(flags) ((flags >> 3)&0x06)

#define MENU_DEFAULT_PRI_VALUE 500
#define MENU_DEFAULT_PRI        -1

/// Structure that represents one built-in command
typedef struct {
  char      *name;            ///< name of the command
  uint_least32_t flags;       ///< flags declared above
  size_t num_args;            ///< number of arguments
  CommandArgType *arg_types;  ///< argument types
  CommandArgsParser parse;    ///< argument parsing function
} VimlCommandDefinition;

/// Array containing all built-in commands.
extern VimlCommandDefinition cmddefs[kCmdREAL_SIZE];

/// Static empty string
///
/// Is used to replace NULL (i.e. “no value”) in NULL-terminated list of 
/// strings.
extern const char *const empty_string;

/// Macros that returns command definition
///
/// @param[in]  type  Command node type.
///
/// @return CommandDefinition structure with the definition of the given 
///         command.
#define CMDDEF(type) cmddefs[type - 1]

/// Command node that is used to represent absense of command
const CommandNode nocmd;

/// Maximum nesting level for :for/:while/:if/:try blocks
#define MAX_NEST_BLOCKS   CSTACK_LEN * 3

/// Characters that end Ex command, except for the comment and NUL characters
#define ENDS_EXCMD_NOCOMMENT_CHARS "|\n"

/// Characters that end Ex command, except for the NUL character
#define ENDS_EXCMD_CHARS ENDS_EXCMD_NOCOMMENT_CHARS "\""

/// Macros that checks whether given character ends Ex command
#define ENDS_EXCMD_NOCOMMENT(ch) ((ch) == NUL || (ch) == '|' || (ch) == '\n')

/// Like #ENDS_EXCMD_NOCOMMENT above, but also includes comment character
#define ENDS_EXCMD(ch) ((ch) == '"' || ENDS_EXCMD_NOCOMMENT(ch))

/// Number that is guaranteed to be less or equal to the minimal mark character
#define FIRST_MARK_CODE 0x21
/// Number that is guaranteed to be greater or equal to the max mark character
#define LAST_MARK_CODE 0x7F
/// Length of the string that may hold enough marks
#define MAX_NUM_MARKS (LAST_MARK_CODE - FIRST_MARK_CODE + 1)

#define SIGN_ID_MISSING -1
#define SIGN_ID_ALL     -2

#ifdef INCLUDE_GENERATED_DECLARATIONS
# include "viml/parser/ex_commands.h.generated.h"
#endif
#endif  // NVIM_VIML_PARSER_EX_COMMANDS_H
