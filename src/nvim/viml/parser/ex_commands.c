// vim: ts=8 sts=2 sw=2 tw=80

//
// Copyright 2014 Nikolay Pavlov

// ex_commands.c: Ex commands parsing

#include <stddef.h>
#include <stdbool.h>
#include <assert.h>
#include <string.h>

#include "nvim/vim.h"
#include "nvim/types.h"
#include "nvim/memory.h"
#include "nvim/strings.h"
#include "nvim/misc2.h"
#include "nvim/charset.h"
#include "nvim/globals.h"
#include "nvim/garray.h"
#include "nvim/term.h"
#include "nvim/farsi.h"
#include "nvim/menu.h"
#include "nvim/option.h"
#include "nvim/regexp.h"
#include "nvim/ascii.h"
#include "nvim/ex_getln.h"
#include "nvim/ex_docmd.h"
#include "nvim/ops.h"
#include "nvim/option.h"

#include "nvim/viml/parser/expressions.h"
#include "nvim/viml/parser/ex_commands.h"

#ifdef INCLUDE_GENERATED_DECLARATIONS
// FIXME should include fileio.h, but this spawns lots of errors
int check_nomodeline(char_u **argp);
# include "auevents_enum.generated.h"
# include "viml/parser/auevents_find.generated.h"
#endif

#define getdigits(arg) getdigits((char_u **) (arg))
#define skipwhite(arg) (char *) skipwhite((char_u *) (arg))
#define skipdigits(arg) (char *) skipdigits((char_u *) (arg))
#define mb_ptr_adv_(p) p += has_mbyte ? (*mb_ptr2len)((const char_u *) p) : 1
#define mb_ptr2len(p) ((size_t) mb_ptr2len((const char_u *) p))
#define enc_canonize(p) (char *) enc_canonize((char_u *) (p))
#define check_ff_value(p) check_ff_value((char_u *) (p))
#define replace_termcodes(a, b, c, d, e, f, g) \
    (char *) replace_termcodes((const char_u *) a, b, (char_u **) c, d, e, f, g)
#define lrswap(s) lrswap((char_u *) s)
#define get_list_range(pp, i1, i2) get_list_range((char_u **)pp, i1, i2)
#define skiptowhite(p) (const char *) skiptowhite((char_u *) p)
#define check_nomodeline(p) check_nomodeline((char_u **) p)
#define get_histtype(p, len, d) get_histtype((const char_u *) p, len, d)
#define find_key_option_len(p, l) find_key_option_len((const char_u *) p, len)
#define findoption_len(p, l) findoption_len((const char_u *) p, l)
#define vim_str2nr(p, ...) vim_str2nr((const char_u *) (p), __VA_ARGS__)
#define string_to_key(p) string_to_key((char_u *) (p))
#define mb_cptr2char_adv(p) mb_cptr2char_adv((char_u **) (p))

typedef struct {
  CommandType find_in_stack;
  CommandType find_in_stack_2;
  CommandType find_in_stack_3;
  CommandType not_after;
  bool push_stack;
  char *not_after_message;
  char *no_start_message;
  char *duplicate_message;
} CommandBlockOptions;

typedef struct {
  VimlLineGetter fgetline;
  void *cookie;
  garray_T ga;
} SavingFgetlineArgs;

/// Options for parse_menu_name
typedef enum {
  kMenuDefaults,    ///< Do unescape menu items and save text after <Tab>.
  kMenuIgnoreText,  ///< Respect text after <Tab>, but do not save it.
  kMenuWholeCmd,    ///< Ignore <Tab>, do not treat whitespaces specially and
                    ///< treat <C-v> as "\\".
} MenuNameParsingOptions;

#ifdef INCLUDE_GENERATED_DECLARATIONS
# include "viml/parser/ex_commands.c.generated.h"
#endif

#include "nvim/viml/parser/command_definitions.h"
#define DO_DECLARE_EXCMD
#include "nvim/viml/parser/command_definitions.h"
#undef DO_DECLARE_EXCMD

#define NODE_IS_ALLOCATED(node) ((node) != NULL && (node) != &nocmd)

/// Allocate new command node and assign its type property
///
/// Uses type argument to determine how much memory it should allocate.
///
/// @param[in]  type      Node type.
/// @param[in]  position  Position of command start.
///
/// @return Pointer to allocated block of memory or NULL in case of error.
static CommandNode *cmd_alloc(CommandType type, CommandPosition position)
  FUNC_ATTR_NONNULL_RET
{
  // XXX May allocate less space then needed to hold the whole struct: less by 
  // one size of CommandArg.
  size_t size = offsetof(CommandNode, args);
  CommandNode *node;

  if (type != kCmdUnknown)
    size += sizeof(CommandArg) * CMDDEF(type).num_args;

  node = (CommandNode *) xcalloc(1, size);
  node->type = type;
  node->position = position;

  return node;
}

/// Allocate new regex definition and assign all its properties
///
/// @param[in]  string  Regular expression string.
/// @param[in]  len     String length. It is not required for string[len] to be 
///                     a NUL byte.
///
/// @return Pointer to allocated block of memory.
static Regex *regex_alloc(const char *string, size_t len)
{
  Regex *regex;

  regex = (Regex *) xmalloc(offsetof(Regex, string) + len + 1);
  memcpy(regex->string, string, len);
  regex->string[len] = NUL;
  regex->prog = NULL;
  // FIXME: use vim_regcomp, but make it save errors in place of throwing 
  //        them right away.
  // regex->prog = vim_regcomp(reg->string, 0);

  return regex;
}

/// Allocate new replacement atom
///
/// @note Does not zero memory when allocating, but does assign NULL to next 
///       field.
///
/// @param[in]  type       Type of the replacement.
/// @param[in]  start_col  First column of the atom.
/// @param[in]  end_col    Last column of the atom.
///
/// @return Pointer to allocated block of memory.
static Replacement *replacement_alloc(const ReplacementType type,
                                      const size_t start_col,
                                      const size_t end_col)
  FUNC_ATTR_WARN_UNUSED_RESULT FUNC_ATTR_NONNULL_RET
{
  Replacement *ret = XMALLOC_NEW(Replacement, 1);
  ret->type = type;
  ret->start_col = start_col;
  ret->end_col = end_col;
  ret->next = NULL;
  return ret;
}

/// Allocate new glob pattern part
///
/// @param[in]  type  Part type.
///
/// @return Pointer to allocated block of memory.
static Pattern *pattern_alloc(PatternType type)
{
  Pattern *pat;

  pat = XCALLOC_NEW(Pattern, 1);

  pat->type = type;

  return pat;
}

/// Allocate new address followup structure and set its type
///
/// @param[in]  type  Followup type.
///
/// @return Pointer to allocated block of memory.
static AddressFollowup *address_followup_alloc(AddressFollowupType type)
{
  AddressFollowup *followup;

  followup = XCALLOC_NEW(AddressFollowup, 1);

  followup->type = type;

  return followup;
}

static void free_complete(CmdComplete *compl)
{
  if (compl == NULL) {
    return;
  }

  free(compl->arg);
  free(compl);
}

static void free_menu_item(MenuItem *menu_item)
{
  if (menu_item == NULL)
    return;

  free_menu_item(menu_item->subitem);
  free(menu_item->name);
  free(menu_item);
}

static void free_regex(Regex *regex)
{
  if (regex == NULL)
    return;

  vim_regfree(regex->prog);
  free(regex);
}

static void free_replacement(Replacement *rep)
{
  if (rep == NULL) {
    return;
  }

  switch (rep->type) {
    case kRepExpr: {
      free_expr(rep->data.expr);
      break;
    }
    case kRepLiteral: {
      free(rep->data.str);
      break;
    }
    case kRepEscaped:
    case kRepEscLiteral:
    case kRepMatched:
    case kRepGroup:
    case kRepPrevSub:
    case kRepCharUpCase:
    case kRepUpCase:
    case kRepCharDownCase:
    case kRepDownCase:
    case kRepCaseEnd:
    case kRepNewLine: {
      break;
    }
  }
  free(rep);
}

static void free_patterns(Patterns *pats)
{
  if (pats == NULL)
    return;
  free_pattern(pats->pat);
  free(pats->pat);
  free_patterns(pats->next);
  free(pats->next);
}

static void free_pattern(Pattern *pat)
{
  if (pat == NULL)
    return;

  switch (pat->type) {
    case kPatMissing:
    case kPatHome:
    case kPatCurrent:
    case kPatAlternate:
    case kPatBufname:
    case kPatOldFile:
    case kPatArguments:
    case kPatCharacter:
    case kPatAnything:
    case kPatAnyRecurse: {
      break;
    }
    case kPatLiteral:
    case kPatEnviron:
    case kPatCollection:
    case kGlobShell: {
      free(pat->data.str);
      break;
    }
    case kGlobExpression: {
      free_expr(pat->data.expr);
      break;
    }
    case kPatAuList:
    case kPatBranch: {
      free_patterns(&(pat->data.pats));
      break;
    }
  }
  free_pattern(pat->next);
}

static void free_glob(Glob *glob)
{
  if (glob == NULL)
    return;

  free_pattern(&(glob->pat));
  free_glob(glob->next);
  free(glob->next);
}

static void free_address_data(Address *address)
{
  if (address == NULL)
    return;

  switch (address->type) {
    case kAddrMissing:
    case kAddrFixed:
    case kAddrEnd:
    case kAddrCurrent:
    case kAddrMark:
    case kAddrForwardPreviousSearch:
    case kAddrBackwardPreviousSearch:
    case kAddrSubstituteSearch: {
      break;
    }
    case kAddrForwardSearch:
    case kAddrBackwardSearch: {
      free_regex(address->data.regex);
      break;
    }
  }
  free_address_followup(address->followups);
}

static void free_address_followup(AddressFollowup *followup)
{
  if (followup == NULL)
    return;

  switch (followup->type) {
    case kAddressFollowupMissing:
    case kAddressFollowupShift: {
      break;
    }
    case kAddressFollowupForwardPattern:
    case kAddressFollowupBackwardPattern: {
      free_regex(followup->data.regex);
      break;
    }
  }
  free_address_followup(followup->next);
  free(followup);
}

static void free_range_data(Range *range)
{
  if (range == NULL)
    return;

  free_address_data(&(range->address));
  free_range(range->next);
}

static void free_range(Range *range)
{
  if (range == NULL)
    return;

  free_range_data(range);
  free(range);
}

const char *const empty_string = "";

static void free_cmd_arg(CommandArg *arg, CommandArgType type)
{
  switch (type) {
    case kArgExpression:
    case kArgExpressions:
    case kArgAssignLhs: {
      free_expr(arg->arg.expr);
      break;
    }
    case kArgFlags:
    case kArgNumber:
    case kArgUNumber:
    case kArgChar:
    case kArgColumn: {
      break;
    }
    case kArgNumbers: {
      free(arg->arg.numbers);
      break;
    }
    case kArgUNumbers: {
      free(arg->arg.unumbers);
      break;
    }
    case kArgString: {
      free(arg->arg.str);
      break;
    }
    case kArgPattern: {
      free_pattern(arg->arg.pat);
      break;
    }
    case kArgGlob: {
      free_glob(arg->arg.glob);
      free(arg->arg.glob);
      break;
    }
    case kArgRegex: {
      free_regex(arg->arg.reg);
      break;
    }
    case kArgReplacement: {
      free_replacement(arg->arg.rep);
      break;
    }
    case kArgLines:
    case kArgGaStrings: {
      ga_clear_strings(&(arg->arg.ga_strs));
      break;
    }
    case kArgStrings: {
      char **const strs = arg->arg.strs;
      if (strs != NULL) {
        char **cur_str = strs;
        while (*cur_str != NULL) {
          if (*cur_str != empty_string) {
            free(*cur_str);
          }
          cur_str++;
        }
        free(strs);
      }
      break;
    }
    case kArgMenuName: {
      free_menu_item(arg->arg.menu_item);
      break;
    }
    case kArgAuEvents: {
      free(arg->arg.events);
      break;
    }
    case kArgAddress: {
      free_range(arg->arg.range);
      break;
    }
    case kArgCmdComplete: {
      // FIXME
      break;
    }
    case kArgArgs: {
      CommandArg *subargs = arg->arg.args.args;
      size_t numsubargs = arg->arg.args.num_args;
      size_t i;
      for (i = 0; i < numsubargs; i++)
        free_cmd_arg(&(subargs[i]), arg->arg.args.types[i]);
      free(subargs);
      break;
    }
  }
}

void free_cmd(CommandNode *node)
{
  size_t numargs;
  size_t i;

  if (!NODE_IS_ALLOCATED(node))
    return;

  numargs = CMDDEF(node->type).num_args;

  if (node->type != kCmdUnknown)
    for (i = 0; i < numargs; i++)
      free_cmd_arg(&(node->args[i]), CMDDEF(node->type).arg_types[i]);

  free_cmd(node->next);
  free_cmd(node->children);
  free_glob(&(node->glob));
  free_range_data(&(node->range));
  free(node->name);
  free(node->skips);
  free(node);
}

/// Get a list of comma separated patterns
///
/// Useful for branches (src/{foo,bar}.c) and autocommand pattern list 
/// (src/foo.c,src/bar.c).
///
/// @param[in,out]  pp       Parsed string. Warning: it expects one character 
///                          before the actual pattern.
/// @param[out]     error    Address where error will be saved.
/// @param[out]     pat      Address where pattern will be saved.
/// @param[in]      is_glob  True if parsing fully qualified globs (i.e. `` 
///                          `shell command` `` and `` `=VimL.Expression()` `` 
///                          are allowed).
/// @param[in]      col      Column number (for error reporting).
///
/// @return OK if everything is good, NOTDONE if there was an error (in this 
///         case error structure must be populated), FAIL if there was error 
///         without error message.
static int get_comma_separated_patterns(const char **pp,
                                        CommandParserError *error,
                                        Pattern **pat,
                                        const bool is_glob,
                                        const size_t col)
  FUNC_ATTR_WARN_UNUSED_RESULT FUNC_ATTR_NONNULL_ALL
{
  Pattern **next = pat;
  const char *p = *pp;
  const char *const s = p;
  Patterns *cur_pats = &((*next)->data.pats);
  Patterns **next_pats = &cur_pats;
  do {
    int cret;
    Pattern *pat = NULL;
    p++;
    if ((cret = get_pattern(&p, error, &pat, true, is_glob,
                            col + (size_t) (p - s)))
        != OK) {
      return cret;
    }
    if (pat != NULL) {
      if (*next_pats == NULL) {
        *next_pats = XCALLOC_NEW(Patterns, 1);
      }
      (*next_pats)->pat = pat;
      next_pats = &((*next_pats)->next);
    }
  } while (*p == ',');
  *pp = p;
  return OK;
}

static int get_pattern(const char **pp, CommandParserError *error,
                       Pattern **pat, const bool is_branch, const bool is_glob,
                       const size_t col)
{
  const char *p = *pp;
  Pattern **next = pat;
  size_t literal_length = 0;
  const char *literal_start = NULL;
  int ret = FAIL;
  const char *const s = p;

  *pat = NULL;

  for (;;) {
    PatternType type = kPatMissing;
    switch (*p) {
      case '`': {
        if (!is_glob)
          type = kPatLiteral;
        // FIXME Not compatible
        else if (is_branch)
          type = kPatLiteral;
        else if (p[1] == '=')
          type = kGlobExpression;
        else
          type = kGlobShell;
        break;
      }
      case '*': {
        if (p[1] == '*')
          type = kPatAnyRecurse;
        else
          type = kPatAnything;
        break;
      }
      case '?': {
        type = kPatCharacter;
        break;
      }
      case '%': {
        type = kPatCurrent;
        break;
      }
      case '#': {
        switch (p[1]) {
          case '1':
          case '2':
          case '3':
          case '4':
          case '5':
          case '6':
          case '7':
          case '8':
          case '9': {
            type = kPatBufname;
            break;
          }
          case '<': {
            if (VIM_ISDIGIT(p[2]))
              type = kPatOldFile;
            else
              type = kPatLiteral;
            break;
          }
          case '#': {
            type = kPatArguments;
            break;
          }
          default: {
            type = kPatAlternate;
            break;
          }
        }
        break;
      }
      case '[': {
        type = kPatCollection;
        break;
      }
      case '{': {
        type = kPatBranch;
        break;
      }
      case '}': {
        if (is_branch) {
          type = kPatMissing;
        } else {
          type = kPatLiteral;
        }
        break;
      }
      case '$': {
        type = kPatEnviron;
        break;
      }
      case ',': {
        if (is_branch) {
          type = kPatMissing;
        } else {
          type = kPatLiteral;
        }
        break;
      }
      case '\\': {
        type = kPatLiteral;
        p++;
        break;
      }
      // Ends the whole command
      case NUL:
      case '|':
      case '\n':
      case '"': {
        // fallthrough
      }
      // Ends one pattern
      case ' ':
      case '\t': {
        type = kPatMissing;
        break;
      }
      case '~': {
        if (p == *pp) {
          type = kPatHome;
        } else {
          type = kPatLiteral;
        }
        break;
      }
      default: {
        type = kPatLiteral;
        break;
      }
    }
    if (type == kPatLiteral) {
      if (literal_start == NULL)
        literal_start = p;
      literal_length++;
#define GLOB_SPECIAL_CHARS "`#*?%\\[{}]$ \t"
#define IS_GLOB_SPECIAL(c) (strchr(GLOB_SPECIAL_CHARS, c) != NULL)
      // TODO Compare with vim
      if (*p == '\\' && IS_GLOB_SPECIAL(p[1]))
        p += 2;
      else
        p++;
    } else {
      if (literal_start != NULL) {
        char *s;
        const char *t;
        *next = pattern_alloc(kPatLiteral);
        (*next)->data.str = XCALLOC_NEW(char, literal_length + 1);
        s = (*next)->data.str;

        for (t = literal_start; t < p; t++) {
          if (*t == '\\' && IS_GLOB_SPECIAL(t[1])) {
            *s++ = t[1];
            t++;
          } else {
            *s++ = *t;
          }
        }
        literal_start = NULL;
        literal_length = 0;
        next = &((*next)->next);
      }
      if (type == kPatMissing)
        break;
      *next = pattern_alloc(type);
      switch (type) {
        case kGlobExpression: {
          ExpressionParserError expr_error;
          p += 2;
          if (((*next)->data.expr = parse_one_expression(&p, &expr_error,
                                                         &parse0_err,
                                                         col + (size_t)(p - s)))
              == NULL) {
            if (expr_error.message == NULL)
              goto get_glob_error_return;
            error->message = expr_error.message;
            error->position = expr_error.position;
            ret = NOTDONE;
            goto get_glob_error_return;
          }
          p++;
          break;
        }
        case kGlobShell: {
          *next = pattern_alloc(kGlobShell);
          const char *const init_p = p;
          p++;
          while (!ENDS_EXCMD(*p) && !(*p == '`' && p[-1] != '\\')) {
            p++;
          }
          if(*p != '`' || p == init_p + 1) {
            free_pattern(*next);
            memset(*next, 0, sizeof(**next));
            p = init_p + 1;
            literal_start = init_p;
            literal_length = 1;
            continue;
          } else {
            ((*next)->data.str = xmemdupz(init_p + 1,
                                          (size_t) (p - init_p) - 1));
            p++;
          }
          break;
        }
        case kPatHome:
        case kPatCurrent:
        case kPatAlternate:
        case kPatCharacter:
        case kPatAnything: {
          p++;
          break;
        }
        case kPatArguments:
        case kPatAnyRecurse: {
          p += 2;
          break;
        }
        case kPatOldFile: {
          p++;
          // fallthrough
        }
        case kPatBufname: {
          p++;
          (*next)->data.number = (int) getdigits(&p);
          break;
        }
        case kPatCollection: {
          p++;
          // FIXME
          break;
        }
        case kPatEnviron: {
          p++;
          const char *const init_p = p;
          const char *const end = find_env_end(&p);
          if (end == NULL) {
            free_pattern(*next);
            memset(*next, 0, sizeof(**next));
            literal_start = init_p - 1;
            p = init_p;
            literal_length = 1;
          } else {
            (*next)->data.str = xmemdupz(init_p, (size_t) (p - init_p));
          }
          break;
        }
        case kPatBranch: {
          const char *const init_p = p;
          int gcsp_ret = get_comma_separated_patterns(&p, error, next, is_glob,
                                                      col + (size_t) (p - s));
          if (gcsp_ret != OK) {
            ret = gcsp_ret;
            goto get_glob_error_return;
          }
          if (*p == '}' && (*next)->data.pats.next != NULL) {
            p++;
          } else {
            p = init_p;
            free_pattern(*next);
            memset(*next, 0, sizeof(**next));
            literal_start = p;
            p++;
            literal_length = 1;
            continue;
          }
          break;
        }
        case kPatAuList:
        case kPatMissing:
        case kPatLiteral: {
          assert(false);
        }
      }
      next = &((*next)->next);
    }
  }

  *pp = p;
  return OK;

get_glob_error_return:
  free_pattern(*pat);
  *pat = NULL;
  return ret;
}

/// Create new syntax error node
///
/// @param[out]  node      Place where created node will be saved.
/// @param[in]   error     Structure that describes the error. Is used to 
///                        populate node fields.
/// @param[in]   position  Position of errorred command.
/// @param[in]   s         Start of the string of errorred command.
///
/// @return Always returns OK.
static int create_error_node(CommandNode **node, CommandParserError *error,
                             CommandPosition position, const char *s)
{
  if (error->message != NULL) {
    *node = cmd_alloc(kCmdSyntaxError, position);
    (*node)->args[ARG_ERROR_LINESTR].arg.str = xstrdup(s);
    (*node)->args[ARG_ERROR_MESSAGE].arg.str = xstrdup(error->message);
    (*node)->args[ARG_ERROR_OFFSET].arg.col = (size_t) (error->position - s);
  }
  return OK;
}

/// Get virtual column for the first non-blank character
///
/// Tabs are considered to be 8 cells wide. Spaces are 1 cell wide. Other 
/// characters are considered non-blank.
///
/// @param[in,out]  pp  String for which indentation should be updated. Is 
///                     advanced to the first non-white character.
///
/// @return Offset of the first non-white character.
static int get_vcol(const char **pp)
  FUNC_ATTR_NONNULL_ALL FUNC_ATTR_WARN_UNUSED_RESULT
{
  int vcol = 0;
  const char *p = *pp;

  for (;;) {
    switch (*p++) {
      case ' ': {
        vcol++;
        continue;
      }
      case TAB: {
        vcol += 8 - vcol % 8;
        continue;
      }
      default: {
        break;
      }
    }
    break;
  }

  *pp = p - 1;
  return vcol;
}

/// Get regular expression
///
/// @param[in,out]  pp     String searched for a regular expression. Should 
///                        point to the first character of the regular 
///                        expression, *not* to the first character before it. 
///                        Is advanced to the character just after endch.
/// @param[out]     error  Structure where errors are saved.
/// @param[out]     regex  Location where regex is saved.
/// @param[in]      endch  Last character of the regex: character at which 
///                        regular expression should end (unless it was 
///                        escaped). Is expected to be either '?', '/' or NUL 
///                        (note: regex will in any case end on NUL, but using 
///                        NUL here will result in a faster code: NULs cannot 
///                        be escaped, so it just uses STRLEN to find regex 
///                        end).
/// @param[in]      no_end_message
///                        Error message which should be thrown if string ended, 
///                        but endch was not found. If NULL this situation is 
///                        considered to be OK.
///
/// @return FAIL in case of non-recoverable failure, NOTDONE in case of error, 
///         OK otherwise.
static int get_regex(const char **pp, CommandParserError *error,
                     Regex **regex, const char endch,
                     const char *const no_end_message)
  FUNC_ATTR_NONNULL_ARG(1,2,3) FUNC_ATTR_WARN_UNUSED_RESULT
{
  assert(endch != NUL || no_end_message == NULL);
  const char *p = *pp;
  const char *s = p;

  if (endch == NUL) {
    p += STRLEN(p);
  } else {
    while (*p != NUL && *p != endch) {
      if (*p == '\\' && p[1] != NUL)
        p += 2;
      else
        p++;
    }
  }

  *regex = regex_alloc(s, (size_t) (p - s));

  if (*p != NUL) {
    p++;
  } else if (no_end_message != NULL) {
    error->message = no_end_message;
    error->position = p;
    return NOTDONE;
  }

  *pp = p;

  return OK;
}

