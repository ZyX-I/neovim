// vim: ts=8 sts=2 sw=2 tw=80

//
// Copyright 2014 Nikolay Pavlov

// ex_commands.c: Ex commands parsing

#include <stddef.h>
#include <stdbool.h>

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

#include "nvim/viml/parser/expressions.h"
#include "nvim/viml/parser/ex_commands.h"

#define vim_strsave(arg) vim_strsave((char_u *) (arg))
#define vim_strnsave(a1, a2) vim_strnsave((char_u *) (a1), a2)
#define getdigits(arg) getdigits((char_u **) (arg))
#define skipwhite(arg) skipwhite((char_u *) (arg))
#define skipdigits(arg) skipdigits((char_u *) (arg))
#define mb_ptr_adv_(p) p += has_mbyte ? (*mb_ptr2len)((char_u *) p) : 1
#define enc_canonize(p) enc_canonize((char_u *) (p))
#define check_ff_value(p) check_ff_value((char_u *) (p))

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
{
  // XXX May allocate less space then needed to hold the whole struct: less by 
  // one size of CommandArg.
  size_t size = offsetof(CommandNode, args);
  CommandNode *node;

  if (type != kCmdUnknown)
    size += sizeof(CommandArg) * CMDDEF(type).num_args;

  node = (CommandNode *) xcalloc(1, size);
  char_u *fname;
  if ((fname = vim_strsave(position.fname)) == NULL) {
    free(node);
    return NULL;
  }
  node->type = type;
  node->position = position;
  node->position.fname = fname;

  return node;
}

/// Allocate new regex definition and assign all its properties
///
/// @param[in]  string  Regular expression string.
/// @param[in]  len     String length. It is not required for string[len] to be 
///                     a NUL byte.
///
/// @return Pointer to allocated block of memory.
static Regex *regex_alloc(const char_u *string, size_t len)
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

/// Allocate new glob pattern part
///
/// @param[in]  type  Part type.
///
/// @return Pointer to allocated block of memory.
static Glob *glob_alloc(GlobType type)
{
  Glob *glob;

  glob = XCALLOC_NEW(Glob, 1);

  glob->type = type;

  return glob;
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

static void free_glob(Glob *glob)
{
  if (glob == NULL)
    return;

  switch (glob->type) {
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
      free(glob->data.str);
      break;
    }
    case kGlobExpression: {
      free_expr(glob->data.expr);
      break;
    }
    case kPatBranch: {
      free_glob(glob->data.glob);
      break;
    }
  }
  free_glob(glob->next);
  free(glob);
}