#define CMD_P_ARGS \
    const char **pp, \
    CommandNode *node, \
    CommandParserError *error, \
    CommandParserOptions o, \
    CommandPosition position, \
    VimlLineGetter fgetline, \
    void *cookie
#define CMD_P_DEF(f) int f(CMD_P_ARGS)

static CMD_P_DEF(parse_append)
  FUNC_ATTR_NONNULL_ALL FUNC_ATTR_WARN_UNUSED_RESULT
{
  garray_T *ga_strs = &(node->args[ARG_APPEND_LINES].arg.ga_strs);
  char *next_line;
  const char *first_nonblank;
  int vcol = -1;
  int cur_vcol = -1;

  ga_init(ga_strs, (int) sizeof(char *), 3);

  while ((next_line = fgetline(':', cookie, vcol == -1 ? 0 : vcol)) != NULL) {
    first_nonblank = next_line;
    if (vcol == -1) {
      vcol = get_vcol(&first_nonblank);
    } else {
      cur_vcol = get_vcol(&first_nonblank);
    }
    if (first_nonblank[0] == '.' && first_nonblank[1] == NUL
        && cur_vcol <= vcol) {
      free(next_line);
      break;
    }
    ga_grow(ga_strs, 1);
    ((char **)(ga_strs->ga_data))[ga_strs->ga_len++] = (char *) next_line;
  }

  return OK;
}

/// Set RHS of :map/:abbrev/:menu node
///
/// @param[in]      rhs      Right hand side of the command.
/// @param[in]      rhs_idx  Offset of RHS argument in node->args array.
/// @parblock
///   @note rhs_idx + 1 is expected to point to parsed variant of RHS (for 
///         <expr>-type mappings)). If parser error occurred during running 
///         expression node->children will point to syntax error node.
/// @endparblock
/// @param[in,out]  node     Node whose argument rhs should be saved to.
/// @param[in]      special  true if explicit <special> was supplied.
/// @param[in]      is_expr  true if it is <expr>-type mapping.
/// @parblock
///   @note If this argument is always false then you do not need to care about 
///         rhs_idx + 1 and node->children.
/// @endparblock
/// @param[in]      o         Options that control parsing behavior.
/// @param[in]      position  Position of input.
///
/// @return FAIL when out of memory, OK otherwise.
static int set_node_rhs(const char *rhs, size_t rhs_idx, CommandNode *node,
                        bool special, bool is_expr, CommandParserOptions o,
                        CommandPosition position)
{
  char *rhs_buf;
  char *new_rhs;

  new_rhs = replace_termcodes(rhs, STRLEN(rhs), &rhs_buf, false, true, special,
                              FLAG_POC_TO_FLAG_CPO(o.flags));
  if (rhs_buf == NULL)
    return FAIL;

  if ((o.flags&FLAG_POC_ALTKEYMAP) && (o.flags&FLAG_POC_RL))
    lrswap(new_rhs);

  if (is_expr) {
    ExpressionParserError expr_error;
    Expression *expr = NULL;
    const char *rhs_end = new_rhs;

    expr_error.position = NULL;
    expr_error.message = NULL;

    if ((expr = parse_one_expression(&rhs_end, &expr_error, &parse0_err,
                                     position.col)) == NULL) {
      CommandParserError error;
      if (expr_error.message == NULL) {
        free(new_rhs);
        return FAIL;
      }
      error.position = expr_error.position;
      error.message = expr_error.message;
      if (create_error_node(&(node->children), &error, position, new_rhs)
          == FAIL) {
        free(new_rhs);
        return FAIL;
      }
    } else if (*rhs_end != NUL) {
      CommandParserError error;

      free_expr(expr);

      error.position = rhs_end;
      error.message = N_("E15: trailing characters");

      if (create_error_node(&(node->children), &error, position, new_rhs)
          == FAIL) {
        free(new_rhs);
        return FAIL;
      }
    } else {
      node->args[rhs_idx + 1].arg.expr = expr;
    }

    free(new_rhs);
  } else {
    node->args[rhs_idx].arg.str = new_rhs;
  }
  return OK;
}

/// Parse :*map/:*abbrev/:*unmap/:*unabbrev commands
///
/// @param[in,out]  pp        Parsed string. Is advanced to the end of the 
///                           string unless a error occurred. Must point to the 
///                           first non-white character of the command argument.
/// @param[out]     node      Location where parsing results are saved.
/// @param[out]     error     Structure where errors are saved.
/// @param[in]      o         Options that control parsing behavior.
/// @param[in]      position  Position of input.
/// @param[in]      fgetline  Function used to obtain the next line. Not used.
/// @param[in,out]  cookie    Argument to the above function. Not used.
/// @param[in]      unmap     Determines whether :*map/:*abbrev or 
///                           :*unmap/:*unabbrev command is being parsed.
///
/// @return FAIL when out of memory, OK otherwise.
static int do_parse_map(CMD_P_ARGS, bool unmap)
{
  uint_least32_t map_flags = 0;
  const char *p = *pp;
  const char *lhs;
  const char *lhs_end;
  const char *rhs;
  char *lhs_buf;
  bool do_backslash = !(o.flags&FLAG_POC_CPO_BSLASH);

  for (;;) {
    if (*p != '<')
      break;

    switch (p[1]) {
      case 'b': {
        // Check for "<buffer>": mapping local to buffer.
        if (STRNCMP(p, "<buffer>", 8) == 0) {
          p = skipwhite(p + 8);
          map_flags |= FLAG_MAP_BUFFER;
          continue;
        }
        break;
      }
      case 'n': {
        // Check for "<nowait>": don't wait for more characters.
        if (STRNCMP(p, "<nowait>", 8) == 0) {
          p = skipwhite(p + 8);
          map_flags |= FLAG_MAP_NOWAIT;
          continue;
        }
        break;
      }
      case 's': {
        // Check for "<silent>": don't echo commands.
        if (STRNCMP(p, "<silent>", 8) == 0) {
          p = skipwhite(p + 8);
          map_flags |= FLAG_MAP_SILENT;
          continue;
        }

        // Check for "<special>": accept special keys in <>
        if (STRNCMP(p, "<special>", 9) == 0) {
          p = skipwhite(p + 9);
          map_flags |= FLAG_MAP_SPECIAL;
          continue;
        }

        // Check for "<script>": remap script-local mappings only
        if (STRNCMP(p, "<script>", 8) == 0) {
          p = skipwhite(p + 8);
          map_flags |= FLAG_MAP_SCRIPT;
          continue;
        }
        break;
      }
      case 'e': {
        // Check for "<expr>": {rhs} is an expression.
        if (STRNCMP(p, "<expr>", 6) == 0) {
          p = skipwhite(p + 6);
          map_flags |= FLAG_MAP_EXPR;
          continue;
        }
        break;
      }
      case 'u': {
        // Check for "<unique>": don't overwrite an existing mapping.
        if (STRNCMP(p, "<unique>", 8) == 0) {
          p = skipwhite(p + 8);
          map_flags |= FLAG_MAP_UNIQUE;
          continue;
        }
        break;
      }
      default: {
        break;
      }
    }
    break;
  }
  node->args[ARG_MAP_FLAGS].arg.flags = map_flags;

  lhs = p;
  while (*p && (unmap || !vim_iswhite(*p))) {
    if ((p[0] == Ctrl_V || (do_backslash && p[0] == '\\')) &&
        p[1] != NUL)
      p++;                      // skip CTRL-V or backslash
    p++;
  }

  lhs_end = p;
  p = skipwhite(p);
  rhs = p;

  p += STRLEN(p);

  if (*lhs != NUL) {
    // Note: type of the abbreviation is not checked because it depends on the 
    //       &iskeyword option. Unlike $ENV parsing (which depends on the 
    //       options too) it is not unlikely that both 1. file will be parsed 
    //       before result is actually used and 2. option value at the execution 
    //       stage will make results invalid.
    lhs = replace_termcodes(lhs, (size_t) (lhs_end - lhs), &lhs_buf, true, true,
                            map_flags&FLAG_MAP_SPECIAL,
                            FLAG_POC_TO_FLAG_CPO(o.flags));
    if (lhs_buf == NULL)
      return FAIL;
    node->args[ARG_MAP_LHS].arg.str = (char *) lhs;
  }

  if (*rhs != NUL) {
    assert(!unmap);
    if (STRICMP(rhs, "<nop>") == 0) {
      // Empty string
      rhs = XCALLOC_NEW(char, 1);
    } else {
      if (set_node_rhs(rhs, ARG_MAP_RHS, node, map_flags&FLAG_MAP_SPECIAL,
                       map_flags&FLAG_MAP_EXPR, o, position)
          == FAIL)
        return FAIL;
    }
  }

  *pp = p;
  return OK;
}

static CMD_P_DEF(parse_map)
{
  return do_parse_map(pp, node, error, o, position, fgetline, cookie, false);
}

static CMD_P_DEF(parse_unmap)
{
  return do_parse_map(pp, node, error, o, position, fgetline, cookie, true);
}

static CMD_P_DEF(parse_mapclear)
{
  bool local;

  local = (STRCMP(*pp, "<buffer>") == 0);
  if (local)
    *pp += 8;

  node->args[ARG_CLEAR_BUFFER].arg.flags = local;

  return OK;
}

/// Unescape characters
///
/// It is expected to be called on a string in allocated memory that is safe to 
/// alter. This function only removes "\\" charaters from the string, modifying 
/// memory in-place.
///
/// @param[in,out]  p        String being unescaped.
/// @param[in]      unctrlv  Treat <C-v> as escape character.
static void menu_unescape(char *p, bool unctrlv)
{
  while (*p) {
    if ((*p == '\\' || (*p == Ctrl_V && unctrlv)) && p[1] != NUL)
      STRMOVE(p, p + 1);
    p++;
  }
}

/// Parse menu name
///
/// @param[in,out]  pp         Parsed string.
/// @param[out]     error      Address where parsing errors are saved.
/// @param[in]      unmenu     True when parsing :unmenu.
/// @param[out]     menu_item  Address where allocated menu is saved.
/// @param[out]     menu_text  Address where left text is saved.
///
/// @return OK in case of success, NOTDONE in case of error, FAIL in case of 
///         unrecoverable error.
static int parse_menu_name(const char **pp, CommandParserError *error,
                           MenuNameParsingOptions mnpo, MenuItem **menu_item,
                           char **menu_text)
{
  MenuItem *sub = NULL;
  const char *p = *pp;
  const char *s = NULL;
  const char *menu_path_end = NULL;
  const char *menu_path = p;
  MenuItem **next = menu_item;

  if (*menu_path == '.') {
    error->message = N_("E475: Expected menu name");
    error->position = menu_path;
    return NOTDONE;
  }

  while (*p && (mnpo == kMenuWholeCmd || !vim_iswhite(*p))) {
    if ((*p == '\\' || *p == Ctrl_V) && p[1] != NUL) {
      p++;
      if (*p == TAB && mnpo != kMenuWholeCmd) {
        s = p + 1;
        menu_path_end = p - 2;
      }
    } else if (STRNICMP(p, "<TAB>", 5) == 0 && mnpo != kMenuWholeCmd) {
      menu_path_end = p - 1;
      p += 4;
      s = p + 1;
    } else if (*p == '.' && s == NULL) {
      menu_path_end = p - 1;
      if (menu_path_end == menu_path) {
        error->message = N_("E792: Empty menu name");
        error->position = p;
        return NOTDONE;
      }
    } else if ((!p[1] || (mnpo != kMenuWholeCmd && vim_iswhite(p[1])))
               && p != menu_path && s == NULL) {
      menu_path_end = p;
    }
    if (menu_path_end != NULL) {
      sub = XCALLOC_NEW(MenuItem, 1);

      *next = sub;
      next = &(sub->subitem);

      sub->name = xmemdupz(menu_path, (size_t) (menu_path_end - menu_path) + 1);

      menu_unescape(sub->name, mnpo == kMenuWholeCmd);

      menu_path = p + 1;
      menu_path_end = NULL;
    }
    p++;
  }

  if (s != NULL) {
    char *text;
    if (*menu_item == NULL) {
      error->message = N_("E792: Empty menu name");
      error->position = s;
      return NOTDONE;
    }

    if (mnpo == kMenuDefaults) {
      text = xmemdupz(s, (size_t) (p - s));

      menu_unescape(text, false);

      *menu_text = text;
    }
  }
  *pp = p;
  return OK;
}

/// Parse :*menu/:*unmenu
///
/// @param[in,out]  pp        Parsed string. Is advanced to the end of the 
///                           string unless a error occurred. Must point to the 
///                           first non-white character of the command argument.
/// @param[out]     node      Location where parsing results are saved.
/// @param[out]     error     Structure where errors are saved.
/// @param[in]      o         Options that control parsing behavior.
/// @param[in]      position  Position of input.
/// @param[in]      fgetline  Function used to obtain the next line. Not used.
/// @param[in,out]  cookie    Argument to the above function. Not used.
/// @param[in]      unmenu    Determines whether :*menu or :*unmenu command is 
///                           being parsed.
///
/// @return FAIL when out of memory, OK otherwise.
static int do_parse_menu(CMD_P_ARGS, bool unmenu)
{
  // FIXME "menu *" parses to something weird
  uint_least32_t menu_flags = 0;
  const char *p = *pp;
  size_t i;
  const char *s;
  const char *map_to;

  for (;;) {
    if (STRNCMP(p, "<script>", 8) == 0) {
      menu_flags |= FLAG_MENU_SCRIPT;
      p = skipwhite(p + 8);
      continue;
    }
    if (STRNCMP(p, "<silent>", 8) == 0) {
      menu_flags |= FLAG_MENU_SILENT;
      p = skipwhite(p + 8);
      continue;
    }
    if (STRNCMP(p, "<special>", 9) == 0) {
      menu_flags |= FLAG_MENU_SPECIAL;
      p = skipwhite(p + 9);
      continue;
    }
    break;
  }

  // Locate an optional "icon=filename" argument.
  if (STRNCMP(p, "icon=", 5) == 0) {
    char *icon;

    p += 5;
    s = p;

    while (*p != NUL && *p != ' ') {
      if (*p == '\\')
        p++;
      mb_ptr_adv_(p);
    }

    icon = xmemdupz(s, (size_t) (p - s));

    menu_unescape(icon, false);

    node->args[ARG_MENU_ICON].arg.str = icon;

    if (*p != NUL)
      p = skipwhite(p);
  }

  for (s = p; *s; ++s)
    if (!VIM_ISDIGIT(*s) && *s != '.')
      break;

  if (vim_iswhite(*s)) {
    int *pris;
    i = 0;
    for (; i < MENUDEPTH && !vim_iswhite(*p); i++) {
    }
    if (i) {
      if (unmenu) {
        for (i = 0; i < MENUDEPTH && !vim_iswhite(*p); i++) {
          p = skipdigits(p);
          if (*p == '.') {
            p++;
          }
        }
      } else {
        pris = XCALLOC_NEW(int, i + 1);
        node->args[ARG_MENU_PRI].arg.numbers = pris;
        for (i = 0; i < MENUDEPTH && !vim_iswhite(*p); i++) {
          pris[i] = (int) getdigits(&p);
          if (pris[i] == 0) {
            pris[i] = MENU_DEFAULT_PRI;
          }
          if (*p == '.') {
            p++;
          }
        }
      }
    }
    p = skipwhite(p);
  }

  if (STRNCMP(p, "enable", 6) == 0 && vim_iswhite(p[6])) {
    menu_flags |= FLAG_MENU_ENABLE;
    p = skipwhite(p + 6);
  } else if (STRNCMP(p, "disable", 7) == 0 && vim_iswhite(p[7])) {
    menu_flags |= FLAG_MENU_DISABLE;
    p = skipwhite(p + 7);
  }

  if (!unmenu) {
    node->args[ARG_MENU_FLAGS].arg.flags = menu_flags;
  }

  if (*p == NUL) {
    *pp = p;
    return OK;
  }

  const char *const menu_path = p;
  const size_t name_idx = (unmenu ? ARG_UNMENU_LHS : ARG_MENU_NAME);
  int pmn_ret = parse_menu_name(&p, error,
                                unmenu ? kMenuIgnoreText : kMenuDefaults,
                                &(node->args[name_idx].arg.menu_item),
                                unmenu
                                ? NULL
                                : &(node->args[ARG_MENU_TEXT].arg.str));
  if (pmn_ret != OK) {
    return pmn_ret;
  }

  p = skipwhite(p);

  if (unmenu) {
    *pp = p;
    return OK;
  }

  map_to = p;

  p += STRLEN(p);

  // FIXME More checks
  if (*map_to != NUL) {
    if (node->args[ARG_MENU_NAME].arg.menu_item == NULL) {
      error->message = N_("E792: Empty menu name");
      error->position = menu_path;
      return NOTDONE;
    } else if (node->args[ARG_MENU_NAME].arg.menu_item->subitem == NULL) {
      error->message = N_("E331: Must not add menu items directly to menu bar");
      error->position = menu_path;
      return NOTDONE;
    }
    if (set_node_rhs(map_to, ARG_MENU_RHS, node, menu_flags&FLAG_MENU_SPECIAL, 
                     false, o, position) == FAIL)
      return FAIL;
  }

  *pp = p;
  return OK;
}

static CMD_P_DEF(parse_menu)
{
  return do_parse_menu(pp, node, error, o, position, fgetline, cookie, false);
}

static CMD_P_DEF(parse_unmenu)
{
  return do_parse_menu(pp, node, error, o, position, fgetline, cookie, true);
}

/// Parse an expression or a whitespace-separated sequence of expressions
///
/// Used for ":execute" and ":echo*"
///
/// @param[in,out]  pp        Parsed string. Is advanced to the end of the last 
///                           expression. Should point to the first character of 
///                           the expression (may point to whitespace 
///                           character).
/// @param[out]     node      Location where parsing results are saved.
/// @param[out]     error     Structure where errors are saved.
/// @param[in]      o         Options that control parsing behavior.
/// @param[in]      position  Position of input.
/// @param[in]      multi     Determines whether parsed expression is actually 
///                           a sequence of expressions.
///
/// @return FAIL if out of memory, NOTDONE in case of error, OK otherwise.
static int do_parse_expr_cmd(const char **pp,
                             CommandNode *node,
                             CommandParserError *error,
                             CommandParserOptions o,
                             CommandPosition position,
                             bool multi)
{
  Expression *expr;
  ExpressionParserError expr_error;

  if (multi) {
    expr = parse_many_expressions(pp, &expr_error, &parse0_err, position.col,
                                  false, "\n|");
  } else {
    expr = parse_one_expression(pp, &expr_error, &parse0_err, position.col);
  }

  if (expr == NULL) {
    if (expr_error.message == NULL) {
      return FAIL;
    }
    error->message = expr_error.message;
    error->position = expr_error.position;
    return NOTDONE;
  }

  node->args[ARG_EXPR_EXPR].arg.expr = expr;

  return OK;
}

static CMD_P_DEF(parse_expr_cmd)
{
  if (node->type == kCmdReturn && ENDS_EXCMD_NOCOMMENT(**pp)) {
    return OK;
  }
  return do_parse_expr_cmd(pp, node, error, o, position, false);
}

static CMD_P_DEF(parse_call)
{
  const char *s = *pp;
  int ret = do_parse_expr_cmd(pp, node, error, o, position, false);
  if (ret == OK
      && node->args[ARG_EXPR_EXPR].arg.expr->node->type != kExprCall) {
    error->message = N_("E129: :call accepts only function calls");
    error->position = s;
    return NOTDONE;
  }
  return ret;
}

static CMD_P_DEF(parse_expr_seq_cmd)
{
  if (ENDS_EXCMD_NOCOMMENT(**pp)) {
    return OK;
  }
  return do_parse_expr_cmd(pp, node, error, o, position, true);
}

static CMD_P_DEF(parse_rest_line)
{
  size_t len;
  if (**pp == NUL) {
    if (node->type == kCmdCstag) {
      error->message = "E562: Usage: cstag <ident>";
    } else {
      error->message = (char *) e_argreq;
    }
    error->position = *pp;
    return NOTDONE;
  }

  len = STRLEN(*pp);

  node->args[0].arg.str = xmemdupz(*pp, len);
  *pp += len;
  return OK;
}

static CMD_P_DEF(parse_rest_allow_empty)
{
  if (**pp == NUL)
    return OK;

  return parse_rest_line(pp, node, error, o, position, fgetline, cookie);
}

static const char *do_fgetline(int c, const char **arg, int indent)
{
  if (*arg) {
    const char *result;
    result = xstrdup(*arg);
    *arg = NULL;
    return result;
  } else {
    return NULL;
  }
}

static CMD_P_DEF(parse_do)
{
  CommandNode *cmd;
  const char *arg = *pp;
  CommandPosition new_position = {
    1,
    1
  };

  if ((cmd = parse_cmd_sequence(o, new_position, (VimlLineGetter) &do_fgetline,
                                &arg, false))
      == NULL) {
    return FAIL;
  }

  node->children = cmd;

  *pp += STRLEN(*pp);

  return OK;
}

/// Check whether given expression node is a valid lvalue
///
/// @param[in]   s            String that holds original representation of 
///                           parsed expression.
/// @param[in]   node         Checked expression.
/// @param[out]  error        Structure where error information is saved.
/// @param[in]   allow_list   Determines whether list nodes are allowed.
/// @param[in]   allow_lower  Determines whether simple variable names are 
///                           allowed to start with a lowercase letter.
/// @param[in]   allow_env    Determines whether it is allowed to contain 
///                           kExprOption, kExprRegister and 
///                           kExprEnvironmentVariable nodes.
///
/// @return true if check failed, false otherwise.
static bool check_lval(const char *const s, ExpressionNode *node,
                       CommandParserError *error, bool allow_list,
                       bool allow_lower, bool allow_env)
{
  switch (node->type) {
    case kExprSimpleVariableName: {
      if (!allow_lower
          && ASCII_ISLOWER(s[node->start])
          && s[node->start + 1] != ':'  // Fast check: most functions 
                                        // containing colon contain it in the 
                                        // second character
          && memchr((void *) (s + node->start), '#',
                    node->end - node->start + 1) == NULL
          // FIXME? Though foo:bar works in Vim Bram said it was never 
          //        intended to work.
          && memchr((void *) (s + node->start), ':',
                    node->end - node->start + 1) == NULL) {
        error->message =
            N_("E128: Function name must start with a capital "
               "or contain a colon or a hash");
        error->position = s + node->start;
        return true;
      }
      break;
    }
    case kExprEnvironmentVariable: {
      if (allow_env) {
        if (node->start > node->end) {
          error->message = N_("E475: Cannot assign to environment variable "
                              "with an empty name");
          error->position = s + node->end;
          return true;
        }
      }
      // fallthrough
    }
    case kExprOption:
    case kExprRegister: {
      if (!allow_env) {
        error->message = N_("E15: Only variable names are allowed");
        error->position = s + node->start;
        return true;
      }
      break;
    }
    case kExprListRest: {
      break;
    }
    case kExprVariableName: {
      break;
    }
    case kExprConcatOrSubscript:
    case kExprSubscript: {
      for (ExpressionNode *root = node; root->children != NULL;
           root = root->children) {
        switch (root->type) {
          case kExprConcatOrSubscript:
          case kExprSubscript: {
            continue;
          }
          case kExprVariableName:
          case kExprSimpleVariableName: {
            break;
          }
          default: {
            error->message =
                N_("E475: Expected variable name or a list of variable names");
            error->position = s + root->start;
            return true;
          }
        }
        break;
      }
      break;
    }
    case kExprList: {
      if (allow_list) {
        ExpressionNode *item = node->children;

        if (item == NULL) {
          error->message =
              N_("E475: Expected non-empty list of variable names");
          error->position = s + node->start;
          return true;
        }

        while (item != NULL) {
          if (check_lval(s, item, error, false, allow_lower, allow_env))
            return true;
          item = item->next;
        }
      } else {
        error->message = N_("E475: Expected variable name");
        error->position = s + node->start;
        return true;
      }
      break;
    }
    default: {
      if (allow_list) {
        error->message =
            N_("E475: Expected variable name or a list of variable names");
      } else {
        error->message = N_("E475: Expected variable name");
      }
      error->position = s + node->start;
      return true;
    }
  }
  return false;
}

#define FLAG_PLVAL_SPACEMULT 0x01
#define FLAG_PLVAL_LISTMULT  0x02
#define FLAG_PLVAL_NOLOWER   0x04
#define FLAG_PLVAL_ALLOW_ENV 0x08

/// Parse left value of assignment
///
/// @param[in,out]  pp          Parsed string. Is advanced to the next 
///                             character after parsed expression.
/// @param[out]     error       Structure where errors are saved.
/// @param[in]      o           Options that control parsing behavior.
/// @param[in]      position    Position of input.
/// @param[out]     expr        Location where result will be saved.
/// @param[in]      flags       Flags:
/// @parblock
///   Flag                 | Description
///   -------------------- | -------------------------------------------------
///   FLAG_PLVAL_SPACEMULT | Allow space-separated multiple values
///   FLAG_PLVAL_LISTMULT  | Allow multiple values in a list ("[a, b]")
///   FLAG_PLVAL_NOLOWER   | Do not allow name to start with a lowercase letter
///   FLAG_PLVAL_ALLOW_ENV | Allow options, env variables and registers
///
///   @note If both FLAG_PLVAL_LISTMULT and FLAG_PLVAL_SPACEMULT were 
///         specified then only either space-separated values or list will be 
///         allowed, but not both.
/// @endparblock
///
/// @return OK if parsing was successfull, NOTDONE if it was not, FAIL when 
///         out of memory.
static int parse_lval(const char **pp,
                      CommandParserError *error,
                      CommandParserOptions o,
                      size_t col,
                      Expression **expr,
                      int flags)
{
  ExpressionParserError expr_error;
  const char *s = *pp;
  bool allow_list = (bool) (flags&FLAG_PLVAL_LISTMULT);
  bool allow_env = (bool) (flags&FLAG_PLVAL_ALLOW_ENV);

  if (flags&FLAG_PLVAL_SPACEMULT) {
    *expr = parse_many_expressions(pp, &expr_error, &parse7_nofunc, col, true,
                                   "\n\"|-.+=");
  } else {
    *expr = parse_one_expression(pp, &expr_error, &parse7_nofunc, col);
  }

  if (*expr == NULL) {
    if (expr_error.message == NULL) {
      return FAIL;
    }
    error->message = expr_error.message;
    error->position = expr_error.position;
    return NOTDONE;
  }

  if ((*expr)->node->next != NULL) {
    allow_list = false;
    allow_env = false;
  }

  for (ExpressionNode *next = (*expr)->node; next != NULL; next = next->next) {
    if (check_lval((*expr)->string, next, error, allow_list,
                   !(flags&FLAG_PLVAL_NOLOWER), allow_env)) {
      free_expr(*expr);
      *expr = NULL;
      if (error->message == NULL) {
        return FAIL;
      }
      if (error->position == NULL) {
        error->position = s;
      }
      return NOTDONE;
    }
  }

  return OK;
}

static CMD_P_DEF(parse_lvals)
{
  Expression *expr;
  int ret;

  if ((ret = parse_lval(pp, error, o, position.col, &expr,
                        node->type == kCmdDelfunction
                        ? FLAG_PLVAL_NOLOWER
                        : FLAG_PLVAL_SPACEMULT)) == FAIL) {
    return FAIL;
  }

  node->args[ARG_EXPRS_EXPRS].arg.expr = expr;

  if (ret == NOTDONE) {
    return NOTDONE;
  }

  return OK;
}

static CMD_P_DEF(parse_lockvar)
{
  if (VIM_ISDIGIT(**pp)) {
    node->args[ARG_LOCKVAR_DEPTH].arg.unumber = (unsigned) getdigits(pp);
  }

  return parse_lvals(pp, node, error, o, position, fgetline, cookie);
}

static CMD_P_DEF(parse_for)
{
  const char *const s = *pp;
  Expression *expr;
  Expression *list_expr;
  ExpressionParserError expr_error;
  int ret;

  if ((ret = parse_lval(pp, error, o, position.col, &expr, FLAG_PLVAL_LISTMULT))
      == FAIL) {
    return FAIL;
  }

  node->args[ARG_FOR_LHS].arg.expr = expr;

  if (ret == NOTDONE) {
    return NOTDONE;
  }

  *pp = skipwhite(*pp);

  if ((*pp)[0] != 'i' || (*pp)[1] != 'n') {
    error->message = N_("E690: Missing \"in\" after :for");
    error->position = *pp;
    return NOTDONE;
  }

  *pp = skipwhite(*pp + 2);

  if ((list_expr = parse_one_expression(pp, &expr_error, &parse0_err,
                                        position.col + (size_t) (*pp - s)))
      == NULL) {
    if (expr_error.message == NULL) {
      return FAIL;
    }
    error->message = expr_error.message;
    error->position = expr_error.position;
    return NOTDONE;
  }

  node->args[ARG_FOR_RHS].arg.expr = list_expr;

  return OK;
}

static CMD_P_DEF(parse_function)
{
  const char *p = *pp;
  Expression *expr;
  int ret;
  garray_T *args = &(node->args[ARG_FUNC_ARGS].arg.ga_strs);
  uint_least32_t flags = 0;
  bool mustend = false;

  if (ENDS_EXCMD(*p))
    return OK;

  if (*p == '/') {
    return get_regex(&p, error, &(node->args[ARG_FUNC_REG].arg.reg), '/', NULL);
  }

  if ((ret = parse_lval(&p, error, o, position.col + (size_t) (p - *pp), &expr,
                        FLAG_PLVAL_NOLOWER))
      == FAIL) {
    return FAIL;
  }

  node->args[ARG_FUNC_NAME].arg.expr = expr;

  if (ret == NOTDONE) {
    return NOTDONE;
  }

  p = skipwhite(p);

  if (*p != '(') {
    if (!ENDS_EXCMD(*p)) {
      error->message = N_("E124: Missing '('");
      error->position = p;
      return NOTDONE;
    }
    *pp = p;
    return OK;
  }

  p = skipwhite(p + 1);

  ga_init(args, (int) sizeof(char *), 3);

  while (*p != ')') {
    char *notend_message = N_("E475: Expected end of arguments list");
    if (p[0] == '.' && p[1] == '.' && p[2] == '.') {
      flags |= FLAG_FUNC_VARARGS;
      p += 3;
      mustend = true;
    } else {
      const char *arg_start = p;
      const char *arg;
      int i;

      while (ASCII_ISALNUM(*p) || *p == '_')
        p++;

      if (arg_start == p)
        error->message = N_("E125: Argument expected, got nothing");
      else if (VIM_ISDIGIT(*arg_start))
        error->message =
            N_("E125: Function argument cannot start with a digit");
      else if ((p - arg_start == 9 && STRNCMP(arg_start, "firstline", 9) == 0)
               ||
               (p - arg_start == 8 && STRNCMP(arg_start, "lastline", 8) == 0))
        error->message =
            N_("E125: Names \"firstline\" and \"lastline\" are reserved");
      else
        error->message = NULL;

      if (error->message != NULL) {
        error->position = arg_start;
        return NOTDONE;
      }

      arg = xmemdupz(arg_start, (size_t) (p - arg_start));

      for (i = 0; i < args->ga_len; i++)
        if (STRCMP(((char **)(args->ga_data))[i], arg) == 0) {
          error->message = N_("E853: Duplicate argument name: %s");
          error->position = arg_start;
          return FAIL;
        }

      ga_grow(args, 1);
#if 0
      if (ga_grow(args, 1) == FAIL) {
        ga_clear_strings(args);
        free(arg);
        return FAIL;
      }
#endif

      ((char **)(args->ga_data))[args->ga_len++] = (char *) arg;
    }
    if (*p == ',') {
      p++;
    } else {
      mustend = true;
      notend_message = N_("E475: Expected end of arguments list or comma");
    }
    p = skipwhite(p);
    if (mustend && *p != ')') {
      error->message = notend_message;
      error->position = p;
      return NOTDONE;
    }
  }
  p++;  // Skip the ')'

  // find extra arguments "range", "dict" and "abort"
  for (;;) {
    p = skipwhite(p);
    if (STRNCMP(p, "range", 5) == 0) {
      flags |= FLAG_FUNC_RANGE;
      p += 5;
    } else if (STRNCMP(p, "dict", 4) == 0) {
      flags |= FLAG_FUNC_DICT;
      p += 4;
    } else if (STRNCMP(p, "abort", 5) == 0) {
      flags |= FLAG_FUNC_ABORT;
      p += 5;
    } else {
      break;
    }
  }

  node->args[ARG_FUNC_FLAGS].arg.flags = flags;

  *pp = p;
  return OK;
}

static CMD_P_DEF(parse_let)
{
  const char *const s = *pp;
  Expression *expr;
  ExpressionParserError expr_error;
  int ret;
  Expression *rval_expr;
  LetAssignmentType ass_type = 0;

  if (ENDS_EXCMD(**pp)) {
    return OK;
  }

  if ((ret = parse_lval(pp, error, o, position.col, &expr,
                        FLAG_PLVAL_LISTMULT|FLAG_PLVAL_SPACEMULT
                        |FLAG_PLVAL_ALLOW_ENV))
      == FAIL) {
    return FAIL;
  }

  node->args[ARG_LET_LHS].arg.expr = expr;

  if (ret == NOTDONE) {
    return NOTDONE;
  }

  *pp = skipwhite(*pp);

  if (ENDS_EXCMD(**pp)) {
    if (expr->node->type == kExprList) {
      error->message =
          N_("E474: To list multiple variables use \":let var|let var2\", "
                   "not \":let [var, var2]\"");
      error->position = *pp;
      return NOTDONE;
    }
    return OK;
  } else {
    if (expr->node->next != NULL) {
      error->message = N_("E18: Expected end of command after last variable");
      error->position = *pp;
      return NOTDONE;
    }
  }

  switch (**pp) {
    case '=': {
      (*pp)++;
      ass_type = VAL_LET_ASSIGN;
      break;
    }
    case '+':
    case '-':
    case '.': {
      switch (**pp) {
        case '+': {
          ass_type = VAL_LET_ADD;
          break;
        }
        case '-': {
          ass_type = VAL_LET_SUBTRACT;
          break;
        }
        case '.': {
          ass_type = VAL_LET_APPEND;
          break;
        }
      }
      if ((*pp)[1] != '=') {
        error->message = N_("E18: '+', '-' and '.' must be followed by '='");
        error->position = *pp;
        return NOTDONE;
      }
      *pp += 2;
      break;
    }
    default: {
      error->message =
          N_("E18: Expected assignment operation or end of command");
      error->position = *pp;
      return NOTDONE;
    }
  }

  node->args[ARG_LET_ASS_TYPE].arg.flags = (uint_least32_t) ass_type;

  *pp = skipwhite(*pp);

  if ((rval_expr = parse_one_expression(pp, &expr_error, &parse0_err,
                                        position.col + (size_t) (*pp - s)))
      == NULL) {
    if (expr_error.message == NULL) {
      return FAIL;
    }
    error->message = expr_error.message;
    error->position = *pp;
    return NOTDONE;
  }

  node->args[ARG_LET_RHS].arg.expr = rval_expr;

  return OK;
}

static CMD_P_DEF(parse_scriptencoding)
{
  if (**pp == NUL)
    return OK;
  // TODO Setup conversion from parsed encoding
  if ((node->args[0].arg.str = enc_canonize(*pp)) == NULL)
    return FAIL;
  *pp += STRLEN(*pp);
  return OK;
}

static AuEvent *parse_events(const char **pp, CommandParserError *error)
{
  const char *p = *pp;
  if (*p == '*') {
    if (p[1] != NUL && !vim_iswhite(p[1])) {
      error->message = N_("E215: Illegal character after *");
      error->position = p + 1;
      return NULL;
    }
    AuEvent *events = XCALLOC_NEW(AuEvent, 2);
    events[0] = ANY_EVENT;
    events[1] = NO_EVENT;
    p = skipwhite(p + 1);
    *pp = p;
    return events;
  }
  garray_T ga;
  ga_init(&ga, (int) sizeof(AuEvent), 1);
  AuEvent event = NO_EVENT;
  do {
    event = find_event(&p);
    GA_APPEND(AuEvent, &ga, event);
    if (*p == ',') {
      p++;
    } else {
      break;
    }
  } while (event != NO_EVENT);
  GA_APPEND(AuEvent, &ga, NO_EVENT);
  if (!(vim_iswhite(*p) || *p == NUL)) {
    ga_clear(&ga);
    return NULL;
  }
  p = skipwhite(p);
  *pp = p;
  return (AuEvent *) ga.ga_data;
}

/// Parse event and group names for :au and :doau
///
/// @param[in,out]  pp      Parsed string.
/// @param[out]     error   Address where errors are saved.
/// @param[out]     events  Address where found events are saved.
/// @param[out]     group   Address where group name is saved.
///
/// @return OK in case of success, NOTDONE in case of syntax error and FAIL in 
///         case of non-recoverable error.
static int parse_group_and_event(const char **pp,
                                 CommandParserError *error,
                                 AuEvent **events,
                                 char **group)
  FUNC_ATTR_NONNULL_ALL FUNC_ATTR_WARN_UNUSED_RESULT
{
  const char *p = *pp;
  *group = NULL;
  *events = parse_events(&p, error);
  if (error->message != NULL) {
    return NOTDONE;
  }
  if (*events == NULL) {
    const char *const start = p;
    while (!vim_iswhite(*p) && *p) {
      p++;
    }
    *group = xmemdupz(start, (size_t) (p - start));
    p = skipwhite(p);
    if (*p) {
      *events = parse_events(&p, error);
      if (error->message != NULL) {
        return NOTDONE;
      }
      if (events == NULL) {
        error->message = N_("E216: No such event");
        error->position = p;
        return NOTDONE;
      }
    }
  }
  *pp = p;
  return OK;
}

static CMD_P_DEF(parse_autocmd)
{
  if (**pp == NUL) {
    return OK;
  }
  const char *p = *pp;
  const char *const s = p;
  int pgeret;
  if ((pgeret = parse_group_and_event(&p,
                                      error,
                                      &(node->args[ARG_AU_EVENTS].arg.events),
                                      &(node->args[ARG_AU_GROUP].arg.str)))
      != OK) {
    return pgeret;
  }
  p = skipwhite(p);
  if (*p == NUL) {
    *pp = p;
    return OK;
  }

  Pattern *pat = pattern_alloc(kPatAuList);
  p--;
  int gcsp_ret = get_comma_separated_patterns(&p, error, &pat, false,
                                              position.col + (size_t) (p - s));
  if (gcsp_ret != OK) {
    return gcsp_ret;
  }
  node->args[ARG_AU_PATTERNS].arg.pat = pat;
  p = skipwhite(p);

  if (STRNCMP(p, "nested", sizeof("nested") - 1) == 0) {
    const char *const start = p;
    p = skipwhite(p + sizeof("nested") - 1 + 1);
    if (*p == NUL) {
      p = start;
    } else {
      node->args[ARG_AU_NESTED].arg.flags = 1;
    }
  }

  if (*p == NUL) {
    *pp = p;
    return OK;
  }

  CommandPosition do_position = position;
  position.col += (size_t) (p - s);
  int do_ret = parse_do(&p, node, error, o, do_position, fgetline, cookie);
  if (do_ret != OK) {
    return do_ret;
  }

  *pp = p;
  return OK;
}

static CMD_P_DEF(parse_doautocmd)
{
  const char *p = *pp;
  const char *const s = p;
  int pgeret;
  AuEvent *events = NULL;
  node->args[ARG_DOAU_NOMDLINE].arg.flags =
      (uint_least32_t) (!check_nomodeline(&p));
  p = skipwhite(p);
  if ((pgeret = parse_group_and_event(&p, error, &events,
                                      &(node->args[ARG_DOAU_GROUP].arg.str)))
      != OK) {
    return pgeret;
  }
  node->args[ARG_DOAU_EVENTS].arg.events = events;
  if (events != NULL && *events == ANY_EVENT) {
    error->message = N_("E217: Can't execute autocommands for ALL events");
    error->position = s;
    return NOTDONE;
  }
  p = skipwhite(p);
  if (*p != NUL) {
    size_t len = STRLEN(p);
    node->args[ARG_DOAU_FNAME].arg.str = xmemdupz(p, len);
    p += len;
  }
  *pp = p;
  return OK;
}

static CMD_P_DEF(parse_behave)
{
  if (STRCMP(*pp, "mswin") == 0 || STRCMP(*pp, "xterm") == 0) {
    node->args[ARG_NAME_NAME].arg.str = xstrdup(*pp);
    *pp += STRLEN(*pp);
    return OK;
  } else {
    error->message =
        N_("E475: :behave command currently only supports mswin and xterm");
    error->position = *pp;
    return NOTDONE;
  }
}

static CMD_P_DEF(parse_breakadd)
{
  BreakType type;
  const char *p = *pp;
  const char *const s = p;
  bool profiling = (node->type == kCmdProfile || node->type == kCmdProfdel);

  if (STRNCMP(p, "func", 4) == 0) {
    type = kBreakInFunction;
    p += 4;
  } else if (STRNCMP(p, "file", 4) == 0) {
    type = kBreakInFile;
    p += 4;
  } else if (!profiling && STRNCMP(p, "here", 4) == 0) {
    type = kBreakHere;
    p += 4;
  } else {
    if (profiling) {
      error->message =
          N_("E475: Profile commands only accept `func' and `file' "
             "as their first argument");
    } else {
      error->message =
          N_("E475: Debug commands only accept `func', `file' and `here'");
    }
    error->position = p;
    return NOTDONE;
  }

  p = skipwhite(p);

  if (type == kBreakHere) {
    // Do nothing
  } else {
    if (!profiling && VIM_ISDIGIT(*p)) {
      size_t lnr = 0;
      lnr = (size_t) getdigits(&p);
      p = skipwhite(p);
      node->range.address.type = kAddrFixed;
      node->range.address.data.lnr = (linenr_T) lnr;
    }
    if (!*p) {
      if (type == kBreakInFunction) {
        error->message = N_("E475: Expecting function name or pattern");
      } else {
        error->message = N_("E475: Expecting file name or pattern");
      }
      error->position = p;
      return NOTDONE;
    }
    p = skipwhite(p);
    Pattern *pat = NULL;
    int cret;
    if ((cret = get_pattern(&p, error, &pat, false, true,
                            (size_t) (p - s) + position.col))
        != OK) {
      free_pattern(pat);
      return cret;
    }
    node->args[ARG_BREAK_NAME].arg.pat = pat;
  }
  node->args[ARG_BREAK_TYPE].arg.flags = (uint_least32_t) type;
  *pp = p;
  return OK;
}

static CMD_P_DEF(parse_cbuffer)
{
  const char *p = *pp;
  node->args[ARG_NUMBER_NUMBER].arg.number = -1;
  if (*p != NUL) {
    int bufnr;
    if (VIM_ISDIGIT(*p)) {
      bufnr = (int) getdigits(&p);
    }
    if (*p != NUL) {
      error->message = N_("E474: Expected buffer number");
      error->position = p;
      return NOTDONE;
    }
    node->args[ARG_NUMBER_NUMBER].arg.number = bufnr;
  }
  *pp = p;
  return OK;
}

static CMD_P_DEF(parse_number)
{
  node->args[ARG_NUMBER_NUMBER].arg.number = atoi(*pp);
  *pp += STRLEN(*pp);
  return OK;
}

static CMD_P_DEF(parse_clist)
{
  int start = 1;
  int end = -1;
  const char *p = *pp;

  if (get_list_range(&p, &start, &end) != OK) {
    error->message = N_("E488: Expected valid integer range");
    error->position = p;
    return NOTDONE;
  }
  node->args[ARG_CLIST_FIRST].arg.number = start;
  node->args[ARG_CLIST_LAST].arg.number = end;
  *pp = p;
  return OK;
}

static CMD_P_DEF(parse_regex)
{
  const char *p = *pp;
  Regex *regex = NULL;
  if (*p == NUL) {
  } else if (*p == '/') {
    p++;
    int rret;
    if ((rret = get_regex(&p, error, &regex, '/', NULL)) != OK) {
      return rret;
    }
  } else {
    size_t numbslashes = 0;
    const char *e;
    for (e = p; *e; e++) {
      if (*e == '\\') {
        numbslashes++;
      }
    }
    char *new_regex = xmalloc(6 + numbslashes + ((size_t) (e - p) + 1));
    //                        ^ \V\< and \>
    char *np = new_regex;
    memcpy(np, "\\V\\<", 4);
    np += 4;
    for (; p < e; p++) {
      *np++ = *p;
      if (*p == '\\') {
        *np++ = '\\';
      }
    }
    memcpy(np, "\\>", 3);
    //                ^ Also copy trailing NUL
    np += 2;
    assert(*np == NUL);
    regex = regex_alloc(new_regex, (size_t) (np - new_regex));
  }
  *pp = p;
  node->args[ARG_REG_REG].arg.reg = regex;
  return OK;
}

static CMD_P_DEF(parse_catch)
{
  const char *p = *pp;
  Regex *regex = NULL;
  if (ENDS_EXCMD(*p)) {
  } else {
    p++;
    int rret;
    if ((rret = get_regex(&p, error, &regex, p[-1], NULL)) != OK) {
      return rret;
    }
  }
  node->args[ARG_REG_REG].arg.reg = regex;
  *pp = p;
  return OK;
}

static CMD_P_DEF(parse_address)
{
  const char *p = *pp;
  Range *range = NULL;
  Range **next = &range;
  while (*p) {
    *next = XCALLOC_NEW(Range, 1);
    if (get_address(&p, &((*next)->address), error) == FAIL) {
      free_range(range);
      return FAIL;
    }
    if ((*next)->address.type == kAddrMissing) {
      if (*next != range) {
        break;
      }
      error->message = N_("E14: Invalid address");
      error->position = p;
      return NOTDONE;
    }
    if (get_address_followups(&p, error, &((*next)->address.followups)) == FAIL) {
      free_range(range);
      return FAIL;
    }
    p = skipwhite(p);
    (*next)->setpos = (*p == ';');
    if (*p == ';' || *p == ',') {
      p++;
      next = &((*next)->next);
    } else {
      break;
    }
  }
  node->args[ARG_ADDR_ADDR].arg.range = range;
  *pp = p;
  return OK;
}

/// Parse -complete option
///
/// @param[in]   p       Pointer to the first character of the parsed value.
/// @param[in]   vallen  Length of the parsed value.
/// @param[out]  error   Address where error is saved.
/// @param[out]  comp    Address where parsing results are saved. Must be zeroed 
///                      before passing here.
///
/// @return OK in case of success, NOTDONE in case of failure if error was set, 
///         FAIL in case of non-recoverable error.
static int parse_completion_argument(const char *p, const size_t vallen,
                                     CommandParserError *error,
                                     CmdComplete *compl)
{
  const char *arg = NULL;
  size_t arglen = 0;
  size_t valend = vallen;

  // Look for any argument part - which is the part after any ','
  for (size_t i = 0; i < vallen; i++) {
    if (p[i] == ',') {
      arg = p + i + 1;
      arglen = vallen - i - 1;
      valend = i;
      break;
    }
  }
  for (size_t i = 0; command_complete[i].expand != 0; i++) {
    if (STRLEN(command_complete[i].name) == valend
        && STRNCMP(p, command_complete[i].name, valend) == 0) {
      compl->type = command_complete[i].expand;
      break;
    }
  }
  if (compl->type == EXPAND_NOTHING) {
    error->message = N_("E180: Invalid complete value");
    error->position = p;
    return NOTDONE;
  }
  if (arg == NULL) {
    if (compl->type == EXPAND_USER_DEFINED || compl->type == EXPAND_USER_LIST) {
      error->message =
          N_("E467: Custom completion requires a function argument");
      error->position = p + vallen;
      return NOTDONE;
    }
  } else {
    if (compl->type != EXPAND_USER_DEFINED && compl->type != EXPAND_USER_LIST) {
      error->message =
          N_("E468: Completion argument only allowed for custom completion");
      error->position = arg - 1;
      return NOTDONE;
    }
    compl->arg = xmemdupz(arg, arglen);
  }
  return OK;
}