static void free_pattern(Pattern *pat)
{
  free_glob((Glob *) pat);
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

static void free_address(Address *address)
{
  if (address == NULL)
    return;

  free_address_data(address);
  free(address);
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
    case kArgAuEvent:
    case kArgChar:
    case kArgColumn: {
      break;
    }
    case kArgNumbers: {
      free(arg->arg.numbers);
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
      break;
    }
    case kArgRegex: {
      free_regex(arg->arg.reg);
      break;
    }
    case kArgReplacement: {
      // FIXME
      break;
    }
    case kArgLines:
    case kArgStrings: {
      ga_clear_strings(&(arg->arg.strs));
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
      free_address(arg->arg.address);
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
  free_glob(node->glob);
  free_range_data(&(node->range));
  free(node->expr);
  free(node->name);
  free(node);
}

static int get_glob(const char_u **pp, CommandParserError *error, Glob **glob,
                    char_u **expr, bool is_branch, bool is_glob)
{
  const char_u *p = *pp;
  Glob **next = glob;
  const char_u *real_expr_start;
  const char_u *e;
  size_t literal_length = 0;
  const char_u *literal_start = NULL;
  int ret = FAIL;

  *glob = NULL;
  if (!is_branch)
    *expr = NULL;

  for (;;) {
    GlobType type = kPatMissing;
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
      }
      case '$': {
        type = kPatHome;
        break;
      }
      case NUL: {
        type = kPatMissing;
        break;
      }
      case ',': {
        if (is_branch)
          type = kPatMissing;
        else
          type = kPatLiteral;
        break;
      }
      case ' ':
      case '\t': {
        type = kPatMissing;
        break;
      }
      case '~': {
        if (p == *pp)
          type = kPatHome;
        else
          type = kPatLiteral;
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
#define GLOB_SPECIAL_CHARS ((char_u *) "`#*?%\\[{}]$")
#define IS_GLOB_SPECIAL(c) (vim_strchr(GLOB_SPECIAL_CHARS, c) != NULL)
      // TODO Compare with vim
      if (*p == '\\' && IS_GLOB_SPECIAL(p[1]))
        p += 2;
      else
        p++;
    } else {
      if (literal_start != NULL) {
        char_u *s;
        const char_u *t;
        *next = glob_alloc(kPatLiteral);
        (*next)->data.str = XCALLOC_NEW(char_u, literal_length + 1);
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
      *next = glob_alloc(type);
      switch (type) {
        case kGlobExpression: {
          ExpressionParserError expr_error;
          p += 2;
          if (*expr == NULL) {
            if ((*expr = vim_strsave(p)) == NULL)
              goto get_glob_error_return;
            real_expr_start = p;
            e = *expr;
          } else {
            e = *expr + (p - real_expr_start);
          }
          if (((*next)->data.expr = parse0_err(&e, &expr_error)) == NULL) {
            if (expr_error.message == NULL)
              goto get_glob_error_return;
            error->message = expr_error.message;
            error->position = real_expr_start + (expr_error.position - *expr);
            ret = NOTDONE;
            goto get_glob_error_return;
          }
          p = real_expr_start + (e - *expr);
          break;
        }
        case kGlobShell: {
          *next = glob_alloc(kGlobShell);
          if (((*next)->data.str = vim_strsave(p)) == NULL)
            goto get_glob_error_return;
          p += STRLEN(p);
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
          (*next)->data.number = getdigits(&p);
          break;
        }
        case kPatCollection: {
          p++;
          // FIXME
          break;
        }
        case kPatEnviron: {
          p++;
          // FIXME
          break;
        }
        case kPatBranch: {
          Glob **cnext = &((*next)->data.glob);
          do {
            int cret;
            p++;
            if ((cret = get_glob(&p, error, cnext, expr, true, is_glob))
                == FAIL)
              return FAIL;
            if (cret == NOTDONE) {
              ret = NOTDONE;
              goto get_glob_error_return;
            }
            cnext = &((*cnext)->next);
          } while (*p == ',');
          break;
        }
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
  free_glob(*glob);
  free(*expr);
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
/// @return FAIL when out of memory, OK otherwise.
static int create_error_node(CommandNode **node, CommandParserError *error,
                             CommandPosition position, const char_u *s)
{
  if (error->message != NULL) {
    char_u *line;
    char_u *message;

    if ((line = (char_u *) vim_strsave(s)) == NULL)
      return FAIL;
    if ((message = (char_u *) vim_strsave((char_u *) error->message))
        == NULL) {
      free(line);
      return FAIL;
    }
    if ((*node = cmd_alloc(kCmdSyntaxError, position)) == NULL) {
      free(line);
      free(message);
      return FAIL;
    }
    (*node)->args[ARG_ERROR_LINESTR].arg.str = line;
    (*node)->args[ARG_ERROR_MESSAGE].arg.str = message;
    (*node)->args[ARG_ERROR_OFFSET].arg.col = (colnr_T) (error->position - s);
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
static int get_vcol(const char_u **pp)
{
  int vcol = 0;
  const char_u *p = *pp;

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
///
/// @return FAIL when out of memory, OK otherwise.
static int get_regex(const char_u **pp, CommandParserError *error,
                     Regex **regex, const char_u endch)
{
  const char_u *p = *pp;
  const char_u *s = p;

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

  *regex = regex_alloc(s, p - s);

  if (*p != NUL)
    p++;

  *pp = p;

  return OK;
}

static int parse_append(const char_u **pp,
                        CommandNode *node,
                        CommandParserError *error,
                        CommandParserOptions o,
                        CommandPosition position,
                        LineGetter fgetline,
                        void *cookie)
{
  garray_T *strs = &(node->args[ARG_APPEND_LINES].arg.strs);
  char_u *next_line;
  const char_u *first_nonblank;
  int vcol = -1;
  int cur_vcol = -1;

  ga_init(strs, (int) sizeof(char_u *), 3);

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
    ga_grow(strs, 1);
#if 0
    if (ga_grow(strs, 1) == FAIL) {
      ga_clear_strings(strs);
      return FAIL;
    }
#endif
    ((char_u **)(strs->ga_data))[strs->ga_len++] = (char_u *) next_line;
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
/// @param[in]      expr     true if it is <expr>-type mapping.
/// @parblock
///   @note If this argument is always false then you do not need to care about 
///         rhs_idx + 1 and node->children.
/// @endparblock
/// @param[in]      o         Options that control parsing behavior.
/// @param[in]      position  Position of input.
///
/// @return FAIL when out of memory, OK otherwise.
static int set_node_rhs(const char_u *rhs, size_t rhs_idx, CommandNode *node,
                        bool special, bool expr, CommandParserOptions o,
                        CommandPosition position)
{
  char_u *rhs_buf;
  char_u *new_rhs;

  new_rhs = replace_termcodes(rhs, STRLEN(rhs), &rhs_buf, false, true, special,
                              FLAG_POC_TO_FLAG_CPO(o.flags));
  if (rhs_buf == NULL)
    return FAIL;

  if ((o.flags&FLAG_POC_ALTKEYMAP) && (o.flags&FLAG_POC_RL))
    lrswap(new_rhs);

  if (expr) {
    ExpressionParserError expr_error;
    ExpressionNode *expr = NULL;
    const char_u *rhs_end = new_rhs;

    expr_error.position = NULL;
    expr_error.message = NULL;

    if ((expr = parse0_err(&rhs_end, &expr_error)) == NULL) {
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
  }
  node->args[rhs_idx].arg.str = new_rhs;
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
static int do_parse_map(const char_u **pp,
                        CommandNode *node,
                        CommandParserError *error,
                        CommandParserOptions o,
                        CommandPosition position,
                        LineGetter fgetline,
                        void *cookie,
                        bool unmap)
{
  uint_least32_t map_flags = 0;
  const char_u *p = *pp;
  const char_u *lhs;
  const char_u *lhs_end;
  const char_u *rhs;
  char_u *lhs_buf;
  // do_backslash = (vim_strchr(p_cpo, CPO_BSLASH) == NULL);
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
    lhs = replace_termcodes(lhs, lhs_end - lhs, &lhs_buf, true, true,
                            map_flags&FLAG_MAP_SPECIAL,
                            FLAG_POC_TO_FLAG_CPO(o.flags));
    if (lhs_buf == NULL)
      return FAIL;
    node->args[ARG_MAP_LHS].arg.str = (char_u *) lhs;
  }

  if (*rhs != NUL) {
    assert(!unmap);
    if (STRICMP(rhs, "<nop>") == 0) {
      // Empty string
      rhs = XCALLOC_NEW(char_u, 1);
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

static int parse_map(const char_u **pp,
                     CommandNode *node,
                     CommandParserError *error,
                     CommandParserOptions o,
                     CommandPosition position,
                     LineGetter fgetline,
                     void *cookie)
{
  return do_parse_map(pp, node, error, o, position, fgetline, cookie, false);
}

static int parse_unmap(const char_u **pp,
                       CommandNode *node,
                       CommandParserError *error,
                       CommandParserOptions o,
                       CommandPosition position,
                       LineGetter fgetline,
                       void *cookie)
{
  return do_parse_map(pp, node, error, o, position, fgetline, cookie, true);
}

static int parse_mapclear(const char_u **pp,
                          CommandNode *node,
                          CommandParserError *error,
                          CommandParserOptions o,
                          CommandPosition position,
                          LineGetter fgetline,
                          void *cookie)
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
/// @param[in,out]  p  String being unescaped.
static void menu_unescape(char_u *p)
{
  while (*p) {
    if (*p == '\\' && p[1] != NUL)
      STRMOVE(p, p + 1);
    p++;
  }
}

static int parse_menu(const char_u **pp,
                      CommandNode *node,
                      CommandParserError *error,
                      CommandParserOptions o,
                      CommandPosition position,
                      LineGetter fgetline,
                      void *cookie)
{
  // FIXME "menu *" parses to something weird
  uint_least32_t menu_flags = 0;
  const char_u *p = *pp;
  size_t i;
  const char_u *s;
  const char_u *menu_path;
  const char_u *menu_path_end;
  const char_u *map_to;
  MenuItem *sub = NULL;
  MenuItem *cur = NULL;

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
    char_u *icon;

    p += 5;
    s = p;

    while (*p != NUL && *p != ' ') {
      if (*p == '\\')
        p++;
      mb_ptr_adv_(p);
    }

    if ((icon = vim_strnsave(s, p - s)) == NULL)
      return FAIL;

    menu_unescape(icon);

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
      pris = XCALLOC_NEW(int, i + 1);
      node->args[ARG_MENU_PRI].arg.numbers = pris;
      for (i = 0; i < MENUDEPTH && !vim_iswhite(*p); i++) {
        pris[i] = (int) getdigits(&p);
        if (pris[i] == 0)
          pris[i] = MENU_DEFAULT_PRI;
        if (*p == '.')
          p++;
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

  node->args[ARG_MENU_FLAGS].arg.flags = menu_flags;

  if (*p == NUL) {
    *pp = p;
    return OK;
  }

  menu_path = p;
  if (*menu_path == '.') {
    error->message = N_("E475: Expected menu name");
    error->position = menu_path;
    return NOTDONE;
  }

  menu_path_end = NULL;
  s = NULL;
  while (*p && !vim_iswhite(*p)) {
    if ((*p == '\\' || *p == Ctrl_V) && p[1] != NUL) {
      p++;
      if (*p == TAB) {
        s = p + 1;
        menu_path_end = p - 2;
      }
    } else if (STRNICMP(p, "<TAB>", 5) == 0) {
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
    } else if ((!p[1] || vim_iswhite(p[1])) && p != menu_path && s == NULL) {
      menu_path_end = p;
    }
    if (menu_path_end != NULL) {
      sub = XCALLOC_NEW(MenuItem, 1);

      if (node->args[ARG_MENU_NAME].arg.menu_item == NULL)
        node->args[ARG_MENU_NAME].arg.menu_item = sub;
      else
        cur->subitem = sub;

      if ((sub->name = vim_strnsave(menu_path, menu_path_end - menu_path + 1))
          == NULL)
        return FAIL;

      menu_unescape(sub->name);

      cur = sub;
      menu_path = p + 1;
      menu_path_end = NULL;
    }
    p++;
  }

  if (s != NULL) {
    char_u *text;
    if (node->args[ARG_MENU_NAME].arg.menu_item == NULL) {
      error->message = N_("E792: Empty menu name");
      error->position = s;
      return NOTDONE;
    }

    if ((text = vim_strnsave(s, p - s)) == NULL)
      return FAIL;

    menu_unescape(text);

    node->args[ARG_MENU_TEXT].arg.str = text;
  }

  p = skipwhite(p);

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

/// Parse an expression or a whitespace-separated sequence of expressions
///
/// Used for ":execute" and ":echo*"
///
/// @param[in,out]  pp     Parsed string. Is advanced to the end of the last 
///                        expression. Should point to the first character of 
///                        the expression (may point to whitespace character).
/// @param[out]     node   Location where parsing results are saved.
/// @param[out]     error  Structure where errors are saved.
/// @param[in]      o      Options that control parsing behavior.
/// @param[in]      multi  Determines whether parsed expression is actually 
///                        a sequence of expressions.
///
/// @return FAIL if out of memory, NOTDONE in case of error, OK otherwise.
static int do_parse_expr(const char_u **pp,
                         CommandNode *node,
                         CommandParserError *error,
                         CommandParserOptions o,
                         bool multi)
{
  ExpressionNode *expr;
  ExpressionParserError expr_error;
  const char_u *expr_str_start;
  const char_u *expr_str;

  if ((expr_str = vim_strsave(*pp)) == NULL)
    return FAIL;

  node->args[ARG_EXPR_STR].arg.str = (char_u *) expr_str;
  expr_str_start = expr_str;

  if (multi)
    expr = parse_mult(&expr_str, &expr_error, &parse0_err, false,
                      (char_u *) "\n|");
  else
    expr = parse0_err(&expr_str, &expr_error);

  if (expr == NULL) {
    if (expr_error.message == NULL)
      return FAIL;
    error->message = expr_error.message;
    error->position = *pp + (expr_error.position - expr_str_start);
    return NOTDONE;
  }

  node->args[ARG_EXPR_EXPR].arg.expr = expr;

  *pp += expr_str - expr_str_start;

  return OK;
}

static int parse_expr(const char_u **pp,
                      CommandNode *node,
                      CommandParserError *error,
                      CommandParserOptions o,
                      CommandPosition position,
                      LineGetter fgetline,
                      void *cookie)
{
  if (node->type == kCmdReturn && ENDS_EXCMD_NOCOMMENT(**pp))
    return OK;
  return do_parse_expr(pp, node, error, o, false);
}

static int parse_exprs(const char_u **pp,
                       CommandNode *node,
                       CommandParserError *error,
                       CommandParserOptions o,
                       CommandPosition position,
                       LineGetter fgetline,
                       void *cookie)
{
  if (ENDS_EXCMD_NOCOMMENT(**pp))
    return OK;
  return do_parse_expr(pp, node, error, o, true);
}

static int parse_rest_line(const char_u **pp,
                           CommandNode *node,
                           CommandParserError *error,
                           CommandParserOptions o,
                           CommandPosition position,
                           LineGetter fgetline,
                           void *cookie)
{
  size_t len;
  if (**pp == NUL) {
    error->message = (char *) e_argreq;
    error->position = *pp;
    return NOTDONE;
  }

  len = STRLEN(*pp);

  if ((node->args[0].arg.str = vim_strnsave(*pp, len)) == NULL)
    return FAIL;
  *pp += len;
  return OK;
}

static int parse_rest_allow_empty(const char_u **pp,
                                  CommandNode *node,
                                  CommandParserError *error,
                                  CommandParserOptions o,
                                  CommandPosition position,
                                  LineGetter fgetline,
                                  void *cookie)
{
  if (**pp == NUL)
    return OK;

  return parse_rest_line(pp, node, error, o, position, fgetline, cookie);
}

static const char_u *do_fgetline(int c, const char_u **arg, int indent)
{
  if (*arg) {
    const char_u *result;
    result = vim_strsave(*arg);
    *arg = NULL;
    return result;
  } else {
    return NULL;
  }
}

static int parse_do(const char_u **pp,
                    CommandNode *node,
                    CommandParserError *error,
                    CommandParserOptions o,
                    CommandPosition position,
                    LineGetter fgetline,
                    void *cookie)
{
  CommandNode *cmd;
  const char_u *arg = *pp;
  CommandPosition new_position = {
    1,
    1,
    (const char_u *) "<*do argument>",
  };

  if ((cmd = parse_cmd_sequence(o, new_position, (LineGetter) &do_fgetline,
                                &arg))
      == NULL)
    return FAIL;

  node->children = cmd;

  *pp += STRLEN(*pp);

  return OK;
}

/// Check whether given expression node is a valid lvalue
///
/// @param[in]   expr         Checked expression.
/// @param[out]  error        Structure where error information is saved.
/// @param[in]   allow_list   Determines whether list nodes are allowed.
/// @param[in]   allow_lower  Determines whether simple variable names are 
///                           allowed to start with a lowercase letter.
/// @param[in]   allow_env    Determines whether it is allowed to contain 
///                           kExprOption, kExprRegister and 
///                           kExprEnvironmentVariable nodes.
///
/// @return true if check failed, false otherwise.
static bool check_lval(ExpressionNode *expr, CommandParserError *error,
                       bool allow_list, bool allow_lower, bool allow_env)
{
  switch (expr->type) {
    case kExprSimpleVariableName: {
      if (!allow_lower
          && ASCII_ISLOWER(*expr->position)
          && expr->position[1] != ':'  // Fast check: most functions 
                                       // containing colon contain it in the 
                                       // second character
          && memchr((void *) expr->position, '#',
                    expr->end_position - expr->position + 1) == NULL
          // FIXME? Though foo:bar works in Vim Bram said it was never 
          //        intended to work.
          && memchr((void *) expr->position, ':',
                    expr->end_position - expr->position + 1) == NULL) {
        error->message =
            N_("E128: Function name must start with a capital "
               "or contain a colon or a hash");
        error->position = expr->position;
        return true;
      }
      break;
    }
    case kExprEnvironmentVariable: {
      if (allow_env) {
        if (expr->position > expr->end_position) {
          error->message = N_("E475: Cannot assign to environment variable "
                              "with an empty name");
          error->position = expr->end_position;
          return true;
        }
      }
      // fallthrough
    }
    case kExprOption:
    case kExprRegister: {
      if (!allow_env) {
        error->message = N_("E15: Only variable names are allowed");
        error->position = expr->position;
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
      ExpressionNode *root = expr;
      while (root->children != NULL)
        root = root->children;

      if (root->type != kExprVariableName
          && root->type != kExprSimpleVariableName) {
        error->message =
            N_("E475: Expected variable name or a list of variable names");
        error->position = root->position;
        return true;
      }
      break;
    }
    case kExprList: {
      if (allow_list) {
        ExpressionNode *item = expr->children;

        if (item == NULL) {
          error->message =
              N_("E475: Expected non-empty list of variable names");
          error->position = expr->position;
          return true;
        }

        while (item != NULL) {
          if (check_lval(item, error, false, allow_lower, allow_env))
            return true;
          item = item->next;
        }
      } else {
        error->message = N_("E475: Expected variable name");
        error->position = expr->position;
        return true;
      }
      break;
    }
    default: {
      ExpressionNode *root = expr;
      while (root->children != NULL && root->position == NULL)
        root = root->children;

      if (allow_list)
        error->message =
            N_("E475: Expected variable name or a list of variable names");
      else
        error->message = N_("E475: Expected variable name");
      error->position = root->position;
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
static int parse_lval(const char_u **pp,
                      CommandParserError *error,
                      CommandParserOptions o,
                      ExpressionNode **expr,
                      int flags)
{
  ExpressionParserError expr_error;
  ExpressionNode *next;
  const char_u *expr_start = *pp;
  bool allow_list = (bool) (flags&FLAG_PLVAL_LISTMULT);
  bool allow_env = (bool) (flags&FLAG_PLVAL_ALLOW_ENV);

  if (flags&FLAG_PLVAL_SPACEMULT)
    *expr = parse_mult(pp, &expr_error, &parse7_nofunc, true,
                       (char_u *) "\n\"|-.+=");
  else
    *expr = parse7_nofunc(pp, &expr_error);

  if (*expr == NULL) {
    if (expr_error.message == NULL)
      return FAIL;
    error->message = expr_error.message;
    error->position = expr_error.position;
    return NOTDONE;
  }

  if ((*expr)->next != NULL) {
    allow_list = false;
    allow_env = false;
  }

  next = *expr;
  while (next != NULL) {
    if (check_lval(next, error, allow_list, !(flags&FLAG_PLVAL_NOLOWER),
                   allow_env)) {
      free_expr(*expr);
      *expr = NULL;
      if (error->message == NULL)
        return FAIL;
      if (error->position == NULL)
        error->position = expr_start;
      return NOTDONE;
    }
    next = next->next;
  }

  return OK;
}

static int parse_lvals(const char_u **pp,
                       CommandNode *node,
                       CommandParserError *error,
                       CommandParserOptions o,
                       CommandPosition position,
                       LineGetter fgetline,
                       void *cookie)
{
  ExpressionNode *expr;
  const char_u *expr_str;
  const char_u *expr_str_start;
  int ret;

  if ((expr_str = vim_strsave(*pp)) == NULL)
    return FAIL;

  node->args[ARG_EXPRS_STR].arg.str = (char_u *) expr_str;

  expr_str_start = expr_str;

  if ((ret = parse_lval(&expr_str, error, o, &expr,
                        node->type == kCmdDelfunction
                        ? FLAG_PLVAL_NOLOWER
                        : FLAG_PLVAL_SPACEMULT)) == FAIL)
    return FAIL;

  node->args[ARG_EXPRS_EXPRS].arg.expr = expr;

  if (ret == NOTDONE) {
    error->position = *pp + (((const char_u *) error->position)
                             - expr_str_start);
    return NOTDONE;
  }

  node->args[ARG_EXPRS_EXPRS].arg.expr = expr;

  *pp += expr_str - expr_str_start;

  return OK;
}

static int parse_lockvar(const char_u **pp,
                         CommandNode *node,
                         CommandParserError *error,
                         CommandParserOptions o,
                         CommandPosition position,
                         LineGetter fgetline,
                         void *cookie)
{
  if (VIM_ISDIGIT(**pp))
    node->args[ARG_LOCKVAR_DEPTH].arg.unumber = (unsigned) getdigits(pp);

  return parse_lvals(pp, node, error, o, position, fgetline, cookie);
}

static int parse_for(const char_u **pp,
                     CommandNode *node,
                     CommandParserError *error,
                     CommandParserOptions o,
                     CommandPosition position,
                     LineGetter fgetline,
                     void *cookie)
{
  ExpressionNode *expr;
  ExpressionNode *list_expr;
  ExpressionParserError expr_error;
  const char_u *expr_str;
  const char_u *expr_str_start;
  int ret;

  if ((expr_str = vim_strsave(*pp)) == NULL)
    return FAIL;

  node->args[ARG_FOR_STR].arg.str = (char_u *) expr_str;

  expr_str_start = expr_str;

  if ((ret = parse_lval(&expr_str, error, o, &expr, FLAG_PLVAL_LISTMULT))
      == FAIL)
    return FAIL;

  node->args[ARG_FOR_LHS].arg.expr = expr;

  if (ret == NOTDONE) {
    error->position = *pp + (((const char_u *) error->position)
                             - expr_str_start);
    return NOTDONE;
  }

  expr_str = skipwhite(expr_str);

  if (expr_str[0] != 'i' || expr_str[1] != 'n') {
    error->message = N_("E690: Missing \"in\" after :for");
    error->position = *pp + (expr_str - expr_str_start);
    return NOTDONE;
  }

  expr_str = skipwhite(expr_str + 2);

  if ((list_expr = parse0_err(&expr_str, &expr_error)) == NULL) {
    if (expr_error.message == NULL)
      return FAIL;
    error->message = expr_error.message;
    error->position = *pp + (((const char_u *) expr_error.position)
                             - expr_str_start);
    return NOTDONE;
  }

  node->args[ARG_FOR_RHS].arg.expr = list_expr;

  *pp += expr_str - expr_str_start;

  return OK;
}

static int parse_function(const char_u **pp,
                          CommandNode *node,
                          CommandParserError *error,
                          CommandParserOptions o,
                          CommandPosition position,
                          LineGetter fgetline,
                          void *cookie)
{
  const char_u *p = *pp;
  ExpressionNode *expr;
  int ret;
  const char_u *expr_str;
  const char_u *expr_str_start;
  garray_T *args = &(node->args[ARG_FUNC_ARGS].arg.strs);
  uint_least32_t flags = 0;
  bool mustend = false;

  if (ENDS_EXCMD(*p))
    return OK;

  if (*p == '/')
    return get_regex(&p, error, &(node->args[ARG_FUNC_REG].arg.reg), '/');

  if ((expr_str = vim_strsave(*pp)) == NULL)
    return FAIL;

  node->args[ARG_FUNC_STR].arg.str = (char_u *) expr_str;

  expr_str_start = expr_str;

  if ((ret = parse_lval(&expr_str, error, o, &expr, FLAG_PLVAL_NOLOWER))
      == FAIL)
    return FAIL;

  node->args[ARG_FUNC_NAME].arg.expr = expr;

  if (ret == NOTDONE) {
    error->position = *pp + (((const char_u *) error->position)
                             - expr_str_start);
    return NOTDONE;
  }

  p = skipwhite(p + (expr_str - expr_str_start));

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

  ga_init(args, (int) sizeof(char_u *), 3);

  while (*p != ')') {
    char *notend_message = N_("E475: Expected end of arguments list");
    if (p[0] == '.' && p[1] == '.' && p[2] == '.') {
      flags |= FLAG_FUNC_VARARGS;
      p += 3;
      mustend = true;
    } else {
      const char_u *arg_start = p;
      const char_u *arg;
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

      if ((arg = vim_strnsave(arg_start, p - arg_start)) == NULL)
        return FAIL;

      for (i = 0; i < args->ga_len; i++)
        if (STRCMP(((char_u **)(args->ga_data))[i], arg) == 0) {
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

      ((char_u **)(args->ga_data))[args->ga_len++] = (char_u *) arg;
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

static int parse_let(const char_u **pp,
                     CommandNode *node,
                     CommandParserError *error,
                     CommandParserOptions o,
                     CommandPosition position,
                     LineGetter fgetline,
                     void *cookie)
{
  ExpressionNode *expr;
  ExpressionParserError expr_error;
  int ret;
  const char_u *expr_str;
  const char_u *expr_str_start;
  ExpressionNode *rval_expr;
  LetAssignmentType ass_type = 0;

  if (ENDS_EXCMD(**pp))
    return OK;

  if ((expr_str = vim_strsave(*pp)) == NULL)
    return FAIL;

  node->args[ARG_LET_STR].arg.str = (char_u *) expr_str;

  expr_str_start = expr_str;

  if ((ret = parse_lval(&expr_str, error, o, &expr,
                        FLAG_PLVAL_LISTMULT|FLAG_PLVAL_SPACEMULT
                        |FLAG_PLVAL_ALLOW_ENV))
      == FAIL)
    return FAIL;

  node->args[ARG_LET_LHS].arg.expr = expr;

  if (ret == NOTDONE) {
    error->position = *pp + (((const char_u *) error->position)
                             - expr_str_start);
    return NOTDONE;
  }

  expr_str = skipwhite(expr_str);

  if (ENDS_EXCMD(*expr_str)) {
    if (expr->type == kExprList) {
      error->message =
          N_("E474: To list multiple variables use \":let var|let var2\", "
                   "not \":let [var, var2]\"");
      error->position = *pp + (expr->position - expr_str_start);
      return NOTDONE;
    }
    *pp += expr_str - expr_str_start;
    return OK;
  } else {
    if (expr->next != NULL) {
      error->message = N_("E18: Expected end of command after last variable");
      error->position = *pp + (expr_str - expr_str_start);
      return NOTDONE;
    }
  }

  switch (*expr_str) {
    case '=': {
      expr_str++;
      ass_type = VAL_LET_ASSIGN;
      break;
    }
    case '+':
    case '-':
    case '.': {
      switch (*expr_str) {
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
      if (expr_str[1] != '=') {
        error->message = N_("E18: '+', '-' and '.' must be followed by '='");
        error->position = *pp + (expr_str + 1 - expr_str_start);
        return NOTDONE;
      }
      expr_str += 2;
      break;
    }
    default: {
      error->message =
          N_("E18: Expected assignment operation or end of command");
      error->position = *pp + (expr_str + 1 - expr_str_start);
      return NOTDONE;
    }
  }

  node->args[ARG_LET_ASS_TYPE].arg.flags = (uint_least32_t) ass_type;

  expr_str = skipwhite(expr_str);

  if ((rval_expr = parse0_err(&expr_str, &expr_error)) == NULL) {
    if (expr_error.message == NULL)
      return FAIL;
    error->message = expr_error.message;
    error->position = *pp + (((const char_u *) expr_error.position)
                             - expr_str_start);
    return NOTDONE;
  }

  node->args[ARG_LET_RHS].arg.expr = rval_expr;

  *pp += expr_str - expr_str_start;
  return OK;
}

static int parse_scriptencoding(const char_u **pp,
                                CommandNode *node,
                                CommandParserError *error,
                                CommandParserOptions o,
                                CommandPosition position,
                                LineGetter fgetline,
                                void *cookie)
{
  if (**pp == NUL)
    return OK;
  // TODO Setup conversion from parsed encoding
  if ((node->args[0].arg.str = enc_canonize(*pp)) == NULL)
    return FAIL;
  *pp += STRLEN(*pp);
  return OK;
}

/// Check for an Ex command with optional tail.
///
/// @param[in,out]  pp   Start of the command. Is advanced to the command 
///                      argument if requested command was found.
/// @param[in]      cmd  Name of the command which is checked for.
/// @param[in]      len  Minimal length required to accept a match.
///
/// @return true if requested command was found, false otherwise.
static int checkforcmd(const char_u **pp, const char_u *cmd, int len)
{
  int i;

  for (i = 0; cmd[i] != NUL; i++)
    if (((const char_u *)cmd)[i] != (*pp)[i])
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
static int get_address(const char_u **pp, Address *address,
                       CommandParserError *error)
{
  const char_u *p;

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
      const char_u c = *p;
      p++;
      if (c == '/')
        address->type = kAddrForwardSearch;
      else
        address->type = kAddrBackwardSearch;
      if (get_regex(&p, error, &(address->data.regex), c) == FAIL)
        return FAIL;
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

static int get_address_followups(const char_u **pp, CommandParserError *error,
                                 AddressFollowup **followup)
{
  AddressFollowup *fw;
  AddressFollowupType type = kAddressFollowupMissing;
  const char_u *p;

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
          fw->data.shift = sign * getdigits(&p);
        else
          fw->data.shift = sign;
        break;
      }
      case kAddressFollowupForwardPattern:
      case kAddressFollowupBackwardPattern: {
        if (get_regex(&p, error, &(fw->data.regex), p[-1]) == FAIL) {
          free_address_followup(fw);
          return FAIL;
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
static const char_u *check_nextcmd(const char_u *p)
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
static int find_command(const char_u **pp, CommandType *type,
                        const char_u **name, CommandParserError *error)
{
  size_t len;
  const char_u *p;
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
    if (p == *pp && vim_strchr((char_u *)"@*!=><&~#", *p) != NULL)
      p++;
    len = (size_t)(p - *pp);
    if (**pp == 'd' && (p[-1] == 'l' || p[-1] == 'p')) {
      // Check for ":dl", ":dell", etc. to ":deletel": that's
      // :delete with the 'l' flag.  Same for 'p'.
      for (i = 0; i < len; i++)
        if ((*pp)[i] != ((const char_u *)"delete")[i])
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
      if ((*name = vim_strnsave(*pp, p - *pp)) == NULL)
        return FAIL;
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
/// Not used for commands with relations with bar or comment symbol: e.g. :echo 
/// (it allows things like "echo 'abc|def'") or :write (":w `='abc|def'`")
///
/// @param[in]   type            Command type.
/// @param[in]   o               Parser options. See parse_one_cmd 
///                              documentation.
/// @param[in]   start           Start of the command-line arguments.
/// @param[out]  arg             Resulting command-line argument.
/// @param[out]  next_cmd_offset Offset of next command.
///
/// @return FAIL when out of memory, OK otherwise.
static int get_cmd_arg(CommandType type, CommandParserOptions o,
                       const char_u *start, char_u **arg,
                       size_t *next_cmd_offset)
{
  char_u *p;

  *next_cmd_offset = 0;

  if ((*arg = vim_strsave(start)) == NULL)
    return FAIL;

  p = *arg;

  for (; *p; mb_ptr_adv(p)) {
    if (*p == Ctrl_V) {
      if (CMDDEF(type).flags & (USECTRLV)) {
        p++;                // skip CTRL-V and next char
      } else {
        (*next_cmd_offset)++;
        STRMOVE(p, p + 1);  // remove CTRL-V and skip next char
      }
      if (*p == NUL)        // stop at NUL after CTRL-V
        break;
    // Check for '"': start of comment or '|': next command
    // :@" and :*" do not start a comment!
    // :redir @" doesn't either.
    } else if ((*p == '"' && !(CMDDEF(type).flags & NOTRLCOM)
                && ((type != kCmdAt && type != kCmdStar)
                    || p != *arg)
                && (type != kCmdRedir
                    || p != *arg + 1 || p[-1] != '@'))
               || *p == '|' || *p == '\n') {
      // We remove the '\' before the '|', unless USECTRLV is used
      // AND 'b' is present in 'cpoptions'.
      if (((o.flags & FLAG_POC_CPO_BAR)
           || !(CMDDEF(type).flags & USECTRLV)) && *(p - 1) == '\\') {
        *next_cmd_offset += 1;
        STRMOVE(p - 1, p);              // remove the '\'
        p--;
      } else {
        const char_u *nextcmd = check_nextcmd(p);
        if (nextcmd != NULL)
          *next_cmd_offset += (nextcmd - p);
        *p = NUL;
        break;
      }
    }
  }

  *next_cmd_offset += p - *arg;

  if (!(CMDDEF(type).flags & NOTRLCOM))  // remove trailing spaces
    del_trailing_spaces(p);

  return OK;
}

/// Fgetline implementation that simply returns given string
///
/// Useful for :execute, :*do, etc.
///
/// @param[in,out]  arg  Pointer to pointer to the start of string which will be 
///                      returned. This string must live in an allocated memory.
const char_u *do_fgetline_allocated(int c, const char_u **arg, int indent)
{
  if (*arg) {
    const char_u *saved_arg = *arg;
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
static int parse_argcmd(const char_u **pp,
                        CommandNode **next_node,
                        CommandParserOptions o,
                        CommandPosition position,
                        const char_u *s)
{
  const char_u *p = *pp;

  if (*p == '+') {
    p++;
    if (vim_isspace(*p) || !*p) {
      if ((*next_node = cmd_alloc(kCmdMissing, position)) == NULL)
        return FAIL;
      (*next_node)->range.address.type = kAddrEnd;
      (*next_node)->end_col = position.col + (p - s);
    } else {
      const char_u *cmd_start = p;
      char_u *arg;
      char_u *arg_start;
      CommandPosition new_position = {
        position.lnr,
        position.col + (p - s),
        position.fname,
      };

      while (*p && !vim_isspace(*p)) {
        if (*p == '\\' && p[1] != NUL)
          p++;
        mb_ptr_adv_(p);
      }

      if ((arg = vim_strnsave(cmd_start, p - cmd_start)) == NULL)
        return FAIL;

      arg_start = arg;

      while (*arg) {
        if (*arg == '\\' && arg[1] != NUL)
          STRMOVE(arg, arg + 1);
        mb_ptr_adv_(arg);
      }

      if ((*next_node = parse_cmd_sequence(o, new_position,
                                           (LineGetter) &do_fgetline_allocated,
                                           &arg_start))
          == NULL)
        return FAIL;

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
static int parse_argopt(const char_u **pp,
                        uint_least32_t *optflags,
                        char_u **enc,
                        CommandParserOptions o,
                        CommandParserError *error)
{
  bool do_ff = false;
  bool do_enc = false;
  bool do_bad = false;
  const char_u *arg_start;

  *pp += 2;
  if (STRNCMP(*pp, "bin", 3) == 0 || STRNCMP(*pp, "nobin", 5) == 0) {
    if (**pp == 'n') {
      *pp += 2;
      *optflags |= FLAG_OPT_BIN_USE_FLAG;
    } else {
      *optflags |= FLAG_OPT_BIN_USE_FLAG|FLAG_OPT_BIN;
    }
    if (!checkforcmd(pp, (const char_u *) "binary", 3)) {
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
    char_u *e;
    if ((*enc = vim_strnsave(arg_start, *pp - arg_start)) == NULL)
      return FAIL;
    for (e = *enc; *e != NUL; e++)
      *e = TOLOWER_ASC(*e);
  } else if (do_bad) {
    size_t len = *pp - arg_start;
    if (STRNICMP(arg_start, "keep", len) == 0) {
      *optflags |= VAL_OPT_BAD_KEEP;
    } else if (STRNICMP(arg_start, "drop", len) == 0) {
      *optflags |= VAL_OPT_BAD_DROP;
    } else if (MB_BYTE2LEN(*arg_start) == 1 && *pp == arg_start + 1) {
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

/// Parses command modifiers
///
/// @param[in,out]  pp        Command to parse.
/// @param[out]     node      Location where parsing results are saved.
/// @param[in]      o         Options that control parsing behavior.
/// @param[in]      position  Position of input.
/// @param[in]      pstart    Position of the first leading digit, if any. 
///                           Points to the same location pp does otherwise.
/// @param[out]     type      Resulting command type.
int parse_modifiers(const char_u **pp, CommandNode ***node,
                    CommandParserOptions o, CommandPosition position,
                    const char_u *const pstart, CommandType *type)
{
  const char_u *p = *pp;
  const char_u *mod_start = p;
  const char_u *s = p;

  // FIXME (genex_cmds.lua): Iterate over precomputed table with only modifier 
  //                         commands
  for (int i = cmdidxs[(int) (*p - 'a')]; *(CMDDEF(i).name) == *p; i++) {
    if (CMDDEF(i).flags & ISMODIFIER) {
      size_t common_len = 0;
      if (i > 0) {
        const char_u *name = CMDDEF(i).name;
        const char_u *prev_name = CMDDEF(i - 1).name;
        common_len++;
        // FIXME (genex_cmds.lua): Precompute and record this in cmddefs
        while (name[common_len] == prev_name[common_len])
          common_len++;
      }
      if (checkforcmd(&p, CMDDEF(i).name, common_len + 1)) {
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
    if ((**node = cmd_alloc(*type, position)) == NULL)
      return FAIL;
    if (VIM_ISDIGIT(*pstart)) {
      (**node)->cnt_type = kCntCount;
      (**node)->cnt.count = getdigits(&pstart);
    }
    if (*p == '!') {
      (**node)->bang = true;
      p++;
    }
    (**node)->position.col = position.col + (mod_start - s);
    (**node)->end_col = position.col + (p - s - 1);
    *node = &((**node)->children);
    *type = kCmdUnknown;
  } else {
    p = pstart;
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
///    FLAG_POC_EXMODE      | Is set if parser is called for Ex mode
///    FLAG_POC_CPO_STAR    | Is set if CPO_STAR flag is present in &cpo
///    FLAG_POC_CPO_SPECI   | Is set if CPO_SPECI flag is present in &cpo
///    FLAG_POC_CPO_KEYCODE | Is set if CPO_KEYCODE flag is present in &cpo
///    FLAG_POC_CPO_BAR     | Is set if CPO_BAR flag is present in &cpo
///    FLAG_POC_ALTKEYMAP   | Is set if &altkeymap option is set
///    FLAG_POC_RL          | Is set if &rl option is set
/// @endparblock
/// @param[in]      position  Position of input.
/// @param[in]      fgetline  Function used to obtain the next line.
/// @param[in,out]  cookie    Second argument to fgetline.
///
/// @return OK if everything was parsed correctly, FAIL if out of memory, 
///         NOTDONE for parser error.
int parse_one_cmd(const char_u **pp,
                  CommandNode **node,
                  CommandParserOptions o,
                  CommandPosition position,
                  LineGetter fgetline,
                  void *cookie)
{
  CommandNode **next_node = node;
  CommandNode *children = NULL;
  CommandParserError error;
  CommandType type = kCmdUnknown;
  Range range;
  Range current_range;
  const char_u *p;
  const char_u *s = *pp;
  const char_u *nextcmd = NULL;
  const char_u *name = NULL;
  const char_u *range_start = NULL;
  bool bang = false;
  uint_least8_t exflags = 0;
  uint_least32_t optflags = 0;
  char_u *enc = NULL;
  size_t len;
  CommandArgsParser parse = NULL;
  CountType cnt_type = kCntMissing;
  int count = 0;
  Glob *glob = NULL;
  char_u *expr = NULL;

  memset(&range, 0, sizeof(Range));
  memset(&current_range, 0, sizeof(Range));
  memset(&error, 0, sizeof(CommandParserError));

  if (((*pp)[0] == '#') &&
      ((*pp)[1] == '!') &&
      position.col == 1) {
    if ((*next_node = cmd_alloc(kCmdHashbangComment, position)) == NULL)
      return FAIL;
    p = *pp + 2;
    len = STRLEN(p);
    if (((*next_node)->args[0].arg.str = vim_strnsave(p, len)) == NULL)
      return FAIL;
    *pp = p + len;
    (*next_node)->end_col = position.col + (*pp - s);
    return OK;
  }

  p = *pp;
  for (;;) {
    const char_u *pstart;
    // 1. skip comment lines and leading white space and colons
    while (*p == ' ' || *p == TAB || *p == ':')
      p++;
    // in ex mode, an empty line works like :+ (switch to next line)
    if (*p == NUL && o.flags&FLAG_POC_EXMODE) {
      AddressFollowup *fw;
      if ((*next_node = cmd_alloc(kCmdMissing, position)) == NULL)
        return FAIL;
      (*next_node)->range.address.type = kAddrCurrent;
      fw = address_followup_alloc(kAddressFollowupShift);
      fw->data.shift = 1;
      (*next_node)->range.address.followups = fw;
      (*next_node)->end_col = position.col + (*pp - s);
      return OK;
    }

    if (*p == '"') {
      if ((*next_node = cmd_alloc(kCmdComment, position)) == NULL)
        return FAIL;
      p++;
      len = STRLEN(p);
      if (((*next_node)->args[0].arg.str = vim_strnsave(p, len)) == NULL)
        return FAIL;
      *pp = p + len;
      (*next_node)->end_col = position.col + (*pp - s);
      return OK;
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
      new_position.col += p - s;
      int ret = parse_modifiers(&p, &next_node, o, new_position, pstart,
                                &type);
      if (ret != OK)
        return ret;
      else if (p == pstart)
        break;
    } else {
      p = pstart;
      break;
    }
  }

  const char_u *modifiers_end = p;
  /*
   * 3. parse a range specifier of the form: addr [,addr] [;addr] ..
   *
   * where 'addr' is:
   *
   * %	      (entire file)
   * $  [+-NUM]
   * 'x [+-NUM] (where x denotes a currently defined mark)
   * .  [+-NUM]
   * [+-NUM]..
   * NUM
   *
   * The ea.cmd pointer is updated to point to the first character following the
   * range spec. If an initial address is found, but no second, the upper bound
   * is equal to the lower.
   */

  /* repeat for all ',' or ';' separated addresses */
  for (;;) {
    p = skipwhite(p);
    if (range_start == NULL)
      range_start = p;
    if (get_address(&p, &current_range.address, &error) == FAIL) {
      free_range_data(&current_range);
      if (error.message == NULL)
        return FAIL;
      if (create_error_node(next_node, &error, position, s) == FAIL)
        return FAIL;
      return NOTDONE;
    }
    if (get_address_followups(&p, &error, &current_range.address.followups)
        == FAIL) {
      free_range_data(&current_range);
      if (error.message == NULL)
        return FAIL;
      if (create_error_node(next_node, &error, position, s) == FAIL)
        return FAIL;
      return NOTDONE;
    }
    p = skipwhite(p);
    if (current_range.address.followups != NULL) {
      if (current_range.address.type == kAddrMissing)
        current_range.address.type = kAddrCurrent;
    } else if (range.address.type == kAddrMissing
               && current_range.address.type == kAddrMissing) {
      if (*p == '%') {
        current_range.address.type = kAddrFixed;
        current_range.address.data.lnr = 1;
        current_range.next = XCALLOC_NEW(Range, 1);
        current_range.next->address.type = kAddrEnd;
        range = current_range;
        p++;
        break;
      } else if (*p == '*' && !(o.flags&FLAG_POC_CPO_STAR)) {
        current_range.address.type = kAddrMark;
        current_range.address.data.mark = '<';
        current_range.next = XCALLOC_NEW(Range, 1);
        current_range.next->address.type = kAddrMark;
        current_range.next->address.data.mark = '>';
        range = current_range;
        p++;
        break;
      }
    }
    current_range.setpos = (*p == ';');
    if (range.address.type != kAddrMissing) {
      Range **target = (&(range.next));
      while (*target != NULL)
        target = &((*target)->next);
      *target = XCALLOC_NEW(Range, 1);
      **target = current_range;
    } else {
      range = current_range;
    }
    memset(&current_range, 0, sizeof(Range));
    if (*p == ';' || *p == ',')
      p++;
    else
      break;
  }

  // 4. parse command

  // Skip ':' and any white space
  while (*p == ' ' || *p == TAB || *p == ':')
    p++;

  /*
   * If we got a line, but no command, then go to the line.
   * If we find a '|' or '\n' we set ea.nextcmd.
   */
  if (*p == NUL || *p == '"' || (nextcmd = check_nextcmd(p)) != NULL) {
    /*
     * strange vi behaviour:
     * ":3"		jumps to line 3
     * ":3|..."	prints line 3
     * ":|"		prints current line
     */
    if (*p == '|' || (o.flags&FLAG_POC_EXMODE
                      && range.address.type != kAddrMissing)) {
      if (NODE_IS_ALLOCATED(*next_node))
        (*next_node)->end_col = position.col + (p - modifiers_end);
      if ((*next_node = cmd_alloc(kCmdPrint, position)) == NULL) {
        free_range_data(&range);
        return FAIL;
      }
      (*next_node)->range = range;
      p++;
      *pp = p;
      (*next_node)->end_col = position.col + (*pp - s);
      return OK;
    } else if (*p == '"') {
      free_range_data(&range);
      if (NODE_IS_ALLOCATED(*next_node))
        (*next_node)->end_col = position.col + (p - modifiers_end);
      if ((*next_node = cmd_alloc(kCmdComment, position)) == NULL)
        return FAIL;
      p++;
      len = STRLEN(p);
      if (((*next_node)->args[0].arg.str = vim_strnsave(p, len)) == NULL)
        return FAIL;
      *pp = p + len;
      (*next_node)->end_col = position.col + (*pp - s);
      return OK;
    } else {
      if (NODE_IS_ALLOCATED(*next_node))
        (*next_node)->end_col = position.col + (p - modifiers_end);
      if ((*next_node = cmd_alloc(kCmdMissing, position)) == NULL) {
        free_range_data(&range);
        return FAIL;
      }
      (*next_node)->range = range;

      *pp = (nextcmd == NULL
             ? p
             : nextcmd);
      (*next_node)->end_col = position.col + (*pp - s);
      return OK;
    }
  }

  if (find_command(&p, &type, &name, &error) == FAIL) {
    free_range_data(&range);
    if (error.message == NULL)
      return FAIL;
    if (create_error_node(next_node, &error, position, s) == FAIL)
      return FAIL;
    return NOTDONE;
  }

  // Here used to be :Ni! egg. It was removed

  if (*p == '!') {
    if (CMDDEF(type).flags & BANG) {
      p++;
      bang = true;
    } else {
      free_range_data(&range);
      error.message = (char *) e_nobang;
      error.position = p;
      if (create_error_node(next_node, &error, position, s) == FAIL)
        return FAIL;
      return NOTDONE;
    }
  }

  if (range.address.type != kAddrMissing) {
    if (!(CMDDEF(type).flags & RANGE)) {
      free_range_data(&range);
      error.message = (char *) e_norange;
      error.position = range_start;
      if (create_error_node(next_node, &error, position, s) == FAIL)
        return FAIL;
      return NOTDONE;
    }
  }

  // Skip to start of argument.
  // Don't do this for the ":!" command, because ":!! -l" needs the space.
  if (type != kCmdBang)
    p = skipwhite(p);

  if (CMDDEF(type).flags & ARGOPT) {
    while (p[0] == '+' && p[1] == '+') {
      int ret;
      if ((ret = parse_argopt(&p, &optflags, &enc, o, &error)) == FAIL)
        return FAIL;
      if (ret == NOTDONE) {
        free_range_data(&range);
        if (create_error_node(next_node, &error, position, s) == FAIL)
          return FAIL;
        return NOTDONE;
      }
    }
  }

  if (CMDDEF(type).flags & EDITCMD) {
    if (parse_argcmd(&p, &children, o, position, s) == FAIL)
      return FAIL;
  }

  if (CMDDEF(type).flags & COUNT) {
    if (VIM_ISDIGIT(*p)) {
      cnt_type = kCntCount;
      count = getdigits(&p);
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

  if (CMDDEF(type).flags & XFILE) {
    int ret;
    if ((ret = get_glob(&p, &error, &glob, &expr, false, true)) == FAIL) {
      free_range_data(&range);
      return FAIL;
    }
    if (ret == NOTDONE) {
      free_range_data(&range);
      if (create_error_node(next_node, &error, position, s) == FAIL)
        return FAIL;
      return NOTDONE;
    }
  }

  if (!(CMDDEF(type).flags & EXTRA)
      && *p != NUL
      && *p != '"'
      && (*p != '|' || !(CMDDEF(type).flags & TRLBAR))) {
    free_range_data(&range);
    free_glob(glob);
    error.message = (char *) e_trailing;
    error.position = p;
    if (create_error_node(next_node, &error, position, s) == FAIL)
      return FAIL;
    return NOTDONE;
  }

  if (NODE_IS_ALLOCATED(*next_node))
    (*next_node)->end_col = position.col + (p - modifiers_end);
  if ((*next_node = cmd_alloc(type, position)) == NULL) {
    free_range_data(&range);
    return FAIL;
  }
  (*next_node)->bang = bang;
  (*next_node)->range = range;
  (*next_node)->name = (char_u *) name;
  (*next_node)->exflags = exflags;
  (*next_node)->cnt_type = cnt_type;
  (*next_node)->cnt.count = count;
  (*next_node)->children = children;
  (*next_node)->optflags = optflags;
  (*next_node)->enc = enc;
  (*next_node)->glob = glob;
  (*next_node)->expr = expr;

  parse = CMDDEF(type).parse;

  if (parse != NULL) {
    int ret;
    bool used_get_cmd_arg = false;
    const char_u *cmd_arg = p;
    char_u *cmd_arg_start = (char_u *) p;
    size_t next_cmd_offset = 0;
    // XFILE commands may have bangs inside `=…`
    // ISGREP commands may have bangs inside patterns
    // ISEXPR commands may have bangs inside "" or as logical OR
    if (!(CMDDEF(type).flags & (XFILE|ISGREP|ISEXPR|LITERAL))) {
      used_get_cmd_arg = true;
      if (get_cmd_arg(type, o, p, &cmd_arg_start, &next_cmd_offset)
          == FAIL) {
        free_cmd(*next_node);
        *next_node = NULL;
        return FAIL;
      }
      cmd_arg = cmd_arg_start;
    }
    if ((ret = parse(&cmd_arg, *next_node, &error, o, position, fgetline,
                     cookie))
        == FAIL) {
      if (used_get_cmd_arg)
        free(cmd_arg_start);
      free_cmd(*next_node);
      *next_node = NULL;
      return FAIL;
    }
    if (ret == NOTDONE) {
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
            == FAIL)
          return FAIL;
        return NOTDONE;
      }
      free(cmd_arg_start);
      p += next_cmd_offset;
    } else {
      p = cmd_arg;
    }
    *pp = p;
    (*next_node)->end_col = position.col + (*pp - s);
    return ret;
  }

  *pp = p;
  (*next_node)->end_col = position.col + (*pp - s);
  return OK;
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
  kCmdMissing,
  NULL,
  NULL,
  NULL,
  NULL,
  {
    {
      kAddrMissing,
      {NULL},
      NULL
    },
    NULL,
    false
  },
  {
    0,
    0,
    NULL
  },
  0,
  kCntMissing,
  {0},
  0,
  0,
  NULL,
  NULL,
  NULL,
  false,
  {
    {{0}}
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
/// @param[in]      o         Options that control parsing behavior. In 
///                           addition to flags documented in parse_one_command 
///                           documentation it accepts o.early_return option 
///                           that makes in not call fgetline once there is 
///                           something to execute.
/// @param[in]      position  Position of input. Only position.fname is used.
/// @param[in]      fgetline  Function used to obtain the next line.
/// @parblock
///   This function should return NULL when there are no more lines.
///
///   @note This function must return string in allocated memory. Only parser 
///         thread must have access to strings returned by fgetline.
/// @endparblock
/// @param[in,out]  cookie    Second argument to fgetline.
CommandNode *parse_cmd_sequence(CommandParserOptions o,
                                CommandPosition position,
                                LineGetter fgetline,
                                void *cookie)
{
  char_u *line_start;
  char_u *line;
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
      const char_u *parse_start = line;
      CommandBlockOptions bo;
      int ret;
      CommandType block_type = kCmdMissing;
      CommandNode *block_command_node;

      position.col = line - line_start + 1;
      assert(!NODE_IS_ALLOCATED(*next_node));
      if ((ret = parse_one_cmd((const char_u **) &line, next_node, o, position,
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
        if (block_command_node->args[ARG_FUNC_ARGS].arg.strs.ga_growsize == 0)
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
    free(line_start);
    if (blockstack_len == 0 && o.early_return)
      break;
  }

  while (blockstack_len) {
    char *missing_message =
        get_missing_message(blockstack[blockstack_len - 1].type);
    char_u *empty = (char_u *) "";
    NEW_ERROR_NODE(blockstack[blockstack_len - 1].node,
                   missing_message, empty, empty)
    blockstack_len--;
  }

  return result;
}