static CMD_P_DEF(parse_command)
{
  const char *p = *pp;
  uint_least32_t flags = 0;
  CmdComplete *compl = NULL;
  while (*p == '-') {
    p++;
    const char *const end = skiptowhite(p);
    const size_t nlen = (size_t) (end - p);
    if (STRNICMP(p, "bang", nlen) == 0) {
      flags |= FLAG_CMD_BANG;
    } else if (STRNICMP(p, "buffer", nlen) == 0) {
      flags |= FLAG_CMD_BUFFER;
    } else if (STRNICMP(p, "bar", nlen) == 0) {
      flags |= FLAG_CMD_BAR;
    } else if (STRNICMP(p, "register", nlen) == 0) {
      flags |= FLAG_CMD_REGISTER;
    } else {
      const char *val = NULL;
      size_t vallen = 0;
      size_t attrlen = nlen;

      // Look for the attribute name - which is the part before any '='
      size_t i;
      for (i = 0; i < nlen; ++i) {
        if (p[i] == '=') {
          val = p + i + 1;
          vallen = nlen - i - 1;
          attrlen = i;
          break;
        }
      }

      if (STRNICMP(p, "nargs", attrlen) == 0) {
        // If vallen != 1 then argument is definitely invalid. NUL value skips 
        // to default: case.
        flags &= ~FLAG_CMD_NARGS_MASK;
        switch (vallen == 1 ? *val : NUL) {
          case '0': {
            flags |= VAL_CMD_NARGS_NO;
            break;
          }
          case '1': {
            flags |= VAL_CMD_NARGS_ONE;
            break;
          }
          case '*': {
            flags |= VAL_CMD_NARGS_ANY;
            break;
          }
          case '?': {
            flags |= VAL_CMD_NARGS_Q;
            break;
          }
          case '+': {
            flags |= VAL_CMD_NARGS_P;
            break;
          }
          default: {
            error->message = N_("E176: Invalid number of arguments");
            error->position = (val == NULL ? p + attrlen : val);
            goto parse_command_error_return;
          }
        }
      } else if (STRNICMP(p, "range", attrlen) == 0) {
        if (vallen == 1 && *val == '%') {
          flags &= ~FLAG_CMD_RANGE_MASK;
          flags |= VAL_CMD_RANGE_ALL;
        } else if (val == NULL) {
          flags &= ~FLAG_CMD_RANGE_MASK;
          flags |= VAL_CMD_RANGE_CUR;
        } else {
          p = val;
          if ((flags & FLAG_CMD_COUNT_MASK) == VAL_CMD_COUNT_COUNT
              || (flags & FLAG_CMD_RANGE_MASK) == VAL_CMD_RANGE_COUNT) {
            goto parse_command_double_count;
          }
          int count = (int) getdigits(&p);
          if (p != val + vallen || vallen == 0) {
            goto parse_command_invalid_count;
          }
          flags &= ~FLAG_CMD_RANGE_MASK;
          flags |= VAL_CMD_RANGE_COUNT;
          // Do not alter has_count so that printer does not dump count without 
          // special-casing it.
          node->count = count;
        }
      } else if (STRNICMP(p, "count", attrlen) == 0) {
        if (val == NULL) {
          flags &= ~FLAG_CMD_COUNT_MASK;
          flags |= VAL_CMD_COUNT_EMPTY;
        } else {
          p = val;
          if ((flags & FLAG_CMD_COUNT_MASK) == VAL_CMD_COUNT_COUNT
              || (flags & FLAG_CMD_RANGE_MASK) == VAL_CMD_RANGE_COUNT) {
            goto parse_command_double_count;
          }
          int count = (int) getdigits(&p);
          if (p != val + vallen) {
            goto parse_command_invalid_count;
          }
          flags &= ~FLAG_CMD_COUNT_MASK;
          flags |= VAL_CMD_COUNT_COUNT;
          // Do not alter has_count so that printer does not dump count without 
          // special-casing it.
          node->count = count;
        }
      } else if (STRNICMP(p, "complete", attrlen) == 0) {
        if (val == NULL) {
          error->message = N_("E179: Argument required for -complete");
          error->position = p + attrlen;
          goto parse_command_error_return;
        }
        compl = XCALLOC_NEW(CmdComplete, 1);
        int cret;
        if ((cret = parse_completion_argument(val, vallen, error, compl))
            != OK) {
          free_complete(compl);
          return cret;
        }
      } else {
        error->message = N_("E181: Invalid attribute");
        error->position = p;
        goto parse_command_error_return;
      }
    }
    p = skipwhite(end);
  }
  const char *const name_start = p;
  if (ASCII_ISALPHA(*p)) {
    while (ASCII_ISALNUM(*p)) {
      p++;
    }
  }
  size_t name_len = (size_t) (p - name_start);
  if (!ends_excmd(*p) && !vim_iswhite(*p)) {
    error->message = N_("E182: Invalid command name");
    error->position = p;
    goto parse_command_error_return;
  } else if (!ASCII_ISUPPER(*name_start)) {
    error->message =
        N_("E183: User defined commands must start with an uppercase letter");
    error->position = name_start;
    goto parse_command_error_return;
  } else if ((name_len == 1 && *name_start == 'X')
             || (name_len <= 4 && STRNCMP(name_start, "Next", name_len) == 0)) {
    error->message =
        N_("E841: Reserved name, cannot be used for user defined command");
    error->position = name_start;
    goto parse_command_error_return;
  }
  char *name = xmemdupz(name_start, name_len);
  p = skipwhite(p);
  if (*p != NUL) {
    node->args[ARG_CMD_COMMAND].arg.str = xstrdup(p);
    p += STRLEN(p);
  }
  node->args[ARG_CMD_FLAGS].arg.flags = flags;
  node->args[ARG_CMD_COMPLETE].arg.complete = compl;
  node->args[ARG_CMD_NAME].arg.str = name;
  *pp = p;
  return OK;
parse_command_double_count:
  error->message = N_("E177: Count cannot be specified twice");
  error->position = p;
  goto parse_command_error_return;
parse_command_invalid_count:
  error->message = N_("E178: Invalid default value for count");
  error->position = p;
  goto parse_command_error_return;
parse_command_error_return:
  free_complete(compl);
  return NOTDONE;
}

static CMD_P_DEF(parse_delmarks)
{
  const char *p = *pp;
  if (node->bang) {
    if (*p == NUL) {
      return OK;
    } else {
      error->message = N_("E474: :delmarks must be called either without bang "
                          "or without arguments");
      error->position = p;
      return NOTDONE;
    }
  } else if (*p == NUL) {
    error->message = N_("E471: You must specify register(s)");
    error->position = p;
    return NOTDONE;
  }
  // All marks are in range 0x20 - 0x7F (really narrower)
  uint8_t marks[MAX_NUM_MARKS];
  memset(&(marks[0]), 'N', MAX_NUM_MARKS);
  for (; *p != NUL; p++) {
    bool is_lower = ASCII_ISLOWER(*p);
    bool is_digit = VIM_ISDIGIT(*p);
    if (is_lower || is_digit || ASCII_ISUPPER(*p)) {
      if (p[1] == '-') {
        uint8_t from = (uint8_t) *p;
        uint8_t to = (uint8_t) p[2];
        if (!(is_lower
              ? ASCII_ISLOWER(to)
              : (is_digit
                 ? VIM_ISDIGIT(to)
                 : ASCII_ISUPPER(to)))) {
          error->message = N_("E475: Trying to construct range out of marks "
                              "from different sets");
          error->position = p + 2;
          return NOTDONE;
        } else if (to < from) {
          error->message =
              N_("E475: Upper range bound is less then lower range bound");
          error->position = p + 2;
          return NOTDONE;
        }
        memset(&(marks[from - FIRST_MARK_CODE]), 'Y', (to - from + 1));
        p += 2;
      } else {
        marks[*p - FIRST_MARK_CODE] = 'Y';
      }
    } else {
      switch (*p) {
        case '"':
        case '^':
        case '.':
        case '[':
        case ']':
        case '<':
        case '>': {
          marks[*p - FIRST_MARK_CODE] = 'Y';
          break;
        }
        case ' ': {
          break;
        }
        default: {
          error->message = N_("E475: Unknown mark");
          error->position = p;
          return NOTDONE;
        }
      }
    }
  }
  node->args[ARG_NAME_NAME].arg.str = xmemdupz(&(marks[0]), MAX_NUM_MARKS);
  *pp = p;
  return OK;
}

static CMD_P_DEF(parse_display)
{
  const char *p = *pp;
  if (*p == NUL) {
    return OK;
  }
#define REGSTART 0x20
#define REGNUM 0x5F - REGSTART + 1
  char regtab[REGNUM];
  size_t reglen = 0;
  memset(regtab, 0, REGNUM);
  for (; *p; p++) {
    if (!valid_yank_reg(*p, false)) {
      continue;
    }
    uint8_t reg = (uint8_t) TOUPPER_ASC(*p);
    assert(reg - REGSTART < REGNUM && reg > REGSTART);
    if (!regtab[reg - REGSTART]) {
      reglen++;
    }
    regtab[reg - REGSTART] = 1;
  }
  char *regnames = xmallocz(reglen);
  char *cur_regname = regnames;
  for (uint8_t i = 0; i < REGNUM; i++) {
    if (regtab[i]) {
      *cur_regname++ = TOLOWER_ASC((char) (i + REGSTART));
    }
  }
  node->args[ARG_NAME_NAME].arg.str = regnames;
  *pp = p;
#undef REGSTART
#undef REGNUM
  return OK;
}

static CMD_P_DEF(parse_digraphs)
{
  const char *p = *pp;
  const char *const s = p;
  if (*p == NUL) {
    return OK;
  }

  size_t dig_count = 0;

  while (*p != NUL) {
    p = skipwhite(p);
    if (*p == NUL) {
      return OK;
    } else if (*p == ESC) {
      goto parse_digraphs_esc_error;
    }
    mb_ptr_adv_(p);
    if (*p == NUL) {
      error->message =
          N_("E474: Expected second digraph character, but got nothing");
      error->position = p;
      return NOTDONE;
    } else if (*p == ESC) {
      goto parse_digraphs_esc_error;
    }
    mb_ptr_adv_(p);
    p = skipwhite(p);
    if (!VIM_ISDIGIT(*p)) {
      error->message = (const char *) e_number_exp;
      error->position = p;
      return NOTDONE;
    }
    p = skipdigits(p);
    dig_count++;
  }

  if (dig_count == 0) {
    return OK;
  }

  p = s;

  char **const digraphs = XMALLOC_NEW(char *, dig_count + 1);
  uint_least32_t *const codepoints = XMALLOC_NEW(uint_least32_t, dig_count);

  char **cur_dig = digraphs;
  uint_least32_t *cur_cp = codepoints;

  while (dig_count) {
    p = skipwhite(p);
    const char *const dig_start = p;
    mb_ptr_adv_(p);
    mb_ptr_adv_(p);
    *cur_dig++ = xmemdupz(dig_start, (size_t) (p - dig_start));
    p = skipwhite(p);
    *cur_cp++ = (uint_least32_t) getdigits(&p);
    dig_count--;
  }
  *cur_dig = NULL;
  node->args[ARG_DIG_DIGRAPHS].arg.strs = digraphs;
  node->args[ARG_DIG_CHARS].arg.unumbers = codepoints;
  *pp = skipwhite(p);
  return OK;
parse_digraphs_esc_error:
  error->message = N_("E104: Escape not allowed in digraph");
  error->position = p;
  return NOTDONE;
}

static CMD_P_DEF(parse_later)
{
  const char *p = *pp;
  uint_least32_t later_type = VAL_LATER_COUNT;
  unsigned count = 1;
  if (VIM_ISDIGIT(*p)) {
    count = (unsigned) getdigits(&p);
    switch (*p) {
#define LATER_TYPE(ch, type) \
      case ch: { \
        p++; \
        later_type = VAL_LATER_##type; \
        break; \
      }
      LATER_TYPE('s', SECONDS)
      LATER_TYPE('m', MINUTES)
      LATER_TYPE('h', HOURS)
      LATER_TYPE('d', DAYS)
      LATER_TYPE('f', FILE)
#undef LATER_TYPE
      case NUL: {
        break;
      }
      default: {
        error->message =
            N_("E475: Expected 's', 'm', 'h', 'd', 'f' or nothing "
               "after number");
        error->position = p;
        return NOTDONE;
      }
    }
    if (*p != NUL) {
      error->message = N_("E475: Trailing characters");
      error->position = p;
      return NOTDONE;
    }
  } else if (*p != NUL) {
    error->message = N_("E475: Expected numeric argument");
    error->position = p;
    return NOTDONE;
  }
  *pp = p;
  node->args[ARG_LATER_FLAGS].arg.flags = later_type;
  node->args[ARG_LATER_COUNT].arg.unumber = count;
  return OK;
}

static CMD_P_DEF(parse_filetype)
{
  const char *p = *pp;
  if (*p == NUL) {
    return OK;
  }
  uint_least32_t flags = 0;
  for (;;) {
    if (STRNCMP(p, "plugin", 6) == 0) {
      flags |= FLAG_FT_PLUGIN;
      p = skipwhite(p + 6);
      continue;
    } else if (STRNCMP(p, "indent", 6) == 0) {
      flags |= FLAG_FT_INDENT;
      p = skipwhite(p + 6);
      continue;
    }
    break;
  }
  if (STRCMP(p, "on") == 0) {
    flags |= FLAG_FT_ON;
    p += 2;
  } else if (STRCMP(p, "detect") == 0) {
    flags |= FLAG_FT_DETECT;
    p += 6;
  } else if (STRCMP(p, "off") == 0) {
    flags |= FLAG_FT_OFF;
    p += 3;
  } else {
    error->message = N_("E475: Invalid syntax: expected "
                        "`filetype[ [plugin|indent]... {on|off|detect}]'");
    error->position = p;
    return NOTDONE;
  }
  *pp = p;
  node->args[ARG_FT_FLAGS].arg.flags = flags;
  return OK;
}

static CMD_P_DEF(parse_history)
{
  const char *p = *pp;
  uint_least32_t flags = 0;
  if (!(VIM_ISDIGIT(*p) || *p == '-' || *p == ',')) {
    const char *end = p;
    while (*end && (ASCII_ISALPHA(*end) || strchr(":=@>/?", *end) != NULL)) {
      end++;
    }
    HistoryType histtype = get_histtype(p, (size_t) (end - p), true);
    switch (histtype) {
      case HIST_INVALID: {
        if (STRNICMP(p, "all", end - p) == 0) {
          flags |= FLAG_HIST_ALL;
          break;
        } else {
          error->message = N_("E488: Expected history name or nothing");
          error->position = p;
          return NOTDONE;
        }
      }
#define HIST_TO_FLAG(h) \
      case HIST_##h: { \
        flags |= FLAG_HIST_##h; \
        break; \
      }
      HIST_TO_FLAG(DEFAULT)
      HIST_TO_FLAG(CMD)
      HIST_TO_FLAG(SEARCH)
      HIST_TO_FLAG(EXPR)
      HIST_TO_FLAG(INPUT)
      HIST_TO_FLAG(DEBUG)
#undef HIST_TO_FLAG
    }
    p = end;
  } else {
    flags |= FLAG_HIST_DEFAULT;
  }
  node->args[ARG_HIST_FIRST].arg.number = 1;
  node->args[ARG_HIST_LAST].arg.number = -1;
  if (get_list_range(&p, &(node->args[ARG_HIST_FIRST].arg.number),
                     &(node->args[ARG_HIST_LAST].arg.number)) != OK) {
    error->message = N_("E488: Expected valid history lines range");
    error->position = p;
    return NOTDONE;
  }
  node->args[ARG_HIST_FLAGS].arg.flags = flags;
  *pp = p;
  return OK;
}

static CMD_P_DEF(parse_mark)
{
  const char *p = *pp;
  if (*p == NUL) {
    error->message = N_("E471: Expected mark name");
    error->position = p;
    return NOTDONE;
  }
  if (   ASCII_ISLOWER(*p)
      || ASCII_ISUPPER(*p)
      || *p == '\''
      || *p == '`') {
    node->args[ARG_MARK_CHAR].arg.ch = *p;
    p++;
  } else {
    error->message =
        N_("E191: Argument must be a letter or forward/backward quote");
    error->position = p;
    return NOTDONE;
  }
  *pp = p;
  return OK;
}

static CMD_P_DEF(parse_popup)
{
  return parse_menu_name(pp, error, kMenuWholeCmd,
                         &(node->args[ARG_POPUP_NAME].arg.menu_item), NULL);
}

static CMD_P_DEF(parse_make)
{
  // Warning: Vim redirects parsing to :vimgrep if &grepprg is "internal". 
  // Parser cannot do this.
  return parse_rest_allow_empty(pp, node, error, o, position, fgetline, cookie);
}

static CMD_P_DEF(parse_retab)
{
  const char *p = *pp;
  int new_ts = (int) getdigits(&p);
  if (new_ts < 0) {
    error->message = (const char *) e_positive;
    error->position = *pp;
    return NOTDONE;
  }
  node->count = new_ts;
  *pp = p;
  return OK;
}

static CMD_P_DEF(parse_resize)
{
  const char *p = *pp;
  int n = atoi(p);
  bool relative = (*p == '-' || *p == '+');
  node->args[ARG_RESIZE_FLAGS].arg.flags = (uint_least32_t) relative;
  node->args[ARG_RESIZE_NUMBER].arg.number = n;
  *pp += STRLEN(p);
  return OK;
}

static CMD_P_DEF(parse_redir)
{
  const char *p = *pp;
  uint_least32_t flags = 0;
  switch (*p) {
    case '>': {
      p++;
      if (*p == '>') {
        p++;
        flags |= FLAG_REDIR_APPEND;
      }
      p = skipwhite(p);
      size_t len = STRLEN(p);
      node->args[ARG_REDIR_FILE].arg.str = xmemdupz(p, len);
      p += len;
      break;
    }
    case '@': {
      p++;
      if (*p == NUL) {
        // :redir @ seems to behave the same way as :redir END.
      } else if (ASCII_ISALPHA(*p) || strchr("\"*+", *p) != NULL) {
        flags |= (((uint_least32_t) (*p)) & FLAG_REDIR_REG_MASK);
        p++;
        if (*p == '>') {
          p++;
          if (*p == '>') {
            p++;
            flags |= FLAG_REDIR_APPEND;
          }
        }
      } else {
        error->message =
            N_("E475: Expected register name; one of A-Z, a-z, \", * and +");
        error->position = p;
        return NOTDONE;
      }
      break;
    }
    case '=': {
      p++;
      if (*p == '>') {
        p++;
        Expression *expr;
        int ret;

        if ((ret = parse_lval(&p, error, o, position.col, &expr, 0)) == FAIL) {
          return FAIL;
        }
        node->args[ARG_REDIR_VAR].arg.expr = expr;
        if (ret == NOTDONE) {
          return NOTDONE;
        }
      } else {
        error->message = N_("E475: Expected `>' and variable name");
        error->position = p;
        return NOTDONE;
      }
      break;
    }
    case 'E':
    case 'e': {
      if (STRICMP(p, "END") == 0) {
        p += 3;
      } else {
        error->message = N_("E475: Expected `END'");
        error->position = p + 1;
        return NOTDONE;
      }
      break;
    }
    default: {
      error->message =
          N_("E475: Expected `END', `>[>] {file}', "
             "`@{register}[>[>]]' or `=> {variable}'");
      error->position = p;
      return NOTDONE;
    }
  }
  node->args[ARG_REDIR_FLAGS].arg.flags = flags;
  *pp = p;
  return OK;
}

static CMD_P_DEF(parse_script)
{
  const char *p = *pp;
  garray_T *ga_strs = &(node->args[ARG_APPEND_LINES].arg.ga_strs);

  ga_init(ga_strs, sizeof(char *), 16);

  if (p[0] != '<' || p[1] != '<') {
    size_t len = STRLEN(p);
    GA_APPEND(char *, ga_strs, xmemdupz(p, len));
    p += len;
  } else {
    p = skipwhite(p + 2);
    const char *const end_pattern = (*p ? p : ".");
    char *next_line;
    while ((next_line = fgetline(':', cookie, 0)) != NULL) {
      if (STRCMP(end_pattern, next_line) == 0) {
        free(next_line);
        break;
      }
      GA_APPEND(char *, ga_strs, next_line);
    }
    p += STRLEN(p);
  }

  *pp = p;
  return OK;
}

static CMD_P_DEF(parse_open)
{
  const char *p = *pp;
  if (*p == '/') {
    p++;
    int rret;
    if ((rret = get_regex(&p, error, &(node->args[ARG_OPEN_REGEX].arg.reg), '/',
                          NULL)) != OK) {
      return rret;
    }
    // Ignore any other arguments
    p += STRLEN(p);
  } else if (*p != NUL) {
    size_t len = STRLEN(p);
    node->args[ARG_OPEN_FILE].arg.str = xmemdupz(p, len);
    p += len;
  }
  *pp = p;
  return OK;
}

static CMD_P_DEF(parse_global)
{
  const char *p = *pp;
  const char *const s = p;
  uint_least32_t flags = 0;
  // Udocumented Vi feature:
  //  :g\/ and :g\?: use previous search pattern
  //  :g\&         : use previous substitute pattern
  if (*p == '\\') {
    p++;
    if (*p == '&') {
      flags |= FLAG_G_RE_SUBST;
    } else if (*p == '/' || *p == '?') {
      flags |= FLAG_G_RE_SEARCH;
    } else {
      error->message = (const char *) e_backslash;
      error->position = p;
      return NOTDONE;
    }
    p++;
  } else if (*p == NUL) {
    error->message = N_("E148: Regular expression missing from global");
    error->position = p;
    return NOTDONE;
  } else {
    p++;
    int rret;
    if ((rret = get_regex(&p, error, &(node->args[ARG_G_REG].arg.reg), p[-1],
                          NULL)) != OK) {
      return rret;
    }
  }
  node->args[ARG_G_FLAGS].arg.flags = flags;
  if (*p != NUL) {
    CommandPosition do_position = position;
    position.col += (size_t) (p - s);
    int do_ret = parse_do(&p, node, error, o, do_position, fgetline, cookie);
    if (do_ret != OK) {
      return do_ret;
    }
  }
  *pp = p;
  return OK;
}

static CMD_P_DEF(parse_vimgrep)
{
  const char *p = *pp;
  const char *const s = p;
  Regex *regex = NULL;
  uint_least32_t flags = 0;
  // ":vimgrep pattern fname"
  if (vim_isIDc(*p)) {
    const char *const s = p;
    p = skiptowhite(p);
    regex = regex_alloc(s, (size_t) (p - s));
  } else {
    p++;
    int rret;
    if ((rret = get_regex(&p, error, &regex, p[-1], NULL)) != OK) {
      return rret;
    }
    for (;;) {
      switch (*p) {
        case 'g': {
          flags |= FLAG_VIMG_EVERY;
          p++;
          continue;
        }
        case 'j': {
          flags |= FLAG_VIMG_NOJUMP;
          p++;
          continue;
        }
        default: {
          break;
        }
      }
      break;
    }
  }
  node->args[ARG_VIMG_FLAGS].arg.flags = flags;
  node->args[ARG_VIMG_REG].arg.reg = regex;
  p = skipwhite(p);
  if (*p == NUL) {
    error->message = N_("E683: File name missing or invalid pattern");
    error->position = p;
    return NOTDONE;
  }
  int pfret = parse_files(&p, error, position.col + (size_t) (p - s),
                          &(node->glob));
  if (pfret != OK) {
    return pfret;
  }
  *pp = p;
  return OK;
}

static CMD_P_DEF(parse_gui)
{
  const char *p = *pp;
  const char *const s = p;
  if (*p == '-' && (p[1] == 'f' || p[1] == 'b') && (!p[2] || vim_iswhite(p[2])))
  {
    node->args[ARG_GUI_FG].arg.flags = (uint_least32_t) (p[1] == 'f');
    p = skipwhite(p + 2);
  }
  if (*p == NUL) {
    *pp = p;
    return OK;
  }
  int pfret = parse_files(&p, error, position.col + (size_t) (p - s),
                          &(node->glob));
  if (pfret != OK) {
    return pfret;
  }
  *pp = p;
  return OK;
}

static CMD_P_DEF(parse_marks)
{
  const char *p = *pp;
  const char *const s = p;
  size_t marksnum = 0;
  for (; *p; p++) {
    if (ASCII_ISALPHA(*p) || strchr("'\"[]^.<>", *p) != NULL) {
      marksnum++;
    }
  }
  *pp = p;
  char *const marks = xmallocz(marksnum);
  char *cur_mark = marks;
  for (p = s; *p; p++) {
    if (ASCII_ISALPHA(*p) || strchr("'\"[]^.<>", *p) != NULL) {
      *cur_mark++ = *p;
    }
  }
  node->args[ARG_NAME_NAME].arg.str = marks;
  return OK;
}

static CMD_P_DEF(parse_match)
{
  const char *p = *pp;
  if (*p == NUL) {
    return OK;
  } else if (STRNICMP(p, "none", 4) == 0
             && (vim_iswhite(p[4]) || ENDS_EXCMD(p[4]))) {
    p += 4;
    return OK;
  }
  const char *const s = p;
  p = skiptowhite(p);
  node->args[ARG_MATCH_GROUP].arg.str = xmemdupz(s, (size_t) (p - s));
  p = skipwhite(p);
  if (*p == NUL) {
    error->message = N_("E475: Expected regular expression");
    error->position = p;
    return NOTDONE;
  }
  p++;
  int rret;
  if ((rret = get_regex(&p, error, &(node->args[ARG_MATCH_REG].arg.reg), p[-1],
                        NULL)) != OK) {
    return rret;
  }
  *pp = skipwhite(p);
  return OK;
}

///< Structure that holds :set arguments
typedef struct set_arg {
  const char *name;      ///< Address of the first character of the option name.
  size_t name_len;       ///< Name length.
  int opt_idx;           ///< Option index or -1.
  int key;               ///< Key index or 0.
  uint_least32_t flags;  ///< Flags.
  const char *value;     ///< String value.
  size_t value_len;      ///< Value length.
  long ivalue;           ///< Integer value.
  struct set_arg *next;  ///< Next processed option.
} SetArg;

static int wildcharm_idx = -2;
static int wildchar_idx = -2;

static CMD_P_DEF(parse_set)
{
  const char *p = *pp;
  if (*p == NUL) {
    return OK;
  }
  if (wildcharm_idx == -2) {
#define FINDOPTION(s) findoption_len(s, sizeof(s) - 1)
    wildcharm_idx = FINDOPTION("wildcharm");
    wildchar_idx = FINDOPTION("wildchar");
#undef FINDOPTION
  }
  size_t num_options = 0;
  SetArg *args;
  SetArg **next = &args;
  while (ASCII_ISLOWER(*p) || *p == '<') {
    const char *const start_arg = p;
    *next = XCALLOC_NEW(SetArg, 1);
    num_options++;
    if (STRNCMP(p, "all", 3) == 0 && !ASCII_ISALPHA(p[3])) {
      (*next)->name = p;
      (*next)->name_len = 3;
      (*next)->opt_idx = -1;
      p += 3;
      if (*p == '&') {
        p++;
        (*next)->flags |= FLAG_SET_DEFAULT;
      } else {
        (*next)->flags |= FLAG_SET_SHOW;
      }
    } else if (STRNCMP(p, "termcap", 7) == 0) {
      (*next)->name = p;
      (*next)->name_len = 7;
      (*next)->opt_idx = -1;
      (*next)->flags |= FLAG_SET_SHOW;
      p += 7;
    } else {
      if (STRNCMP(p, "no", 2) == 0 && STRNCMP(p, "novice", 6) != 0) {
        (*next)->flags |= FLAG_SET_UNSET;
        p += 2;
      } else if (STRNCMP(p, "inv", 3) == 0) {
        (*next)->flags |= FLAG_SET_INVERT;
        p += 3;
      }
      int key = 0;
      size_t len = 0;
      int nextchar = 0;
      int opt_idx = -1;
      if (*p == '<') {
        if (p[1] == 't' && p[2] == '_' && p[3] && p[4]) {
          len = 5;
        } else {
          len = 1;
          while (p[len] && p[len] != '>') {
            len++;
          }
        }
        if (p[len] != '>') {
          error->message = N_("E474: Expected `<'");
          error->position = p + len;
          goto parse_set_error;
        }
        opt_idx = findoption_len(p + 1, len - 1);
        len++;
        if (opt_idx == -1) {
          key = find_key_option_len(p + 1, len);
          (*next)->name = p;
          (*next)->name_len = len;
        } else {
          (*next)->name = p + 1;
          (*next)->name_len = len - 2;
        }
      } else {
        if (p[0] == 't' && p[1] == '_' && p[2] && p[3]) {
          len = 4;
        } else {
          while (ASCII_ISALNUM(p[len]) || p[len] == '_') {
            len++;
          }
        }
        opt_idx = findoption_len(p, len);
        if (opt_idx == -1) {
          key = find_key_option_len(p, len);
        }
        (*next)->name = p;
        (*next)->name_len = len;
      }

      if (opt_idx == -1 && key == 0) {
        error->message = N_("E518: Unknown option");
        error->position = p;
        goto parse_set_error;
      }

      (*next)->opt_idx = opt_idx;
      (*next)->key = key;

      int afterchar = p[len];
      while (vim_iswhite(p[len])) {
        len++;
      }

      if (p[len] && p[len + 1] == '=') {
        switch (p[len]) {
          case '+': {
            (*next)->flags |= FLAG_SET_APPEND;
            len++;
            break;
          }
          case '-': {
            (*next)->flags |= FLAG_SET_REMOVE;
            len++;
            break;
          }
          case '^': {
            (*next)->flags |= FLAG_SET_PREPEND;
            len++;
            break;
          }
          default: {
            break;
          }
        }
      }
      nextchar = p[len];

      uint_least8_t properties;

      if (opt_idx >= 0) {
        properties = get_option_properties_idx(opt_idx);
      } else {
        // Key properties
        properties = GOP_STRING|GOP_GLOBAL;
      }

      p += len;
      if (nextchar == '&' && p[1] == 'v' && p[2] == 'i') {
        (*next)->flags |= FLAG_SET_DEFAULT;
        if (p[3] == 'm') {
          (*next)->flags |= FLAG_SET_VIM;
          p += 3;
        } else {
          (*next)->flags |= FLAG_SET_VI;
          p += 2;
        }
      }
      if (nextchar && strchr("?!&<", nextchar) != NULL
          && p[1] != NUL && !vim_iswhite(p[1])) {
        error->message = (const char *) e_trailing;
        error->position = p + 1;
        goto parse_set_error;
      }

      if (nextchar == '?' || (
              ((*next)->flags & (FLAG_SET_UNSET|FLAG_SET_INVERT)) == 0
              && (!nextchar || strchr("=:&<", nextchar) == NULL)
              && !(properties & GOP_BOOLEAN))) {
        (*next)->flags |= FLAG_SET_SHOW;
        if (nextchar == '?') {
          p++;
        }
      } else {
        if (properties & GOP_BOOLEAN) {
          if (nextchar == '=' || nextchar == ':') {
            error->message =
                N_("E474: Cannot set boolean options with `=' or `:'");
            error->position = p;
            goto parse_set_error;
          }
          switch (nextchar) {
            case '!': {
              (*next)->flags |= FLAG_SET_INVERT;
              p++;
              break;
            }
            case '&': {
              (*next)->flags |= FLAG_SET_DEFAULT;
              p++;
              break;
            }
            case '<': {
              (*next)->flags |= FLAG_SET_GLOBAL;
              p++;
              break;
            }
            case NUL: {
              break;
            }
            default: {
              if (!vim_iswhite(afterchar)) {
                error->message = (const char *) e_trailing;
                error->position = p;
                goto parse_set_error;
              }
              break;
            }
          }
        } else {
          if ((nextchar && strchr("=:&<", nextchar) == NULL)) {
            error->message = N_("E474: Expected `=', `:', `&' or `<'");
            error->position = p;
            goto parse_set_error;
          }
          if ((*next)->flags & (FLAG_SET_UNSET|FLAG_SET_INVERT)) {
            error->message =
                N_("E474: Cannot invert or unset non-boolean option");
            error->position = start_arg;
            goto parse_set_error;
          }
          if (properties & GOP_NUMERIC) {
            p++;
            if (nextchar == '&') {
              (*next)->flags |= FLAG_SET_DEFAULT;
            } else if (nextchar == '<') {
              (*next)->flags |= FLAG_SET_GLOBAL;
            } else if ((opt_idx == wildcharm_idx
                        || opt_idx == wildchar_idx)
                       && (*p == '<' || *p == '^'
                           || ((!p[1] || vim_iswhite(p[1]))
                               && !VIM_ISDIGIT(*p)))) {
              (*next)->ivalue = string_to_key(p);
              (*next)->flags |= FLAG_SET_IVALUE|FLAG_SET_ASSIGN;
              if ((*next)->ivalue == 0 && opt_idx == wildcharm_idx) {
                error->message = N_("E474: Expected key definition");
                error->position = p;
                goto parse_set_error;
              }
              while (*p != NUL && !vim_iswhite(*p)) {
                if (*p++ == '\\' && *p != NUL) {
                  p++;
                }
              }
            } else if (*p == '-' || VIM_ISDIGIT(*p)) {
              (*next)->flags |= FLAG_SET_IVALUE|FLAG_SET_ASSIGN;
              int ilen = 0;
              vim_str2nr(p, NULL, &ilen, TRUE, TRUE, &((*next)->ivalue), NULL);
              if (p[ilen] != NUL && !vim_iswhite(p[ilen])) {
                error->message = N_("E474: Only numbers are allowed");
                error->position = p;
                goto parse_set_error;
              }
              p += ilen;
            } else {
              error->message = N_("E521: Number required after =");
              error->position = p;
              goto parse_set_error;
            }
          } else if (opt_idx >= 0) {
            p++;
            if (nextchar == '&') {
              (*next)->flags |= FLAG_SET_DEFAULT;
            } else if (nextchar == '<') {
              (*next)->flags |= FLAG_SET_GLOBAL;
            } else {
              (*next)->flags |= FLAG_SET_ASSIGN;
              size_t arglen = 0;
              for (; p[arglen] && !vim_iswhite(p[arglen]); arglen++) {
                if (*p == '\\') {
                  p++;
                }
              }
              (*next)->value = p;
              (*next)->value_len = arglen;
              p += arglen;
            }
          } else {
            assert(nextchar == '=');
            (*next)->flags |= FLAG_SET_ASSIGN;
            p++;
            size_t arglen = 0;
            for (; p[arglen] && !vim_iswhite(p[arglen]); p++) {
              if (*p == '\\') {
                p++;
              }
            }
            (*next)->value = p;
            (*next)->value_len = arglen;
            p += arglen;
          }
        }
      }
    }
    p = skipwhite(p);
    next = &((*next)->next);
  }
  // Note: using num_options + 1 here to have place for NULL which indicates 
  //       number of options set. It is not indicated anywhere else.
  char **const names = node->args[ARG_SET_OPTIONS].arg.strs =
      XMALLOC_NEW(char *, num_options + 1);
  // Note: using xcalloc so that NULLs will appear where appropriate.
  // Note: using num_options + 1 here because trailing NULL is needed for 
  //       free_cmd_arg(). One should not use it to determine whether option 
  //       list ended.
  char **const values = node->args[ARG_SET_VALUES].arg.strs =
      XCALLOC_NEW(char *, num_options + 1);
  uint_least32_t *const flagss = node->args[ARG_SET_FLAGSS].arg.unumbers =
      XMALLOC_NEW(uint_least32_t, num_options);
  uint_least32_t *const keys = node->args[ARG_SET_KEYS].arg.unumbers =
      XMALLOC_NEW(uint_least32_t, num_options);
  int *ivalues = node->args[ARG_SET_IVALUES].arg.numbers =
      XMALLOC_NEW(int, num_options);
  int *opt_idxs = node->args[ARG_SET_INDEXES].arg.numbers =
      XMALLOC_NEW(int, num_options);
  SetArg *cur = args;
  for (size_t i = 0; cur != NULL; i++) {
    names[i] = xmemdupz(cur->name, cur->name_len);
    if (cur->value != NULL) {
      // FIXME Assigned values are escaped.
      values[i] = xmemdupz(cur->value, cur->value_len);
    } else {
      values[i] = (char *) empty_string;
    }
    flagss[i] = cur->flags;
    keys[i] = (uint_least32_t) cur->key;
    // TODO Hold long?
    ivalues[i] = (int) cur->ivalue;
    opt_idxs[i] = cur->opt_idx;
    SetArg *prev = cur;
    cur = cur->next;
    free(prev);
  }
  names[num_options] = NULL;
  *pp = p;
  return OK;
parse_set_error:
  for (SetArg *cur = args; cur != NULL;) {
    SetArg *prev = cur;
    cur = prev->next;
    free(prev);
  }
  return NOTDONE;
}

static CMD_P_DEF(parse_sleep)
{
  switch (**pp) {
    case 'm': {
      // :sleep in Vim ignores characters after 'm'
      *pp += STRLEN(*pp);
      break;
    }
    case NUL: {
      node->args[ARG_SLEEP_MULT].arg.unumber = 1000;
      break;
    }
    default: {
      error->message = N_("E475: Expected `m' or nothing");
      error->position = *pp;
      return NOTDONE;
    }
  }
  return OK;
}

static CMD_P_DEF(parse_sub)
{
#define P_COL(p) ((size_t) ((p) - s) + position.col)
  const char *p = *pp;
  const char *const s = p;
  uint_least32_t flags = 0;
  char delim = 0;
  if ((node->type == kCmdSubstitute
       || node->type == kCmdSmagic
       || node->type == kCmdSnomagic)
      && *p && !vim_iswhite(*p) && strchr("0123456789cegriIp|\"", *p) == NULL) {
    if (isalpha(*p)) {
      error->message =
          N_("E146: Regular expressions can't be delimited by letters");
      error->position = p;
      return NOTDONE;
    }
    if (*p == '\\') {
      p++;
      switch (*p) {
        case '/':
        case '?': {
          flags |= FLAG_S_RE_SEARCH;
          break;
        }
        case '&': {
          flags |= FLAG_S_RE_SUBST;
          break;
        }
        default: {
          error->message = (char *) e_backslash;
          error->position = p;
          return NOTDONE;
        }
      }
      delim = *p;
      p++;
    } else {
      delim = *p;
      p++;
      int rret;
      if ((rret = get_regex(&p, error, &(node->args[ARG_S_REG].arg.reg), delim,
                            NULL)) != OK) {
        return rret;
      }
    }
    const char *const sub_start = p;
    while (*p) {
      if (*p == delim) {
        break;
      }
      if (*p == '\\' && p[1]) {
        p++;
      }
      mb_ptr_adv_(p);
    }
    if (p - sub_start == 1 && *sub_start == '%'
        && (o.flags & FLAG_POC_CPO_SUBPC)) {
      flags |= FLAG_S_SUB_PREV;
    } else {
      if (*sub_start == '\\' && sub_start[1] == '=') {
        char *const expr_str_start = xmemdupz(sub_start + 2,
                                              (size_t) (p - sub_start) - 2);
        const char *expr_str = expr_str_start;
        Expression *expr;
        ExpressionParserError expr_error;

        expr = parse_one_expression(&expr_str, &expr_error, &parse0_err,
                                    P_COL(sub_start + 2));
        if (expr == NULL) {
          if (expr_error.message == NULL) {
            return FAIL;
          }
          error->message = expr_error.message;
          error->position = sub_start + 2
              + (expr_error.position - expr_str_start);
          return NOTDONE;
        }
        if (*expr_str) {
          // Expected expression to end here. Early return will result in 
          // e_trailing error message.
          *pp = sub_start + (expr_str - expr_str_start);
          free(expr_str_start);
          return OK;
        }
        free(expr_str_start);
        Replacement *rep = node->args[ARG_S_REP].arg.rep
            = replacement_alloc(kRepExpr, P_COL(sub_start), P_COL(p - 1));
        rep->data.expr = expr;
      } else {
        const char *p2 = sub_start;
        Replacement **next = &(node->args[ARG_S_REP].arg.rep);
        while (p2 < p) {
          switch (*p2) {
            case '\\': {
              p2++;
              switch (*p2) {
#define REP_ATOM(ch, rep_type) \
                case ch: { \
                  (*next) = replacement_alloc(rep_type, P_COL(p2 - 1), \
                                              P_COL(p2)); \
                  p2++; \
                  break; \
                }
                REP_ATOM('u', kRepCharUpCase)
                REP_ATOM('l', kRepCharDownCase)
                REP_ATOM('U', kRepUpCase)
                REP_ATOM('L', kRepDownCase)
                case 'e':
                REP_ATOM('E', kRepCaseEnd)
                REP_ATOM('0', kRepMatched)
                REP_ATOM('r', kRepNewLine)
#undef REP_ATOM
#define REP_ESC_ATOM(c, res) \
                case c: { \
                  (*next) = replacement_alloc(kRepEscaped, P_COL(p2 - 1), \
                                              P_COL(p2)); \
                  (*next)->data.ch = (uint32_t) (res); \
                  p2++; \
                  break; \
                }
                REP_ESC_ATOM('n', NUL)
                REP_ESC_ATOM('b', BS)
                REP_ESC_ATOM('t', TAB)
                // May as well use literal escapes for the characters below, but 
                // these ones are explicitly mentioned in help.
                REP_ESC_ATOM('\\', '\\')
                REP_ESC_ATOM(CAR, CAR)
#undef REP_ESC_ATOM
                case '1':
                case '2':
                case '3':
                case '4':
                case '5':
                case '6':
                case '7':
                case '8':
                case '9': {
                  (*next) = replacement_alloc(kRepGroup, P_COL(p2 - 1),
                                              P_COL(p2));
                  (*next)->data.group = (uint8_t) *p2 - '0';
                  p2++;
                  break;
                }
                case '&': {
                  if (!(o.flags & FLAG_POC_MAGIC)) {
                    (*next) = replacement_alloc(kRepMatched,
                                                P_COL(p2 - 1), P_COL(p2));
                    p2++;
                    break;
                  }
                  // fallthrough
                }
                case '~': {
                  if (!(o.flags & FLAG_POC_MAGIC)) {
                    (*next) = replacement_alloc(kRepPrevSub,
                                                P_COL(p2 - 1), P_COL(p2));
                    p2++;
                    break;
                  }
                  // fallthrough
                }
                default: {
                  const char *const p2_s = p2;
                  uint32_t ch = (uint32_t) mb_cptr2char_adv(&p2);
                  (*next) = replacement_alloc(kRepEscLiteral,
                                              P_COL(p2_s - 1),
                                              P_COL(p2 - 1));
                  (*next)->data.ch = ch;
                  // TODO: Give a warning in most cases because \x is reserved.
                  break;
                }
              }
              break;
            }
            case '&': {
              if (o.flags & FLAG_POC_MAGIC) {
                (*next) = replacement_alloc(kRepMatched,
                                            P_COL(p2 - 1), P_COL(p2));
                p2++;
                break;
              }
              // fallthrough
            }
            case '~': {
              if (o.flags & FLAG_POC_MAGIC) {
                (*next) = replacement_alloc(kRepPrevSub,
                                            P_COL(p2 - 1), P_COL(p2));
                p2++;
                break;
              }
              // fallthrough
            }
            default: {
              const char *const p2_s = p2;
              while (p2 < p && *p2 != '\\'
                     && ((*p2 != '~' && *p2 != '&')
                         || !(o.flags & FLAG_POC_MAGIC))) {
                assert(*p2 != NUL);
                p2++;
              }
              (*next) = replacement_alloc(kRepLiteral, P_COL(p2_s), P_COL(p2));
              (*next)->data.str = xmemdupz(p2_s, (size_t) (p2 - p2_s));
              break;
            }
          }
          assert(*next != NULL);
          next = &((*next)->next);
        }
      }
    }
  } else {
    delim = '&';
  }
  if (*p == delim) {
    p++;
  }
  while (*p) {
    switch (*p) {
#define S_FLAG(ch, flag) \
      case ch: { \
        flags |= flag; \
        p++; \
        continue; \
      }
      S_FLAG('c', FLAG_S_CONFIRM)
      S_FLAG('n', FLAG_S_COUNT)
      S_FLAG('e', FLAG_S_NOERR)
      S_FLAG('r', FLAG_S_R)
      S_FLAG('p', FLAG_S_PRINT)
      S_FLAG('#', FLAG_S_PRINT_LNR)
      S_FLAG('l', FLAG_S_PRINT_LIST)
      S_FLAG('i', FLAG_S_IC)
      S_FLAG('I', FLAG_S_NOIC)
#undef S_FLAG
      case 'g': {
        if (o.flags & FLAG_POC_ED) {
          if (flags & FLAG_S_G) {
            flags &= ~FLAG_S_G;
            flags |= FLAG_S_G_REVERSE;
          } else {
            flags &= ~FLAG_S_G_REVERSE;
            flags |= FLAG_S_G;
          }
        } else {
          flags |= FLAG_S_G;
        }
        p++;
        continue;
      }
      default: {
        break;
      }
    }
    break;
  }
  if (flags & FLAG_S_COUNT) {
    // TODO Give a warning here if COUNT and CONFIRM flags are both enabled.
    flags &= ~FLAG_S_CONFIRM;
  }
  p = skipwhite(p);
  if (VIM_ISDIGIT(*p)) {
    const char *const p_s = p;
    node->count = (int) getdigits(&p);
    if (node->count == 0) {
      error->message = (char *) e_zerocount;
      error->position = p_s;
      return NOTDONE;
    }
  }
  node->args[ARG_S_FLAGS].arg.flags = flags;
  p = skipwhite(p);
  *pp = p;
#undef P_COL
  return OK;
}

static CMD_P_DEF(parse_sort)
{
  const char *p = *pp;
  uint_least32_t flags = 0;
  for (; *p; p++) {
    switch (*p) {
      case ' ':
      case TAB: {
        continue;
      }
#define SORT_FLAG(ch, flag) \
      case ch: { \
        flags |= flag; \
        continue; \
      }
      SORT_FLAG('i', FLAG_SORT_IC)
      SORT_FLAG('r', FLAG_SORT_USEMATCH)
      SORT_FLAG('u', FLAG_SORT_KEEPFST)
#undef SORT_FLAG
      case 'n': {
        flags |= FLAG_SORT_DECIMAL;
        if (flags & (FLAG_SORT_OCTAL | FLAG_SORT_HEX)) {
          goto parse_sort_numeric_error;
        }
        continue;
      }
      case 'o': {
        flags |= FLAG_SORT_OCTAL;
        if (flags & (FLAG_SORT_DECIMAL | FLAG_SORT_HEX)) {
          goto parse_sort_numeric_error;
        }
        continue;
      }
      case 'x': {
        flags |= FLAG_SORT_HEX;
        if (flags & (FLAG_SORT_DECIMAL | FLAG_SORT_OCTAL)) {
          goto parse_sort_numeric_error;
        }
        continue;
      }
      // ENDS_EXCMD
      case NL:
      case NUL:
      case '|':
      case '"': {
        break;
      }
      default: {
        if (ASCII_ISALPHA(*p)) {
          error->message = N_("E475: Expected sort flag or non-ASCII "
                              "regular expression delimiter");
          error->position = p;
          return NOTDONE;
        }
        const char delim = *p;
        p++;
        int rret;
        if (*p == delim) {
          flags |= FLAG_SORT_RE_SEARCH;
          p++;
        } else if ((rret = get_regex(&p, error,
                                     &(node->args[ARG_SORT_REG].arg.reg),
                                     delim, (char *) e_invalpat)) != OK) {
          return rret;
        }
        break;
      }
    }
    break;
  }
  node->args[ARG_SORT_FLAGS].arg.flags = flags;
  *pp = p;
  return OK;
parse_sort_numeric_error:
  error->message = N_("E474: Can only specify one kind of numeric sort");
  error->position = p;
  return NOTDONE;
}

static CMD_P_DEF(parse_syntime)
{
  uint_least32_t action = 0;
  if (STRCMP(*pp, "on") == 0) {
    action = VAL_SYNTIME_ON;
  } else if (STRCMP(*pp, "off") == 0) {
    action = VAL_SYNTIME_OFF;
  } else if (STRCMP(*pp, "clear") == 0) {
    action = VAL_SYNTIME_CLEAR;
  } else if (STRCMP(*pp, "report") == 0) {
    action = VAL_SYNTIME_REPORT;
  } else {
    error->message =
        N_("E475: Expected one action of `on', `off', `clear' or `report'");
    error->position = *pp;
    return NOTDONE;
  }
  node->args[ARG_SYNTIME_ACTION].arg.flags = action;
  *pp += STRLEN(*pp);
  return OK;
}

static CMD_P_DEF(parse_2numbers)
{
  const char *p = *pp;
  if (node->type == kCmdWinpos && *p == NUL) {
    node->args[ARG_2INTS_FLAGS].arg.flags = (uint_least32_t) true;
    return OK;
  }
  node->args[ARG_2INTS_NUM1].arg.number = (int) getdigits(&p);
  p = skipwhite(p);
  const char *const num2_start = p;
  node->args[ARG_2INTS_NUM2].arg.number = (int) getdigits(&p);
  if (*num2_start == NUL || *p != NUL) {
    switch (node->type) {
      case kCmdWinsize: {
        error->message = N_("E465: :winsize requires two number arguments");
        break;
      }
      case kCmdWinpos: {
        error->message = N_("E466: :winpos requires two number arguments");
        break;
      }
      default: {
        assert(false);
      }
    }
    error->position = (*num2_start == NUL ? num2_start : p);
    return NOTDONE;
  }
  *pp = p;
  return OK;
}

static CMD_P_DEF(parse_wincmd)
{
  const char *p = *pp;
  uint8_t action = 0;
  switch (*p) {
#define WINCMD_ACTION(ch) \
    case ch: { \
      action = (uint8_t) ch; \
      p++; \
      break; \
    }
    // split current window in two parts, horizontally
    case 'S':
    case Ctrl_S:
    WINCMD_ACTION('s')
    // split current window in two parts, vertically
    case Ctrl_V:
    WINCMD_ACTION('v')
    // split current window and edit alternate file
    case Ctrl_HAT:
    WINCMD_ACTION('^')
    // open new window
    case Ctrl_N:
    WINCMD_ACTION('n')
    // quit current window
    case Ctrl_Q:
    WINCMD_ACTION('q')
    // close current window
    case Ctrl_C:
    WINCMD_ACTION('c')
    // close preview window
    case Ctrl_Z:
    WINCMD_ACTION('z')
    // cursor to preview window
    WINCMD_ACTION('P')
    // close all but current window
    case Ctrl_O:
    WINCMD_ACTION('o')
    // cursor to next window with wrap around
    case Ctrl_W:
    WINCMD_ACTION('w')
    // cursor to previous window with wrap around
    WINCMD_ACTION('W')
    // cursor to window below
    case Ctrl_J:
    WINCMD_ACTION('j')
    // cursor to window above
    case Ctrl_K:
    WINCMD_ACTION('k')
    // cursor to left window
    case Ctrl_H:
    WINCMD_ACTION('h')
    // cursor to right window
    case Ctrl_L:
    WINCMD_ACTION('l')
    // move window to new tab page
    WINCMD_ACTION('T')
    // cursor to top-left window
    case Ctrl_T:
    WINCMD_ACTION('t')
    // cursor to bottom-right window
    case Ctrl_B:
    WINCMD_ACTION('b')
    // cursor to last accessed (previous) window
    case Ctrl_P:
    WINCMD_ACTION('p')
    // exchange current and next window
    case Ctrl_X:
    WINCMD_ACTION('x')
    // rotate windows downwards
    case Ctrl_R:
    WINCMD_ACTION('r')
    // rotate windows upwards
    WINCMD_ACTION('R')
    // move window to the very top/bottom/left/right
    WINCMD_ACTION('K')
    WINCMD_ACTION('J')
    WINCMD_ACTION('H')
    WINCMD_ACTION('L')
    // make all windows the same height
    WINCMD_ACTION('=')
    // increase current window height
    WINCMD_ACTION('+')
    // decrease current window height
    WINCMD_ACTION('-')
    // set current window height
    case Ctrl__:
    WINCMD_ACTION('_')
    // increase current window width
    WINCMD_ACTION('>')
    // decrease current window width
    WINCMD_ACTION('<')
    // set current window width
    WINCMD_ACTION('|')
    // jump to tag and split window if tag exists (in preview window)
    WINCMD_ACTION('}')
    case Ctrl_RSB:
    WINCMD_ACTION(']')
    // edit file name under cursor in a new window
    case 'F':
    case Ctrl_F:
    WINCMD_ACTION('f')
    // Go to the first occurrence of the identifier under cursor along path in 
    // a new window -- webb
    //
    // Go to any match
    WINCMD_ACTION('i')
    // Go to definition, using 'define'
    case Ctrl_D:
    WINCMD_ACTION('d')
    WINCMD_ACTION(CAR)
    // CTRL-W g  extended commands
    case 'g':
    case Ctrl_G: {
      p++;
      switch (*p) {
#define EXTENDED_WINCMD_ACTION(ch) \
        case ch: { \
          action = 0x80 | ((uint8_t) ch); \
          p++; \
          break; \
        }
        EXTENDED_WINCMD_ACTION('}');
        case Ctrl_RSB:
        EXTENDED_WINCMD_ACTION(']');
        EXTENDED_WINCMD_ACTION('f');
        EXTENDED_WINCMD_ACTION('F');
#undef EXTENDED_WINCMD_ACTION
        case NUL: {
          error->message =
              N_("E474: Expected extended window action "
                 "(see help tags starting with CTRL-W_g)");
          error->position = p + 1;
          return NOTDONE;
        }
        default: {
          p++;
          break;
        }
      }
      break;
    }
    case NUL: {
      error->message = (char *) e_argreq;
      error->position = p;
      return NOTDONE;
    }
    default: {
      p++;
      break;
    }
#undef WINCMD_ACTION
  }
  p = skipwhite(p);
  if (!ENDS_EXCMD(*p)) {
    error->message = N_("E474: Trailing characters");
    error->position = p;
    return NOTDONE;
  }
  node->args[ARG_WINCMD_CHAR].arg.ch = (char) action;
  *pp = p;
  return OK;
}

static CMD_P_DEF(parse_z)
{
  const char *p = *pp;

  const char kind = *p;
  if (kind && strchr("-+=^.", kind) != NULL) {
    node->args[ARG_Z_KIND].arg.ch = kind;
    p++;
  }
  if (kind == '+' || kind == '-') {
    size_t multiplier = 1;
    while (p[multiplier - 1] == kind) {
      multiplier++;
    }
    p += multiplier - 1;
    node->args[ARG_Z_MULTIPLIER].arg.unumber = (uint_least32_t) multiplier;
  }
  while (*p == '-' || *p == '+') {
    p++;
  }
  if (*p) {
    if (!VIM_ISDIGIT(*p)) {
      error->message = N_("E144: non-numeric argument to :z");
      error->position = p;
      return NOTDONE;
    } else {
      node->args[ARG_Z_BIGNESS].arg.unumber = (uint_least32_t) getdigits(&p);
    }
  }

  *pp = p;
  return OK;
}

static CMD_P_DEF(parse_at)
{
  char c = **pp;
  if (c == NUL || (c == '*' && node->type == kCmdStar)) {
    c = '@';
  }
  if (c != '@' && !valid_yank_reg(c, false)) {
    error->message = (const char *) e_invalidreg;
    error->position = *pp;
    return NOTDONE;
  }
  node->args[ARG_MARK_CHAR].arg.ch = c;
  *pp += STRLEN(*pp);
  return OK;
}

/// Get :help language
///
/// @param[in]      s    Start of the checked string.
/// @param[in,out]  end  Pointer to the end (e.g. NUL character) of the checked 
///                      string. Will be moved to the "@" sign if language was 
///                      found.
///
/// @return Help language (NUL-terminated string with length equal to 2) or 
///         NULL.
static inline char *get_help_lang(const char *const s, const char **end)
{
  const char *p = *end;
  if (p - 3 >= s
      && p[-3] == '@'
      && ASCII_ISALPHA(p[-2])
      && ASCII_ISALPHA(p[-1])) {
    *end -= 3;
    return xmemdupz(p - 2, 2);
  } else {
    return NULL;
  }
}

static CMD_P_DEF(parse_help)
{
  const char *p = *pp;
  const char *const s = p;
  const char *end = p;
  while (*end && *end != '\n' && *end != '\r'
        && !(*end == '|' && end[1] && end[1] != '|')) {
    end++;
  }
  p = end;
  // Remove trailing blanks
  while (end - 1 >= s && vim_iswhite(end[-1]) && end[-2] != '\\') {
    end--;
  }
  node->args[ARG_HELP_LANG].arg.str = get_help_lang(s, &end);
  if (end != s) {
    node->args[ARG_HELP_TOPIC].arg.str = xmemdupz(s, (size_t) (end - s));
  }
  *pp = p;
  return OK;
}

static CMD_P_DEF(parse_helpgrep)
{
  const char *p = *pp;
  const char *end = p + STRLEN(p);
  *pp = end;
  if (end == p) {
    error->message = (char *) e_argreq;
    error->position = p;
    return NOTDONE;
  }
  node->args[ARG_HELPG_LANG].arg.str = get_help_lang(p, &end);
  node->args[ARG_HELPG_REG].arg.reg = regex_alloc(p, (size_t) (end - p));
  return OK;
}

static CMD_P_DEF(parse_helptags)
{
  const char *p = *pp;
  const char *const s = p;
  if (STRNCMP(p, "++t", 3) == 0 && vim_iswhite(p[3])) {
    p = skipwhite(p + 3);
    node->args[ARG_HT_MAIN].arg.flags = 1;
  }
  if (*p == NUL) {
    error->message = (char *) e_argreq;
    error->position = p;
    return NOTDONE;
  }
  int pfret = parse_files(&p, error, position.col + (size_t) (p - s),
                          &(node->glob));
  if (pfret != OK) {
    return pfret;
  }
  *pp = p;
  return OK;
}

static CMD_P_DEF(parse_language)
{
  const char *p = *pp;
  const char *end = skiptowhite(p);
  LocaleType type = VAL_LANG_ALL;
  if ((*end == NUL || vim_iswhite(*end)) && end - p >= 3) {
    if (STRNICMP(p, "messages", end - p) == 0) {
      type = VAL_LANG_MESSAGES;
      p = skipwhite(end);
    } else if (STRNICMP(p, "ctype", end - p) == 0) {
      type = VAL_LANG_CTYPE;
      p = skipwhite(end);
    } else if (STRNICMP(p, "time", end - p) == 0) {
      type = VAL_LANG_TIME;
      p = skipwhite(end);
    }
  }
  node->args[ARG_LANG_TYPE].arg.flags = (uint_least32_t) type;
  if (*p != NUL) {
    const size_t len = STRLEN(p);
    node->args[ARG_LANG_LANG].arg.str = xmemdupz(p, len);
    p += len;
  }
  *pp = p;
  return OK;
}

#undef CMD_P_DEF
#undef CMD_P_ARGS

/// Check for an Ex command with optional tail.
///
/// @param[in,out]  pp   Start of the command. Is advanced to the command 
///                      argument if requested command was found.
/// @param[in]      cmd  Name of the command which is checked for.
/// @param[in]      len  Minimal length required to accept a match.
///
/// @return true if requested command was found, false otherwise.
static bool check_for_cmd(const char **pp, const char *cmd, size_t len)
  FUNC_ATTR_NONNULL_ALL FUNC_ATTR_WARN_UNUSED_RESULT
{
  size_t i;

  for (i = 0; cmd[i] != NUL; i++)
    if (cmd[i] != (*pp)[i])
      break;

  if ((i >= len) && !isalpha((*pp)[i])) {
    *pp = skipwhite(*pp + i);
    return true;
  }
  return false;
}

/// Get a single Ex adress
///
/// @param[in,out]  pp       Parsed string. Is advanced to the next character 
///                          after parsed address. May point at whitespace.
/// @param[out]     address  Structure where result will be saved.
/// @param[out]     error    Structure where errors are saved.
///
/// @return OK when parsing was successfull, FAIL otherwise.
static int get_address(const char **pp, Address *address,
                       CommandParserError *error)
  FUNC_ATTR_NONNULL_ALL FUNC_ATTR_WARN_UNUSED_RESULT
{
  const char *p;

  p = skipwhite(*pp);
  switch (*p) {
    case '.': {
      address->type = kAddrCurrent;
      p++;
      break;
    }
    case '$': {
      address->type = kAddrEnd;
      p++;
      break;
    }
    case '\'': {
      address->type = kAddrMark;
      p++;
      address->data.mark = *p;
      if (*p != NUL)
        p++;
      break;
    }
    case '/':
    case '?': {
      const char c = *p;
      p++;
      if (c == '/')
        address->type = kAddrForwardSearch;
      else
        address->type = kAddrBackwardSearch;
      int rret;
      if ((rret = get_regex(&p, error, &(address->data.regex), c, NULL))
          != OK) {
        return rret;
      }
      break;
    }
    case '\\': {
      p++;
      switch (*p) {
        case '&': {
          address->type = kAddrSubstituteSearch;
          break;
        }
        case '?': {
          address->type = kAddrBackwardPreviousSearch;
          break;
        }
        case '/': {
          address->type = kAddrForwardPreviousSearch;
          break;
        }
        default: {
          error->message = (char *) e_backslash;
          error->position = p;
          return FAIL;
        }
      }
      p++;
      break;
    }
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9': {
      address->type = kAddrFixed;
      address->data.lnr = getdigits(&p);
      break;
    }
    default: {
      address->type = kAddrMissing;
      break;
    }
  }
  *pp = p;
  return OK;
}

static int get_address_followups(const char **pp, CommandParserError *error,
                                 AddressFollowup **followup)
  FUNC_ATTR_NONNULL_ALL FUNC_ATTR_WARN_UNUSED_RESULT
{
  AddressFollowup *fw;
  AddressFollowupType type = kAddressFollowupMissing;
  const char *p;

  p = skipwhite(*pp);
  switch (*p) {
    case '-':
    case '+': {
      type = kAddressFollowupShift;
      break;
    }
    case '/': {
      type = kAddressFollowupForwardPattern;
      break;
    }
    case '?': {
      type = kAddressFollowupBackwardPattern;
      break;
    }
  }
  if (type != kAddressFollowupMissing) {
    p++;
    fw = address_followup_alloc(type);
    fw->type = type;
    switch (type) {
      case kAddressFollowupShift: {
        int sign = (p[-1] == '+' ? 1 : -1);
        if (VIM_ISDIGIT(*p))
          fw->data.shift = sign * (int) getdigits(&p);
        else
          fw->data.shift = sign;
        break;
      }
      case kAddressFollowupForwardPattern:
      case kAddressFollowupBackwardPattern: {
        int rret;
        if ((rret = get_regex(&p, error, &(fw->data.regex), p[-1], NULL))
            != OK) {
          free_address_followup(fw);
          return rret;
        }
        break;
      }
      default: {
        assert(false);
      }
    }
    *followup = fw;
    if (get_address_followups(&p, error, &(fw->next)) == FAIL)
      return FAIL;
  }
  *pp = p;
  return OK;
}

/// Check if p points to a separator between Ex commands (possibly with spaces)
///
/// @param[in]  p  Checked string
///
/// @return First character of next command (last character after command 
///         separator), NULL if no separator was found.
static const char *check_next_cmd(const char *p)
  FUNC_ATTR_CONST
{
  p = skipwhite(p);
  if (*p == '|' || *p == '\n')
    return p + 1;
  else
    return NULL;
}

// Table used to quickly search for a command, based on its first character.
static CommandType cmdidxs[27] =
{
  kCmdAppend,
  kCmdBuffer,
  kCmdChange,
  kCmdDelete,
  kCmdEdit,
  kCmdFile,
  kCmdGlobal,
  kCmdHelp,
  kCmdInsert,
  kCmdJoin,
  kCmdK,
  kCmdList,
  kCmdMove,
  kCmdNext,
  kCmdOpen,
  kCmdPrint,
  kCmdQuit,
  kCmdRead,
  kCmdSubstitute,
  kCmdT,
  kCmdUndo,
  kCmdVglobal,
  kCmdWrite,
  kCmdXit,
  kCmdYank,
  kCmdZ,
  kCmdBang
};

/// Find a built-in Ex command by its name or find user command name
///
/// @param[in,out]  pp     Parsed string. Is advanced to the next character 
///                        after the command.
/// @param[out]     type   Command type: for built-in commands the only value 
///                        returned.
/// @param[out]     name   For user commands: command name in allocated memory 
///                        (not resolved because parser does not know about 
///                        defined user commands). For built-in commands: NULL.
/// @param[out]     error  Structure where errors are saved.
///
/// @return OK if parsing was successfull, FAIL otherwise. Use error->message 
///         value to distinguish out-of-memory and failing parsing cases.
static int find_command(const char **pp, CommandType *type,
                        const char **name, CommandParserError *error)
  FUNC_ATTR_NONNULL_ALL FUNC_ATTR_WARN_UNUSED_RESULT
{
  size_t len;
  const char *p;
  size_t i;
  CommandType cmdidx = kCmdUnknown;

  *name = NULL;

  /*
   * Isolate the command and search for it in the command table.
   * Exceptions:
   * - the 'k' command can directly be followed by any character.
   * - the 's' command can be followed directly by 'c', 'g', 'i', 'I' or 'r'
   *	    but :sre[wind] is another command, as are :scrip[tnames],
   *	    :scs[cope], :sim[alt], :sig[ns] and :sil[ent].
   * - the "d" command can directly be followed by 'l' or 'p' flag.
   */
  p = *pp;
  if (*p == 'k') {
    *type = kCmdK;
    p++;
  } else if (p[0] == 's'
             && ((p[1] == 'c' && p[2] != 's' && p[2] != 'r'
                  && p[3] != 'i' && p[4] != 'p')
                 || p[1] == 'g'
                 || (p[1] == 'i' && p[2] != 'm' && p[2] != 'l' && p[2] != 'g')
                 || p[1] == 'I'
                 || (p[1] == 'r' && p[2] != 'e'))) {
    *type = kCmdSubstitute;
    p++;
  } else {
    bool found = false;

    while (ASCII_ISALPHA(*p))
      p++;
    // for python 3.x support ":py3", ":python3", ":py3file", etc.
    if ((*pp)[0] == 'p' && (*pp)[1] == 'y')
      while (ASCII_ISALNUM(*p))
        p++;

    // check for non-alpha command
    if (p == *pp && strchr("@*!=><&~#", *p) != NULL)
      p++;
    len = (size_t)(p - *pp);
    if (**pp == 'd' && (p[-1] == 'l' || p[-1] == 'p')) {
      // Check for ":dl", ":dell", etc. to ":deletel": that's
      // :delete with the 'l' flag.  Same for 'p'.
      for (i = 0; i < len; i++)
        if ((*pp)[i] != "delete"[i])
          break;
      if (i == len - 1)
        --len;
    }

    if (ASCII_ISLOWER(**pp))
      cmdidx = cmdidxs[(int)(**pp - 'a')];
    else
      cmdidx = cmdidxs[26];

    for (; (int)cmdidx < (int)kCmdSIZE;
         cmdidx = (CommandType)((int)cmdidx + 1)) {
      if (STRNCMP(CMDDEF(cmdidx).name, (char *) (*pp), (size_t) len) == 0) {
        found = true;
        break;
      }
    }

    // Look for a user defined command as a last resort.
    if (!found && **pp >= 'A' && **pp <= 'Z') {
      // User defined commands may contain digits.
      while (ASCII_ISALNUM(*p))
        p++;
      if (p == *pp)
        cmdidx = kCmdUnknown;
      *name = xmemdupz(*pp, (size_t) (p - *pp));
      *type = kCmdUSER;
    } else if (!found) {
      *type = kCmdUnknown;
      error->message = N_("E492: Not an editor command");
      error->position = *pp;
      return FAIL;
    } else {
      *type = cmdidx;
    }
  }

  *pp = p;

  return OK;
}

/// Get command argument
///
/// Not used for commands with complex relations with bar or comment symbol: 
/// e.g. :echo (it allows things like "echo 'abc|def'") or :write 
/// (":w`='abc|def'`").
///
/// @param[in]   type             Command type.
/// @param[in]   o                Parser options. See parse_one_cmd 
///                               documentation.
/// @param[in]   start            Start of the command-line arguments.
/// @param[in]   arg_start        Offset of the first character in start. Is 
///                               added to each value in skips.
/// @param[out]  arg              Resulting command-line argument.
/// @param[out]  next_cmd_offset  Offset of next command.
/// @param[out]  skips            List of the offsets used to compute real 
///                               character positions after unescaping string. 
///                               Is populated with offsets of characters which 
///                               were omitted from output (namely <C-v> and 
///                               backslashes, depending on options).
/// @param[out]  skips_count      Number of items in the skips array.
///
/// @return OK.
static int get_cmd_arg(CommandType type, CommandParserOptions o,
                       const char *start, const size_t arg_start,
                       char **arg, size_t *next_cmd_offset,
                       size_t **skips, size_t *skips_count)
{
  size_t skcnt = 0;
  const char *p = start;
  bool did_set_next_cmd_offset = false;
  for (; *p; mb_ptr_adv_(p)) {
    if (*p == Ctrl_V) {
      if (CMDDEF(type).flags & (USECTRLV)) {
      } else {
        skcnt++;
      }
      if (*p == NUL) {  // stop at NUL after CTRL-V
        break;
      }
    } else if ((*p == '"' && !(CMDDEF(type).flags & NOTRLCOM)
                && ((type != kCmdAt && type != kCmdStar)
                    || p != *arg)
                && (type != kCmdRedir
                    || p != *arg + 1 || p[-1] != '@'))
               || *p == '|' || *p == '\n') {
      if (((o.flags & FLAG_POC_CPO_BAR)
           || !(CMDDEF(type).flags & USECTRLV)) && *(p - 1) == '\\') {
        skcnt++;
      } else {
        const char *nextcmd = check_next_cmd(p);
        if (nextcmd != NULL) {
          did_set_next_cmd_offset = true;
          *next_cmd_offset = (size_t) (nextcmd - start);
        }
        break;
      }
    }
  }

  *skips_count = skcnt;

  if (!did_set_next_cmd_offset) {
    *next_cmd_offset = (size_t) (p - start);
  }

  size_t len = (size_t) (p - start) - skcnt + 1;

  // From del_trailing_spaces
  if (!(CMDDEF(type).flags & NOTRLCOM)) {  // remove trailing spaces
    while (--p > start && vim_iswhite(*p) && p[-1] != '\\' && p[-1] != Ctrl_V) {
      len--;
    }
  }

  const char *e = p;

  *arg = XMALLOC_NEW(char, len);

  if (skcnt) {
    *skips = XMALLOC_NEW(size_t, skcnt);
  } else {
    *skips = NULL;
  }

  size_t *cur_move = *skips;

  char *s = *arg;
  for (p = start; p <= e;) {
    if (*p == Ctrl_V && !(CMDDEF(type).flags&USECTRLV)) {
      p++;
      *cur_move++ = (size_t) (p - start);
      if (*p == NUL) {  // stop at NUL after CTRL-V
        break;
      }
    // Check for '"': start of comment or '|': next command
    // :@" and :*" do not start a comment!
    // :redir @" doesn't either.
    } else if ((*p == '"'
                && !(CMDDEF(type).flags & NOTRLCOM)
                && ((type != kCmdAt && type != kCmdStar)
                    || p != start)
                && (type != kCmdRedir
                    || p != start + 1
                    || p[-1] != '@'))
               || *p == '|'
               || *p == '\n') {
      // We remove the '\' before the '|', unless USECTRLV is used
      // AND 'b' is present in 'cpoptions'.
      if (((o.flags & FLAG_POC_CPO_BAR)
           || !(CMDDEF(type).flags & USECTRLV)) && p[-1] == '\\') {
        s--;  // remove the '\'
        *cur_move++ = (size_t) (p - start) - 1;
      } else {
        break;
      }
    } else if (*p == NUL) {
      break;
    }
    size_t ch_len = mb_ptr2len(p);
    memcpy(s, p, ch_len);
    s += ch_len;
    p += ch_len;
  }
  *s = NUL;

  return OK;
}

/// Fgetline implementation that simply returns given string
///
/// Useful for :execute, :*do, etc.
///
/// @param[in,out]  arg  Pointer to pointer to the start of string which will be 
///                      returned. This string must live in an allocated memory.
const char *do_fgetline_allocated(int c, const char **arg, int indent)
{
  if (*arg) {
    const char *saved_arg = *arg;
    *arg = NULL;
    return saved_arg;
  } else {
    return NULL;
  }
}

/// Parse +cmd
///
/// @param[in,out]  pp         Command to parse.
/// @param[out]     next_node  Location where parsing results are saved.
/// @param[in]      o          Options that control parsing behavior.
/// @param[in]      position   Position of input.
/// @param[in]      s          Address of position.col. Used to adjust position 
///                            in case of error.
///
/// @return OK if everything was parsed correctly, FAIL if out of memory.
///
/// @note Syntax errors in parsed command only happen *after* opening buffer.
static int parse_argcmd(const char **pp,
                        CommandNode **next_node,
                        CommandParserOptions o,
                        CommandPosition position,
                        const char *s)
{
  const char *p = *pp;

  if (*p == '+') {
    p++;
    if (vim_isspace(*p) || !*p) {
      *next_node = cmd_alloc(kCmdMissing, position);
      (*next_node)->range.address.type = kAddrEnd;
      (*next_node)->end_col = position.col + (size_t) (p - s);
    } else {
      const char *cmd_start = p;
      char *arg;
      char *arg_start;
      CommandPosition new_position = {
        position.lnr,
        position.col + (size_t) (p - s)
      };

      while (*p && !vim_isspace(*p)) {
        if (*p == '\\' && p[1] != NUL)
          p++;
        mb_ptr_adv_(p);
      }

      arg = xmemdupz(cmd_start, (size_t) (p - cmd_start));

      arg_start = arg;

      // TODO Record skips and adjust positions
      while (*arg) {
        if (*arg == '\\' && arg[1] != NUL)
          STRMOVE(arg, arg + 1);
        mb_ptr_adv_(arg);
      }

      if ((*next_node = parse_cmd_sequence(o, new_position,
                                           (VimlLineGetter) &do_fgetline_allocated,
                                           &arg_start, true))
          == NULL) {
        return FAIL;
      }

      assert(arg_start == NULL);
    }
    *pp = p;
  }
  return OK;
}

/// Parse ++opt
///
/// @param[in,out]  pp         Option to parse. Must point to the first +.
/// @param[out]     optflags   Opt flags.
/// @param[out]     enc        Encoding.
/// @param[in]      o          Options that control parsing behavior.
/// @param[out]     error      Structure where errors are saved.
///
/// @return OK if everything was parsed correctly, NOTDONE in case of error, 
///         FAIL if out of memory.
static int parse_argopt(const char **pp,
                        uint_least32_t *optflags,
                        char **enc,
                        CommandParserOptions o,
                        CommandParserError *error)
{
  bool do_ff = false;
  bool do_enc = false;
  bool do_bad = false;
  const char *arg_start;

  *pp += 2;
  if (STRNCMP(*pp, "bin", 3) == 0 || STRNCMP(*pp, "nobin", 5) == 0) {
    if (**pp == 'n') {
      *pp += 2;
      *optflags |= FLAG_OPT_BIN_USE_FLAG;
    } else {
      *optflags |= FLAG_OPT_BIN_USE_FLAG|FLAG_OPT_BIN;
    }
    if (!check_for_cmd(pp, "binary", 3)) {
      error->message = N_("E474: Expected ++[no]bin or ++[no]binary");
      error->position = *pp + 3;
      return NOTDONE;
    }
    *pp = skipwhite(*pp);
    return OK;
  }

  if (STRNCMP(*pp, "edit", 4) == 0) {
    *optflags |= FLAG_OPT_EDIT;
    *pp = skipwhite(*pp + 4);
    return OK;
  }

  if (STRNCMP(*pp, "ff", 2) == 0) {
    *pp += 2;
    do_ff = true;
  } else if (STRNCMP(*pp, "fileformat", 10) == 0) {
    *pp += 10;
    do_ff = true;
  } else if (STRNCMP(*pp, "enc", 3) == 0) {
    if (STRNCMP(*pp, "encoding", 8) == 0)
      *pp += 8;
    else
      *pp += 3;
    do_enc = true;
  } else if (STRNCMP(*pp, "bad", 3) == 0) {
    *pp += 3;
    do_bad = true;
  } else {
    error->message = N_("E474: Unknown ++opt");
    error->position = *pp;
    return NOTDONE;
  }

  if (**pp != '=') {
    error->message = N_("E474: Option requires argument: use ++opt=arg");
    error->position = *pp;
    return NOTDONE;
  }

  (*pp)++;

  arg_start = *pp;

  while (**pp && !vim_isspace(**pp)) {
    if (**pp == '\\' && (*pp)[1] != NUL)
      (*pp)++;
    mb_ptr_adv_((*pp));
  }

  if (do_ff) {
    // XXX check_ff_value requires NUL-terminated string. Thus I duplicate list 
    //     of accepted strings here
    switch (*arg_start) {
      case 'd': {
        if (*pp == arg_start + 3
            && arg_start[1] == 'o'
            && arg_start[2] == 's')
          *optflags |= VAL_OPT_FF_DOS;
        else
          goto parse_argopt_ff_error;
        break;
      }
      case 'u': {
        if (*pp == arg_start + 4
            && arg_start[1] == 'n'
            && arg_start[2] == 'i'
            && arg_start[3] == 'x')
          *optflags |= VAL_OPT_FF_UNIX;
        else
          goto parse_argopt_ff_error;
        break;
      }
      case 'm': {
        if (*pp == arg_start + 3
            && arg_start[1] == 'a'
            && arg_start[2] == 'c')
          *optflags |= VAL_OPT_FF_MAC;
        else
          goto parse_argopt_ff_error;
        break;
      }
      default: {
        goto parse_argopt_ff_error;
      }
    }
  } else if (do_enc) {
    char *e;
    *enc = xmemdupz(arg_start, (size_t) (*pp - arg_start));
    for (e = *enc; *e != NUL; e++)
      *e = TOLOWER_ASC(*e);
  } else if (do_bad) {
    size_t len = (size_t) (*pp - arg_start);
    if (STRNICMP(arg_start, "keep", len) == 0) {
      *optflags |= VAL_OPT_BAD_KEEP;
    } else if (STRNICMP(arg_start, "drop", len) == 0) {
      *optflags |= VAL_OPT_BAD_DROP;
    } else if (MB_BYTE2LEN((size_t) *arg_start) == 1 && *pp == arg_start + 1) {
      *optflags |= CHAR_TO_VAL_OPT_BAD(*arg_start);
    } else {
      error->message = N_("E474: Invalid ++bad argument: use "
                          "\"keep\", \"drop\" or a single-byte character");
      error->position = arg_start;
      return NOTDONE;
    }
  } else {
    assert(false);
  }
  *pp = skipwhite(*pp);
  return OK;
parse_argopt_ff_error:
  error->message = N_("E474: Invalid ++ff argument");
  error->position = arg_start;
  return NOTDONE;
}

/// Parse a range specifier of the form addr[,addr][;addr]…
///
/// Here `addr` is `main[+-]followup[+-]followup…` where `main` is one of
///
/// `main` | Accepts followup | Description
/// ------ | ---------------- | ------------------------------------------------
///  `%`   |       No         | Entire buffer. Equivalent to `1,$`.
///  `*`   |       No         | Visual range. Equivalent to `'<,'>`.
///  `$`   |       Yes        | Last line of the buffer.
///  `.`   |       Yes        | Current line.
/// empty  |       Yes        | Current line.
///  NUM   |       Yes        | Line number NUM in the buffer.
///  `'x`  |       Yes        | Mark `x`, defined for the current buffer.
/// `/re/` |       Yes        | Next line matching `re`.
/// `?re?` |       Yes        | Previous line matching `re`.
///
/// and `followup` is one of
///
/// `followup` | Description
/// ---------- | --------------------------------------------
///    NUM     | Move address defined by `main` by NUM lines.
///   `/re/`   | Search for `re` after `main`.
///   `?re?`   | Search for `re` before `main`.
///
/// @param[in,out]  pp           Command to parse.
/// @param[out]     node         Location where parsing results are saved. Only 
///                              applicable if parser errors need to be saved.
/// @param[in]      o            Options that control parsing behavior.
/// @param[in]      position     Position of input.
/// @param[out]     range        Location where to save resulting range.
/// @param[out]     range_start  Location where to save pointer to the first 
///                              character of the range.
///
/// @return OK if everything was parsed correctly, NOTDONE in case of error, 
///         FAIL otherwise.
int parse_range(const char **pp, CommandNode **node, CommandParserOptions o,
                CommandPosition position, Range *range,
                const char **range_start)
{
  const char *p = *pp;
  const char *s = p;
  Range current_range;

  *range_start = NULL;

  memset(range, 0, sizeof(Range));
  memset(&current_range, 0, sizeof(Range));

  // repeat for all ',' or ';' separated addresses
  for (;;) {
    CommandParserError error = {
      .message = NULL
    };
    p = skipwhite(p);
    if (*range_start == NULL)
      *range_start = p;
    if (get_address(&p, &current_range.address, &error) == FAIL) {
      free_range_data(&current_range);
      if (error.message == NULL)
        return FAIL;
      if (create_error_node(node, &error, position, s) == FAIL)
        return FAIL;
      return NOTDONE;
    }
    if (get_address_followups(&p, &error, &current_range.address.followups)
        == FAIL) {
      free_range_data(&current_range);
      if (error.message == NULL)
        return FAIL;
      if (create_error_node(node, &error, position, s) == FAIL)
        return FAIL;
      return NOTDONE;
    }
    p = skipwhite(p);
    if (current_range.address.followups != NULL) {
      if (current_range.address.type == kAddrMissing)
        current_range.address.type = kAddrCurrent;
    } else if (range->address.type == kAddrMissing
               && current_range.address.type == kAddrMissing) {
      if (*p == '%') {
        current_range.address.type = kAddrFixed;
        current_range.address.data.lnr = 1;
        current_range.next = XCALLOC_NEW(Range, 1);
        current_range.next->address.type = kAddrEnd;
        *range = current_range;
        p++;
        break;
      } else if (*p == '*' && !(o.flags&FLAG_POC_CPO_STAR)) {
        current_range.address.type = kAddrMark;
        current_range.address.data.mark = '<';
        current_range.next = XCALLOC_NEW(Range, 1);
        current_range.next->address.type = kAddrMark;
        current_range.next->address.data.mark = '>';
        *range = current_range;
        p++;
        break;
      }
    }
    current_range.setpos = (*p == ';');
    if (range->address.type != kAddrMissing) {
      Range **target = (&(range->next));
      while (*target != NULL)
        target = &((*target)->next);
      *target = XCALLOC_NEW(Range, 1);
      **target = current_range;
    } else {
      *range = current_range;
    }
    memset(&current_range, 0, sizeof(Range));
    if (*p == ';' || *p == ',')
      p++;
    else
      break;
  }
  *pp = p;
  return OK;
}

/// Parses command modifiers
///
/// @param[in,out]  pp        Command to parse.
/// @param[out]     node      Location where parsing results are saved.
/// @param[in]      o         Options that control parsing behavior.
/// @param[in]      position  Position of input.
/// @param[in]      pstart    Position of the first leading digit, if any. 
///                           Points to the same location pp does otherwise.
/// @param[out]     type      Resulting command type.
///
/// @return OK if everything was parsed correctly, NOTDONE in case of error, 
///         FAIL otherwise.
int parse_modifiers(const char **pp, CommandNode ***node,
                    CommandParserOptions o, CommandPosition position,
                    const char *const pstart, CommandType *type)
{
  const char *p = *pp;
  const char *mod_start = p;
  const char *s = p;

  // FIXME (genex_cmds.lua): Iterate over precomputed table with only modifier 
  //                         commands
  for (int i = cmdidxs[(int) (*p - 'a')]; *(CMDDEF(i).name) == *p; i++) {
    if (CMDDEF(i).flags & ISMODIFIER) {
      size_t common_len = 0;
      if (i > 0) {
        const char *name = CMDDEF(i).name;
        const char *prev_name = CMDDEF(i - 1).name;
        common_len++;
        // FIXME (genex_cmds.lua): Precompute and record this in cmddefs
        while (name[common_len] == prev_name[common_len])
          common_len++;
      }
      if (check_for_cmd(&p, CMDDEF(i).name, common_len + 1)) {
        *type = (CommandType) i;
        break;
      }
    }
  }
  if (*type != kCmdUnknown) {
    if (VIM_ISDIGIT(*pstart) && !((CMDDEF(*type).flags) & COUNT)) {
      CommandParserError error = {
        .message = (char *) e_norange,
        .position = pstart
      };
      if (create_error_node(*node, &error, position, s) == FAIL)
        return FAIL;
      return NOTDONE;
    }
    if (*p == '!' && !(CMDDEF(*type).flags & BANG)) {
      CommandParserError error = {
        .message = (char *) e_nobang,
        .position = p
      };
      if (create_error_node(*node, &error, position, s) == FAIL)
        return FAIL;
      return NOTDONE;
    }
    **node = cmd_alloc(*type, position);
    if (VIM_ISDIGIT(*pstart)) {
      (**node)->has_count = true;
      (**node)->count = (int) getdigits(&pstart);
    }
    if (*p == '!') {
      (**node)->bang = true;
      p++;
    }
    (**node)->position.col = position.col + (size_t) (mod_start - s);
    (**node)->end_col = position.col + ((size_t) (p - s) - 1);
    *node = &((**node)->children);
    *type = kCmdUnknown;
  } else {
    p = pstart;
  }
  *pp = p;
  return OK;
}

/// Add comment node
///
/// @param[in,out]  pp            Parsed string, points to the start of the 
///                               comment.
/// @param[in]      comment_type  Comment type (there are hashbang comments and 
///                               '"' comments).
/// @param[out]     node          Address where node should be saved.
/// @param[in]      position      Node position.
/// @param[in]      s             Start of the string (position pointed by 
///                               `position` argument).
///
/// @return OK
static int set_comment_node(const char **pp, CommandType comment_type, 
                            CommandNode **node, CommandPosition position,
                            const char *const s)
  FUNC_ATTR_NONNULL_ALL
{
  *node = cmd_alloc(comment_type, position);
  size_t len = STRLEN(*pp);
  (*node)->args[0].arg.str = xmemdup(*pp, len + 1);
  *pp += len;
  (*node)->end_col = position.col + (size_t) (*pp - s);
  return OK;
}

/// Parse lines without commands
///
/// Duplicates vi behaviour:
///
/// - `:3` jumps to line 3
/// - `:3|…` prints line 3
/// - `:|` prints current line
///
/// Is also responsible for creating comment nodes (unless they were created 
/// earlier).
///
/// @param[in,out]  pp            String being parsed.
/// @param[out]     node          Address where parsed node will be saved.
/// @param[in]      o             Parser options, as described in parse_one_cmd 
///                               documentation.
/// @param[in]      position      Position where parsing started.
/// @param[in]      s             Start of the string (position pointed to by 
///                               `position` argument).
/// @param[in]      prev_cmd_end  End of the previous command. Will be saved to 
///                               the current node’s end_col member if there is 
///                               current node.
/// @param[in]      range         Previously parsed range.
/// @param[in]      nextcmd       Start of the next command.
///
/// @returns OK.
static int parse_no_cmd(const char **pp,
                        CommandNode **node,
                        CommandParserOptions o,
                        CommandPosition position,
                        const char *const s,
                        const char *const prev_cmd_end,
                        Range range,
                        const char *const nextcmd)
  FUNC_ATTR_NONNULL_ALL
{
  CommandNode **next_node = node;
  const char *p = *pp;
  if (NODE_IS_ALLOCATED(*next_node)) {
    (*next_node)->end_col = position.col + (size_t) (p - prev_cmd_end);
  }
  if (*p == '|' || (o.flags&FLAG_POC_EXMODE
                    && range.address.type != kAddrMissing)) {
    *next_node = cmd_alloc(kCmdPrint, position);
    (*next_node)->range = range;
    p++;
    *pp = p;
    (*next_node)->end_col = position.col + (size_t) (*pp - s);
    return OK;
  } else if (*p == '"') {
    free_range_data(&range);
    *pp = p + 1;
    return set_comment_node(pp, kCmdComment, next_node, position, s);
  } else {
    *next_node = cmd_alloc(kCmdMissing, position);
    (*next_node)->range = range;

    *pp = (nextcmd == NULL
           ? p
           : nextcmd);
    (*next_node)->end_col = position.col + (size_t) (*pp - s);
    return OK;
  }
}

/// Parse a sequence of file names
///
/// @param[in,out]  pp     Parsed string.
/// @param[out]     error  Address where errors are saved.
/// @param[in]      col    Column number of the first parsed character.
/// @param[out]     glob   Address where result will be saved.
///
/// @return FAIL in case of failure, NOTDONE in case of error, OK in case of 
///         success.
static int parse_files(const char **pp, CommandParserError *error,
                       const size_t col, Glob *glob)
{
  const char *p = *pp;
  const char *s = p;
  Glob *cur_glob = glob;
  Glob **next = &cur_glob;
  while (!ENDS_EXCMD(*p)) {
    Pattern *pat = NULL;
    p = skipwhite(p);
    int pret;
    if ((pret = get_pattern(&p, error, &pat, false, true,
                            (size_t) (p - s) + col))
        == FAIL) {
      return FAIL;
    }
    if (pret == NOTDONE) {
      free_pattern(pat);
      free(pat);
      return NOTDONE;
    }
    if (pat != NULL) {
      if (*next == NULL) {
        *next = XCALLOC_NEW(Glob, 1);
      }
      (*next)->pat = *pat;
      free(pat);
      next = &((*next)->next);
    }
  }
  *pp = p;
  return OK;
}

/// Parses one command
///
/// @param[in,out]  pp        Command to parse.
/// @param[out]     node      Parsing result. Should be freed with free_cmd.
/// @param[in]      o         Options that control parsing behavior.
/// @parblock
///   o.flags:
///
///    Flag                 | Description
///    -------------------- | -------------------------------------------------
///    FLAG_POC_EXMODE      | Is set if parser is called for Ex mode.
///    FLAG_POC_CPO_STAR    | Is set if CPO_STAR flag is present in &cpo.
///    FLAG_POC_CPO_SPECI   | Is set if CPO_SPECI flag is present in &cpo.
///    FLAG_POC_CPO_KEYCODE | Is set if CPO_KEYCODE flag is present in &cpo.
///    FLAG_POC_CPO_BAR     | Is set if CPO_BAR flag is present in &cpo.
///    FLAG_POC_CPO_SUBPC   | Is set if CPO_SUBPERCENT flag is present in &cpo.
///    FLAG_POC_ALTKEYMAP   | Is set if &altkeymap option is set.
///    FLAG_POC_RL          | Is set if &rl option is set.
///    FLAG_POC_MAGIC       | Is set if &magic option is set.
///    FLAG_POC_ED          | Is set if &edcompatible option is set.
/// @endparblock
/// @param[in]      position  Position of input.
/// @param[in]      fgetline  Function used to obtain the next line.
/// @param[in,out]  cookie    Second argument to fgetline.
///
/// @return OK if everything was parsed correctly, FAIL if out of memory, 
///         NOTDONE for parser error.
int parse_one_cmd(const char **pp,
                  CommandNode **node,
                  CommandParserOptions o,
                  CommandPosition position,
                  VimlLineGetter fgetline,
                  void *cookie)
  FUNC_ATTR_NONNULL_ALL
{
  CommandNode **next_node = node;
  // Node options, populated before allocation
  CommandType type = kCmdUnknown;
  bool bang = false;
  Range range = {{kAddrMissing, {NULL}, NULL}, false, NULL};
  uint_least8_t exflags = 0;
  const char *name = NULL;
  bool has_count = false;
  int count = 0;
  Register reg = {NUL, NULL};
  CommandNode *children = NULL;
  uint_least32_t optflags = 0;
  char *enc = NULL;
  Glob glob = {{kPatMissing, {NULL}, NULL}, NULL};
  size_t *skips = NULL;
  size_t skips_count = 0;

  CommandParserError error = {NULL, NULL};

  const char *p;
  // Start of the command: position pointed to by `position` argument
  const char *s = *pp;

#define FREE_CMD_ARG_START \
  do { \
    if (used_get_cmd_arg) { \
      free((void *) cmd_arg_start); \
      cmd_arg_start = NULL; \
    } \
  } while (0)
#define FAIL_RET do { ret = FAIL; goto parse_one_cmd_free_return; } while (0)
  bool used_get_cmd_arg = false;
  const char *cmd_arg;
  const char *cmd_arg_start;
  size_t next_cmd_offset = 0;
  size_t *cur_skip = skips;
  const char *real_cmd_arg = NULL;
  int ret = OK;

  if (((*pp)[0] == '#') &&
      ((*pp)[1] == '!') &&
      position.col == 1) {
    *pp += 2;
    return set_comment_node(pp, kCmdHashbangComment, next_node, position, s);
  }

  cmd_arg_start = cmd_arg = p = *pp;
  for (;;) {
    const char *pstart;
    // 1. skip comment lines and leading white space and colons
    while (*p == ' ' || *p == TAB || *p == ':')
      p++;
    // in ex mode, an empty line works like :+ (switch to next line)
    if (*p == NUL && o.flags&FLAG_POC_EXMODE) {
      AddressFollowup *fw;
      *next_node = cmd_alloc(kCmdMissing, position);
      (*next_node)->range.address.type = kAddrCurrent;
      fw = address_followup_alloc(kAddressFollowupShift);
      fw->data.shift = 1;
      (*next_node)->range.address.followups = fw;
      (*next_node)->end_col = position.col + (size_t) (*pp - s);
      return OK;
    }

    if (*p == '"') {
      *pp = p + 1;
      return set_comment_node(pp, kCmdComment, next_node, position, s);
    }

    if (*p == NUL) {
      *pp = p;
      return OK;
    }

    pstart = p;
    if (VIM_ISDIGIT(*p))
      p = skipwhite(skipdigits(p));

    // 2. handle command modifiers.
    if (ASCII_ISLOWER(*p)) {
      CommandPosition new_position = position;
      new_position.col += (size_t) (p - s);
      int mret = parse_modifiers(&p, &next_node, o, new_position, pstart,
                                 &type);
      if (mret != OK) {
        return mret;
      } else if (p == pstart) {
        break;
      }
    } else {
      p = pstart;
      break;
    }
  }

  const char *modifiers_end = p;
  const char *range_start = NULL;
  // 3. parse a range specifier
  {
    CommandPosition new_position = position;
    new_position.col += (size_t) (p - s);
    int rret = parse_range(&p, next_node, o, new_position, &range,
                           &range_start);
    if (rret != OK) {
      return rret;
    }
  }

  // 4. parse command

  // Skip ':' and any white space
  while (*p == ' ' || *p == TAB || *p == ':')
    p++;

  const char *nextcmd = NULL;
  if (*p == NUL || *p == '"' || (nextcmd = check_next_cmd(p)) != NULL) {
    *pp = p;
    return parse_no_cmd(pp, next_node, o, position, s, modifiers_end, range,
                        nextcmd);
  }

  if (find_command(&p, &type, &name, &error) == FAIL) {
    goto parse_one_cmd_checked_error_return;
  }

  // Here used to be :Ni! egg. It was removed

  if (*p == '!') {
    if (CMDDEF(type).flags & BANG) {
      p++;
      bang = true;
    } else {
      error.message = (char *) e_nobang;
      error.position = p;
      goto parse_one_cmd_error_return;
    }
  }

  if (range.address.type != kAddrMissing && !(CMDDEF(type).flags & RANGE)) {
    error.message = (char *) e_norange;
    error.position = range_start;
    goto parse_one_cmd_error_return;
  }

  // Skip to start of argument.
  // Don't do this for the ":!" command, because ":!! -l" needs the space.
  if (type != kCmdBang)
    p = skipwhite(p);

  if (CMDDEF(type).flags & ARGOPT) {
    while (p[0] == '+' && p[1] == '+') {
      int aret;
      if ((aret = parse_argopt(&p, &optflags, &enc, o, &error)) == FAIL) {
        FAIL_RET;
      }
      if (aret == NOTDONE) {
        goto parse_one_cmd_error_return;
      }
    }
  }

  if (CMDDEF(type).flags & EDITCMD) {
    if (parse_argcmd(&p, &children, o, position, s) == FAIL) {
      FAIL_RET;
    }
  }

  if (CMDDEF(type).flags & REGSTR
      && *p != NUL
      // Numbered registers are not allowed for if count is allowed
      && !(CMDDEF(type).flags & COUNT && VIM_ISDIGIT(*p))) {
    if (valid_yank_reg(*p, type != kCmdPut)) {
      if (*p == '=') {
        p++;
        const char *expr_start = p;
        used_get_cmd_arg = true;
        char *new_cmd_arg = NULL;
        CommandPosition arg_position = position;
        arg_position.col += (size_t) (p - s);
        if (get_cmd_arg(type, o, p, arg_position.col, &new_cmd_arg,
                        &next_cmd_offset, &skips, &skips_count)
            == FAIL) {
          free_cmd(*node);
          *node = NULL;
          FAIL_RET;
        }
        cmd_arg = cmd_arg_start = new_cmd_arg;
        ExpressionParserError expr_error;
        reg.name = '=';
        if ((reg.expr = parse_one_expression(&cmd_arg, &expr_error, &parse0_err,
                                             position.col)) == NULL) {
          error.message = expr_error.message;
          error.position = expr_error.position;
          goto parse_one_cmd_checked_error_return;
        }
        // Adjust p according to cmd_arg adjustment
        p += (cmd_arg - cmd_arg_start);
        if (skips_count) {
          // TODO Test behavior when skip occurs at the very end
          cur_skip = skips;
          while (*cur_skip < (size_t) (p - expr_start)) {
            p++;
            cur_skip++;
          }
          real_cmd_arg = p;
        }
      } else {
        reg.name = *p;
        p++;
      }
    } else {
      error.message = (const char *) e_invalidreg;
      error.position = p;
      goto parse_one_cmd_error_return;
    }
  }

  if (CMDDEF(type).flags & COUNT && !(CMDDEF(type).flags & BUFNAME)) {
    if (VIM_ISDIGIT(*p)) {
      has_count = true;
      count = (int) getdigits(&p);
      p = skipwhite(p);
    }
  }

  if (CMDDEF(type).flags & EXFLAGS) {
    for (;;) {
      switch (*p) {
        case 'l': {
          exflags |= FLAG_EX_LIST;
          p++;
          continue;
        }
        case '#': {
          exflags |= FLAG_EX_LNR;
          p++;
          continue;
        }
        case 'p': {
          exflags |= FLAG_EX_PRINT;
          p++;
          continue;
        }
        default: {
          break;
        }
      }
      break;
    }
  }

  if (CMDDEF(type).flags & (XFILE|BUFNAME)) {
    switch (parse_files(&p, &error, (size_t) (p - s) + position.col, &glob)) {
      case OK: {
        break;
      }
      case NOTDONE: {
        goto parse_one_cmd_error_return;
      }
      case FAIL: {
        goto parse_one_cmd_free_return;
      }
    }
  }

  if (!(CMDDEF(type).flags & EXTRA)
      && *p != NUL
      && *p != '"'
      && (*p != '|' || !(CMDDEF(type).flags & TRLBAR))) {
    error.message = (char *) e_trailing;
    error.position = p;
    goto parse_one_cmd_error_return;
  }

  if (NODE_IS_ALLOCATED(*next_node)) {
    (*next_node)->end_col = position.col + (size_t) (p - modifiers_end);
  }
  *next_node = cmd_alloc(type, position);
  (*next_node)->bang = bang;
  (*next_node)->range = range;
  (*next_node)->name = (char *) name;
  (*next_node)->exflags = exflags;
  (*next_node)->has_count = has_count;
  (*next_node)->count = count;
  (*next_node)->reg = reg;
  (*next_node)->children = children;
  (*next_node)->optflags = optflags;
  (*next_node)->enc = enc;
  (*next_node)->glob = glob;
  (*next_node)->skips = skips;
  (*next_node)->skips_count = skips_count;

  CommandArgsParser parse = CMDDEF(type).parse;

  if (parse != NULL) {
    // Adjust cmd_arg according to p
    if (used_get_cmd_arg) {
      cmd_arg += (p - real_cmd_arg);
      if (skips_count) {
        // TODO Test behavior when skip occurs at the very end
        while (*cur_skip < (size_t) (p - real_cmd_arg)) {
          cmd_arg--;
          cur_skip++;
        }
      }
    } else {
      cmd_arg = p;
    }
    CommandPosition arg_position = position;
    arg_position.col += (size_t) (p - s);
    // XFILE commands may have bars inside `=…`
    // ISGREP commands may have bars inside patterns
    // ISEXPR commands may have bars inside "" or as logical OR
    if (!used_get_cmd_arg
        && !(CMDDEF(type).flags & (XFILE|ISGREP|ISEXPR|LITERAL))) {
      used_get_cmd_arg = true;
      char *new_cmd_arg = NULL;
      if (get_cmd_arg(type, o, p, arg_position.col, &new_cmd_arg,
                      &next_cmd_offset, &((*next_node)->skips),
                      &((*next_node)->skips_count))
          == FAIL) {
        goto parse_one_cmd_node_free_return;
      }
      cmd_arg = cmd_arg_start = new_cmd_arg;
    }
    int pret;
    if ((pret = parse(&cmd_arg, *next_node, &error, o, arg_position, fgetline,
                      cookie))
        == FAIL) {
      goto parse_one_cmd_node_free_return;
    }
    assert(pret == NOTDONE || error.message == NULL);
    if (pret == NOTDONE) {
      free_cmd(*next_node);
      *next_node = NULL;
      if (create_error_node(next_node, &error, position,
                            used_get_cmd_arg ? cmd_arg_start : s) == FAIL)
        return FAIL;
      return NOTDONE;
    } else if (used_get_cmd_arg) {
      if (*cmd_arg != NUL) {
        free_cmd(*next_node);
        *next_node = NULL;
        error.message = (char *) e_trailing;
        error.position = cmd_arg;
        if (create_error_node(next_node, &error, position, cmd_arg_start)
            == FAIL) {
          FREE_CMD_ARG_START;
          return FAIL;
        }
        FREE_CMD_ARG_START;
        return NOTDONE;
      }
      p += next_cmd_offset;
    } else {
      p = cmd_arg;
    }
    ret = pret;
  }

  FREE_CMD_ARG_START;
  *pp = p;
  (*next_node)->end_col = position.col + (size_t) (*pp - s);
  return ret;
parse_one_cmd_checked_error_return:
  if (error.message == NULL) {
    ret = FAIL;
  }
parse_one_cmd_error_return:
  if (create_error_node(next_node, &error, position, s) == FAIL) {
    ret = FAIL;
  } else {
    ret = NOTDONE;
  }
parse_one_cmd_free_return:
  FREE_CMD_ARG_START;
  free_range_data(&range);
  free_glob(&glob);
  free_expr(reg.expr);
  free_cmd(children);
  return ret;
parse_one_cmd_node_free_return:
  FREE_CMD_ARG_START;
  free_cmd(*next_node);
  *next_node = NULL;
  return FAIL;
#undef FREE_CMD_ARG_START
#undef FAIL_RET
}

static void get_block_options(CommandType type, CommandBlockOptions *bo)
{
  memset(bo, 0, sizeof(CommandBlockOptions));
  switch (type) {
    case kCmdEndif: {
      bo->no_start_message  = N_("E580: :endif without :if");
      bo->find_in_stack_3 = kCmdElse;
      bo->find_in_stack_2 = kCmdElseif;
      bo->find_in_stack   = kCmdIf;
      break;
    }
    case kCmdElseif: {
      bo->not_after_message = N_("E584: :elseif after :else");
      bo->no_start_message  = N_("E582: :elseif without :if");
      bo->find_in_stack_2 = kCmdElseif;
      bo->find_in_stack   = kCmdIf;
      bo->not_after = kCmdElse;
      bo->push_stack = true;
      break;
    }
    case kCmdElse: {
      bo->no_start_message  = N_("E581: :else without :if");
      bo->duplicate_message = N_("E583: multiple :else");
      bo->find_in_stack_2 = kCmdElseif;
      bo->find_in_stack   = kCmdIf;
      bo->push_stack = true;
      break;
    }
    case kCmdEndfunction: {
      bo->no_start_message  = N_("E193: :endfunction not inside a function");
      bo->find_in_stack   = kCmdFunction;
      break;
    }
    case kCmdEndtry: {
      bo->no_start_message  = N_("E602: :endtry without :try");
      bo->find_in_stack_3 = kCmdFinally;
      bo->find_in_stack_2 = kCmdCatch;
      bo->find_in_stack   = kCmdTry;
      break;
    }
    case kCmdFinally: {
      bo->no_start_message  = N_("E606: :finally without :try");
      bo->duplicate_message = N_("E607: multiple :finally");
      bo->find_in_stack_2 = kCmdCatch;
      bo->find_in_stack   = kCmdTry;
      bo->push_stack = true;
      break;
    }
    case kCmdCatch: {
      bo->not_after_message = N_("E604: :catch after :finally");
      bo->no_start_message  = N_("E603: :catch without :try");
      bo->find_in_stack_2 = kCmdCatch;
      bo->find_in_stack   = kCmdTry;
      bo->not_after = kCmdFinally;
      bo->push_stack = true;
      break;
    }
    case kCmdEndfor: {
      bo->not_after_message = N_("E732: Using :endfor with :while");
      bo->no_start_message  = (char *) e_for;
      bo->find_in_stack   = kCmdFor;
      bo->not_after = kCmdWhile;
      break;
    }
    case kCmdEndwhile: {
      bo->not_after_message = N_("E733: Using :endwhile with :for");
      bo->no_start_message  = (char *) e_while;
      bo->find_in_stack   =kCmdWhile;
      bo->not_after = kCmdFor;
      break;
    }
    case kCmdIf:
    case kCmdFunction:
    case kCmdTry:
    case kCmdFor:
    case kCmdWhile: {
      bo->push_stack = true;
      break;
    }
    default: {
      break;
    }
  }
}

/// Get message for missing block command ends
///
/// @param[in]  type  Command for which message should be obtained. Must be one 
///                   of the commands that starts block or separates it (i.e. 
///                   if, else, elseif, function, finally, catch, try, for, 
///                   while). Behavior is undefined if called for other 
///                   commands.
///
/// @return Pointer to the error message. Must not be freed.
static char *get_missing_message(CommandType type)
{
  char *missing_message = NULL;
  switch (type) {
    case kCmdElseif:
    case kCmdElse:
    case kCmdIf: {
      missing_message   = (char *) e_endif;
      break;
    }
    case kCmdFunction: {
      missing_message   = N_("E126: Missing :endfunction");
      break;
    }
    case kCmdFinally:
    case kCmdCatch:
    case kCmdTry: {
      missing_message   = (char *) e_endtry;
      break;
    }
    case kCmdFor: {
      missing_message   = (char *) e_endfor;
      break;
    }
    case kCmdWhile: {
      missing_message   = (char *) e_endwhile;
      break;
    }
    default: {
      assert(false);
    }
  }
  return missing_message;
}

const CommandNode nocmd = {
  .type = kCmdMissing,
  .name = NULL,
  .prev = NULL,
  .next = NULL,
  .children = NULL,
  .range = {
    .address = {
      .type = kAddrMissing,
      .data = {.regex = NULL},
      .followups = NULL,
    },
    .setpos = false,
    .next = NULL,
  },
  .skips = NULL,
  .skips_count = 0,
  .position = {
    .lnr = 0,
    .col = 0,
  },
  .end_col = 0,
  .has_count = false,
  .count = 0,
  .reg = {
    .name = NUL,
    .expr = NULL,
  },
  .exflags = 0,
  .optflags = 0,
  .enc = NULL,
  .glob = {
    .pat = {
      .type = kPatMissing,
      .data = {.pats = {.pat = NULL, .next = NULL}},
      .next = NULL,
    },
    .next = NULL,
  },
  .bang = false,
  .args = {
    {
      .arg = {
        .str = NULL,
      }
    },
  }
};

#define _NEW_ERROR_NODE(next_node, prev_node, error_message, error_position, \
                       line_start) \
        { \
          CommandParserError error; \
          error.message = error_message; \
          error.position = error_position; \
          assert(prev_node == NULL || prev_node->next == NULL || \
                 next_node != NULL); \
          if (create_error_node(prev_node == NULL \
                                ? next_node \
                                : &(prev_node->next), &error, position, \
                                line_start) \
              == FAIL) { \
            free_cmd(result); \
            free(line_start); \
            return FAIL; \
          } \
          if (prev_node != NULL) \
            prev_node->next->prev = prev_node; \
        }
#define NEW_ERROR_NODE(prev_node, error_message, error_position, line_start) \
        { \
          _NEW_ERROR_NODE(NULL, prev_node, error_message, error_position, \
                          line_start) \
          assert(prev_node != NULL); \
          prev_node = prev_node->next; \
        }
#define NEW_BLOCK_SEP_ERROR_NODE(next_node, blockstack, blockstack_len, \
                                 prev_node, error_message, push_stack, \
                                 error_position, line_start) \
        { \
          free_cmd(*next_node); \
          *next_node = NULL; \
          NEW_ERROR_NODE(blockstack[blockstack_len - 1].node, \
                         error_message, error_position, line_start) \
          if (push_stack) { \
            prev_node = NULL; \
            next_node = &(blockstack[blockstack_len - 1].node->children); \
          } else { \
            prev_node = blockstack[blockstack_len - 1].node; \
            next_node = &(prev_node->next); \
            blockstack_len--; \
          } \
        }
#define APPEND_ERROR_NODE(next_node, prev_node, error_message, \
                          error_position, line_start) \
        { \
          _NEW_ERROR_NODE(next_node, prev_node, error_message, \
                          error_position, line_start) \
          prev_node = prev_node == NULL ? *next_node : prev_node->next; \
          next_node = &(prev_node->next); \
        }

/// Parses sequence of commands
///
/// @param[in]      o         Options that control parsing behavior. In addition 
///                           to flags documented in parse_one_cmd documentation 
///                           it accepts o.early_return option that makes in not 
///                           call fgetline once there is something to execute.
/// @param[in]      position  Position of input.
/// @param[in]      fgetline  Function used to obtain the next line.
/// @param[in,out]  cookie    Second argument to fgetline.
/// @param[in]      can_free  True if strings returned by fgetline can be freed.
CommandNode *parse_cmd_sequence(CommandParserOptions o,
                                CommandPosition position,
                                VimlLineGetter fgetline,
                                void *cookie,
                                bool can_free)
{
  char *line_start;
  char *line;
  struct {
    CommandType type;
    CommandNode *node;
    CommandNode *block_node;
  } blockstack[MAX_NEST_BLOCKS];
  CommandNode *result = (CommandNode *) &nocmd;
  CommandNode **next_node = &result;
  CommandNode *prev_node = NULL;
  size_t blockstack_len = 0;

  while ((line_start = fgetline(':', cookie, 0)) != NULL) {
    line = line_start;
    while (*line) {
      const char *parse_start = line;
      CommandBlockOptions bo;
      int ret;
      CommandType block_type = kCmdMissing;
      CommandNode *block_command_node;

      position.col = (size_t) (line - line_start) + 1;
      assert(!NODE_IS_ALLOCATED(*next_node));
      if ((ret = parse_one_cmd((const char **) &line, next_node, o, position,
                               fgetline, cookie))
          == FAIL) {
        free_cmd(result);
        return NULL;
      }
      assert(parse_start != line || ret == NOTDONE);

      block_command_node = *next_node;
      while (block_command_node != NULL
             && block_command_node->children != NULL
             && (CMDDEF(block_command_node->type).flags&ISMODIFIER))
        block_command_node = block_command_node->children;

      if (block_command_node != NULL)
        block_type = block_command_node->type;

      get_block_options(block_type, &bo);

      if (block_type == kCmdFunction)
        if (block_command_node->args[ARG_FUNC_ARGS].arg.ga_strs.ga_growsize == 0)
          memset(&bo, 0, sizeof(CommandBlockOptions));

      if (bo.find_in_stack != kCmdUnknown) {
        size_t initial_blockstack_len = blockstack_len;

        if (blockstack_len == 0) {
          free_cmd(*next_node);
          *next_node = NULL;
          APPEND_ERROR_NODE(next_node, prev_node, bo.no_start_message, line,
                            line_start)
        }
        while (blockstack_len) {
          CommandType last_block_type = blockstack[blockstack_len - 1].type;
          if (bo.not_after != kCmdUnknown && last_block_type == bo.not_after) {
            NEW_BLOCK_SEP_ERROR_NODE(next_node, blockstack, blockstack_len,
                                     prev_node, bo.not_after_message,
                                     bo.push_stack, line, line_start)
            break;
          } else if (bo.duplicate_message != NULL
                     && last_block_type == (*next_node)->type) {
            NEW_BLOCK_SEP_ERROR_NODE(next_node, blockstack, blockstack_len,
                                     prev_node, bo.duplicate_message,
                                     bo.push_stack, line, line_start)
            break;
          } else if (last_block_type == bo.find_in_stack
                     || (bo.find_in_stack_2 != kCmdUnknown
                         && last_block_type == bo.find_in_stack_2)
                     || (bo.find_in_stack_3 != kCmdUnknown
                         && last_block_type == bo.find_in_stack_3)) {
            CommandNode *new_node = *next_node;
            if (prev_node == NULL) {
              assert(blockstack[initial_blockstack_len - 1].block_node->children
                     == new_node);
              blockstack[initial_blockstack_len - 1].block_node->children
                  = NULL;
            } else {
              assert(prev_node->next == new_node);
              prev_node->next = NULL;
            }
            new_node->prev = blockstack[blockstack_len - 1].node;
            blockstack[blockstack_len - 1].node->next = new_node;
            if (bo.push_stack) {
              blockstack[blockstack_len - 1].node = new_node;
              blockstack[blockstack_len - 1].type = block_type;
              blockstack[blockstack_len - 1].block_node = block_command_node;
              next_node = &(block_command_node->children);
              prev_node = NULL;
              break;
            } else {
              prev_node = new_node;
              next_node = &(new_node->next);
              blockstack_len--;
              break;
            }
          } else {
            char *missing_message =
                get_missing_message(blockstack[blockstack_len - 1].type);
            NEW_ERROR_NODE(blockstack[blockstack_len - 1].node,
                           missing_message, line, line_start)
          }
          blockstack_len--;
          if (blockstack_len == 0) {
            free_cmd(*next_node);
            *next_node = NULL;
            prev_node = blockstack[0].node;
            while (prev_node->next != NULL)
              prev_node = prev_node->next;
            APPEND_ERROR_NODE(next_node, prev_node, bo.no_start_message, line,
                              line_start)
            break;
          }
        }
      } else if ((*next_node) != NULL && (*next_node) != &nocmd) {
        (*next_node)->prev = prev_node;
        if (bo.push_stack) {
          blockstack_len++;
          if (blockstack_len >= MAX_NEST_BLOCKS) {
            CommandParserError error;
            // FIXME Make message with error code
            error.message = N_("too many nested blocks");
            error.position = line;
            if (create_error_node(next_node, &error, position, line_start)
                == FAIL) {
              free_cmd(result);
              free(line_start);
              return FAIL;
            }
          }
          blockstack[blockstack_len - 1].node = *next_node;
          blockstack[blockstack_len - 1].type = block_type;
          blockstack[blockstack_len - 1].block_node = block_command_node;
          next_node = &(block_command_node->children);
          prev_node = NULL;
        } else {
          prev_node = *next_node;
          next_node = &((*next_node)->next);
        }
      }
      if (ret == NOTDONE)
        break;
      line = skipwhite(line);
      if (*line == '|' || *line == '\n')
        line++;
    }
    position.lnr++;
    if (can_free) {
      free(line_start);
    }
    if (blockstack_len == 0 && o.early_return)
      break;
  }

  while (blockstack_len) {
    char *missing_message =
        get_missing_message(blockstack[blockstack_len - 1].type);
    char *empty = "";
    NEW_ERROR_NODE(blockstack[blockstack_len - 1].node,
                   missing_message, empty, empty)
    blockstack_len--;
  }

  return result;
}

/// Fgetline implementation that calls another fgetline and saves the result
static char *saving_fgetline(int c, SavingFgetlineArgs *args, int indent)
{
  char *line = args->fgetline(c, args->cookie, indent);
  if (line != NULL) {
    GA_APPEND(char *, &(args->ga), line);
  }
  return line;
}

void free_parser_result(ParserResult *pres)
{
  if (pres == NULL) {
    return;
  }
  free_cmd(pres->node);
  for (size_t i = 0; i < pres->lines_size; i++) {
    free(pres->lines[i]);
  }
  free(pres->lines);
  free(pres->fname);
  free(pres);
}

/// Return a pair (AST, lines that were parsed)
///
/// Parsed lines are supposed to be used for implementing `:function Func` 
/// introspection and error reporting.
///
/// @param[in]  o         Parser options.
/// @param[in]  fname     Path to the parsed file or a string enclosed in `<` to 
///                       indicate that current input is not from any file.
/// @param[in]  fgetline  Function used to obtain the next line.
///
///                       @par
///                       This function should return NULL when there are no 
///                       more lines.
///
///                       @note This function must return string in allocated 
///                             memory. Only parser thread must have access to 
///                             strings returned by fgetline.
/// @param      cookie    Second argument to the above function.
ParserResult *parse_string(CommandParserOptions o, const char *fname,
                           VimlLineGetter fgetline, void *cookie)
  FUNC_ATTR_NONNULL_ALL
{
  ParserResult *ret = XCALLOC_NEW(ParserResult, 1);
  SavingFgetlineArgs fgargs = {
    .fgetline = fgetline,
    .cookie = cookie
  };
  CommandPosition position = {
    .lnr = 1,
    .col = 1
  };
  ga_init(&(fgargs.ga), (int) sizeof(char *), 16);
  ret->node = parse_cmd_sequence(o, position, (VimlLineGetter) &saving_fgetline,
                                 &fgargs, false);
  ret->lines = (char **) fgargs.ga.ga_data;
  ret->lines_size = (size_t) fgargs.ga.ga_len;
  ret->fname = xstrdup(fname);
  if (ret->node == NULL) {
    free_parser_result(ret);
    return NULL;
  }
  return ret;
}
