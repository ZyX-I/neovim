// vim: ts=8 sts=2 sw=2 tw=80

//
// Copyright 2014 Nikolay Pavlov

// cmd.c: Ex commands parsing

#include "nvim/vim.h"
#include "nvim/types.h"
#include "nvim/cmd.h"
#include "nvim/expr.h"
#include "nvim/misc2.h"
#include "nvim/charset.h"
#include "nvim/globals.h"
#include "nvim/garray.h"
#include "nvim/term.h"
#include "nvim/main.h"

#ifdef INCLUDE_GENERATED_DECLARATIONS
# include "cmd.c.generated.h"
#endif

#include "nvim/cmd_def.h"
#define DO_DECLARE_EXCMD
#include "nvim/cmd_def.h"
#undef DO_DECLARE_EXCMD

static int get_vcol(char_u **pp)
  FUNC_ATTR_NONNULL_ALL FUNC_ATTR_WARN_UNUSED_RESULT
{
  int vcol = 0;
  char_u *p = *pp;

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

static int parse_append(char_u **pp, CommandNode *node, uint_least8_t flags,
                        CommandPosition *position, line_getter fgetline,
                        void *cookie)
  FUNC_ATTR_NONNULL_ALL FUNC_ATTR_WARN_UNUSED_RESULT
{
  garray_T *strs = &(node->args[ARG_APPEND_LINES].arg.strs);
  char_u *next_line;
  char_u *first_nonblank;
  int vcol = -1;
  int cur_vcol = -1;

  ga_init2(strs, (int) sizeof(char_u *), 3);

  while ((next_line = fgetline(':', cookie, vcol == -1 ? 0 : vcol)) != NULL) {
    first_nonblank = next_line;
    if (vcol == -1) {
      vcol = get_vcol(&first_nonblank);
    } else {
      cur_vcol = get_vcol(&first_nonblank);
    }
    if (first_nonblank[0] == '.' && first_nonblank[1] == NUL
        && cur_vcol <= vcol) {
      vim_free(next_line);
      break;
    }
    if (ga_grow(strs, 1) == FAIL) {
      ga_clear_strings(strs);
      return FAIL;
    }
    ((char_u **)(strs->ga_data))[strs->ga_len++] = next_line;
  }

  return OK;
}

static int parse_map(char_u **pp, CommandNode *node, uint_least8_t flags,
                     CommandPosition *position, line_getter fgetline,
                     void *cookie)
{
  uint_least32_t map_flags = 0;
  char_u *p = *pp;
  char_u *lhs;
  char_u *lhs_end;
  char_u *rhs;
  char_u *lhs_buf;
  char_u *rhs_buf;
  // do_backslash = (vim_strchr(p_cpo, CPO_BSLASH) == NULL);
  bool do_backslash = !(flags&FLAG_POC_CPO_BSLASH);

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
  while (*p && !vim_iswhite(*p)) {
    if ((p[0] == Ctrl_V || (do_backslash && p[0] == '\\')) &&
        p[1] != NUL)
      p++;                      // skip CTRL-V or backslash
    p++;
  }

  lhs_end = p;
  p = skipwhite(p);
  rhs = p;

  while (*p) {
    if ((p[0] == Ctrl_V || (do_backslash && p[0] == '\\')) &&
        p[1] != NUL)
      p++;                      // skip CTRL-V or backslash
    p++;
  }

  if (*lhs != NUL) {
    char_u saved = *lhs_end;
    *lhs_end = NUL;
    lhs = replace_termcodes(lhs, &lhs_buf, TRUE, TRUE,
                            map_flags&FLAG_MAP_SPECIAL,
                            FLAG_POC_TO_FLAG_CPO(flags));
    *lhs_end = saved;
    if (lhs_buf == NULL)
      return FAIL;
#if 0
    lhs = vim_strnsave(lhs, (lhs_end - lhs));
#endif
    node->args[ARG_MAP_LHS].arg.str = lhs;
  }

  if (*rhs != NUL) {
    if (STRICMP(rhs, "<nop>") == 0) {
      // Empty string
      rhs = ALLOC_CLEAR_NEW(char_u, 1);
    } else {
      rhs = replace_termcodes(rhs, &rhs_buf, FALSE, TRUE,
                              map_flags&FLAG_MAP_SPECIAL,
                              FLAG_POC_TO_FLAG_CPO(flags));
      if (rhs_buf == NULL)
        return FAIL;
#if 0
      rhs = vim_strnsave(rhs, p - rhs);
#endif
      node->args[ARG_MAP_RHS].arg.str = rhs;
    }
  }

  if ((flags&FLAG_POC_ALTKEYMAP) && (flags&FLAG_POC_RL))
    lrswap(rhs);

  *pp = p;
  return OK;
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

static CommandNode *cmd_alloc(CommandType type)
  FUNC_ATTR_WARN_UNUSED_RESULT FUNC_ATTR_NONNULL_RET
{
  // XXX May allocate less space then needed to hold the whole struct: less by 
  // one size of CommandArg.
  size_t size = sizeof(CommandNode) - sizeof(CommandArg);
  CommandNode *node;

  if (type == kCmdUSER)
    size++;
  else if (type != kCmdUnknown)
    size += sizeof(CommandArg) * CMDDEF(type).num_args;

  if ((node = (CommandNode *) xcalloc(size, 1)) != NULL)
    node->type = type;

  return node;
}

static void free_menu_item(MenuItem *menu_item)
{
  if (menu_item == NULL)
    return;

  free_menu_item(menu_item->subitem);
  vim_free(menu_item->name);
}

static void free_regex(Regex *regex)
{
  // FIXME
  vim_free(regex);
  return;
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
}

static void free_address(Address *address)
{
  if (address == NULL)
    return;

  free_address_data(address);
  vim_free(address);
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
  vim_free(followup);
}

static void free_range_data(Range *range)
{
  if (range == NULL)
    return;

  free_address_data(&(range->address));
  free_address_followup(range->followups);
  free_range(range->next);
}

static void free_range(Range *range)
{
  if (range == NULL)
    return;

  free_range_data(range);
  vim_free(range);
}

static void free_cmd_arg(CommandArg *arg, CommandArgType type)
{
  switch (type) {
    case kArgCommand: {
      free_cmd(arg->arg.cmd);
      break;
    }
    case kArgExpression:
    case kArgExpressions:
    case kArgAssignLhs: {
      free_expr(arg->arg.expr);
      break;
    }
    case kArgFlags:
    case kArgNumber:
    case kArgAuEvent:
    case kArgChar: {
      break;
    }
    case kArgPosition: {
      vim_free(arg->arg.position.fname);
      break;
    }
    case kArgNumbers: {
      vim_free(arg->arg.numbers);
      break;
    }
    case kArgString: {
      vim_free(arg->arg.str);
      break;
    }
    case kArgPattern: {
      // FIXME
      break;
    }
    case kArgGlob: {
      // FIXME
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
      vim_free(arg->arg.events);
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
      vim_free(subargs);
      break;
    }
  }
}

void free_cmd(CommandNode *node)
{
  size_t numargs;
  size_t i;

  if (node == NULL)
    return;

  numargs = CMDDEF(node->type).num_args;

  if (node->type == kCmdUSER)
    free_cmd_arg(&(node->args[0]), kArgString);
  else if (node->type != kCmdUnknown)
    for (i = 0; i < numargs; i++)
      free_cmd_arg(&(node->args[i]), CMDDEF(node->type).arg_types[i]);

  free_range_data(&(node->range));
  vim_free(node->name);
  vim_free(node);
}

/*
 * Check for an Ex command with optional tail.
 * If there is a match advance "pp" to the argument and return TRUE.
 */
static int checkforcmd(char_u **pp, /* start of command */
                       char_u *cmd, /* name of command */
                       int    len)  /* required length */
  FUNC_ATTR_NONNULL_ALL FUNC_ATTR_WARN_UNUSED_RESULT
{
  int i;

  for (i = 0; cmd[i] != NUL; i++)
    if (((char_u *)cmd)[i] != (*pp)[i])
      break;

  if ((i >= len) && !isalpha((*pp)[i])) {
    *pp = skipwhite(*pp + i);
    return TRUE;
  }
  return FALSE;
}

static int get_pattern(char_u **pp, CommandParserError *error, Regex **target)
  FUNC_ATTR_NONNULL_ALL FUNC_ATTR_WARN_UNUSED_RESULT
{
  // FIXME compile regex
  char_u *p = *pp;
  char_u *s = p;
  char_u c = p[-1];

  while (*p != c && *p != NUL)
  {
    if (*p == '\\' && p[1] != NUL)
      p += 2;
    else
      p++;
  }

  if (*p != NUL)
    p++;

  if ((*target = vim_strnsave(s, p - s)) == NULL)
    return FAIL;

  *pp = p;

  return OK;
}

/*
 * get a single EX address
 *
 * Set ptr to the next character after the part that was interpreted.
 * Set ptr to NULL when an error is encountered.
 *
 * Return MAXLNUM when no Ex address was found.
 */
static int get_address(char_u **pp, Address *address, CommandParserError *error)
  FUNC_ATTR_NONNULL_ALL FUNC_ATTR_WARN_UNUSED_RESULT
{
  char_u *p;

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
      if (*p == NUL) {
        // FIXME proper message
        error->message = (char_u *)"expected mark name, but got nothing";
        error->position = p - 1;
        return FAIL;
      }
      address->data.mark = *p;
      p++;
      break;
    }
    case '/':
    case '?': {
      char_u c = *p;
      p++;
      if (c == '/')
        address->type = kAddrForwardSearch;
      else
        address->type = kAddrBackwardSearch;
      if (get_pattern(pp, error, &(address->data.regex)) == FAIL)
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
          error->message = e_backslash;
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

static AddressFollowup *address_followup_alloc(AddressFollowupType type)
  FUNC_ATTR_NONNULL_RET
{
  AddressFollowup *followup;

  followup = xcalloc(AddressFollowup, 1);
  followup->type = type;

  return followup;
}

static int get_address_followups(char_u **pp, CommandParserError *error,
                                 AddressFollowup **followup)
  FUNC_ATTR_NONNULL_ALL FUNC_ATTR_WARN_UNUSED_RESULT
{
  AddressFollowup *fw;
  AddressFollowupType type = kAddressFollowupMissing;
  char_u *p;

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
    if ((fw = address_followup_alloc(type)) == NULL)
      return FAIL;
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
        if (get_pattern(pp, error, &(fw->data.regex)) == FAIL) {
          free_address_followup(fw);
          return FAIL;
        }
        break;
      }
      default: {
        assert(FALSE);
      }
    }
    *followup = fw;
    if (get_address_followups(&p, error, &(fw->next)) == FAIL)
      return FAIL;
  }
  *pp = p;
  return OK;
}

static int create_error_node(CommandNode **node, CommandParserError *error,
                             CommandPosition *position, char_u *s)
  FUNC_ATTR_NONNULL_ALL FUNC_ATTR_WARN_UNUSED_RESULT
{
  if (error->message != NULL) {
    char_u *line;
    char_u *fname;
    char_u *message;

    if ((line = vim_strsave(s)) == NULL)
      return FAIL;
    if ((fname = vim_strsave(position->fname)) == NULL) {
      vim_free(line);
      return FAIL;
    }
    if ((message = vim_strsave(error->message)) == NULL) {
      vim_free(line);
      vim_free(fname);
      return FAIL;
    }
    if ((*node = cmd_alloc(kCmdSyntaxError)) == NULL) {
      vim_free(line);
      vim_free(fname);
      vim_free(message);
      return FAIL;
    }
    (*node)->args[ARG_ERROR_POSITION].arg.position = *position;
    (*node)->args[ARG_ERROR_POSITION].arg.position.fname = fname;
    (*node)->args[ARG_ERROR_LINESTR].arg.str = line;
    (*node)->args[ARG_ERROR_MESSAGE].arg.str = message;
    (*node)->args[ARG_ERROR_OFFSET].arg.flags =
        (uint_least32_t) (error->position - s);
  }
  return OK;
}

/*
 * Check if *p is a separator between Ex commands.
 * Return NULL if it isn't, (p + 1) if it is.
 */
static char_u *check_nextcmd(char_u *p)
  FUNC_ATTR_CONST
{
  p = skipwhite(p);
  if (*p == '|' || *p == '\n')
    return p + 1;
  else
    return NULL;
}

/*
 * Find an Ex command by its name, either built-in or user.
 * User command name can be found in *name. It is not resolved
 * Built in command name is not returned, use CMDDEF(type).name
 * Returns OK or FAIL, may set fields in CommandParserError in case of FAIL
 */
static int find_command(char_u **pp, CommandType *type, char_u **name,
                        CommandParserError *error)
  FUNC_ATTR_NONNULL_ALL FUNC_ATTR_WARN_UNUSED_RESULT
{
  size_t len;
  char_u *p;
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
    bool found = FALSE;

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
        if ((*pp)[i] != ((char_u *)"delete")[i])
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
        found = TRUE;
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
      error->message = (char_u *) N_("E492: Not an editor command");
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
/// @param[in]  type            Command type
/// @param[in]  flags           Flags. See parse_one_cmd documentation
/// @param[in]  start           Start of the command-line arguments
/// @param[out] arg             Resulting command-line argument
/// @param[out] next_cmd_offset Offset of next command
///
/// @return FAIL when out of memory, OK otherwise
static int get_cmd_arg(CommandType type, uint_least8_t flags, char_u *start,
                       char_u **arg, size_t *next_cmd_offset)
{
  char_u *p;

  *next_cmd_offset = 0;

  if ((*arg = vim_strsave(start)) == NULL)
    return FAIL;

  p = *arg;

  for (; *p; mb_ptr_adv(p)) {
    if (*p == Ctrl_V) {
      *next_cmd_offset += 2;
      if (CMDDEF(type).flags & (USECTRLV))
        p++;                // skip CTRL-V and next char
      else
        STRMOVE(p, p + 1);  // remove CTRL-V and skip next char
      if (*p == NUL)        // stop at NUL after CTRL-V
        break;
    }
    // Check for '"': start of comment or '|': next command
    // :@" and :*" do not start a comment!
    // :redir @" doesn't either.
    else if ((*p == '"' && !(CMDDEF(type).flags & NOTRLCOM)
              && ((type != kCmdAt && type != kCmdStar)
                  || p != *arg)
              && (type != kCmdRedir
                  || p != *arg + 1 || p[-1] != '@'))
             || *p == '|' || *p == '\n') {
      *next_cmd_offset += 1;
      // We remove the '\' before the '|', unless USECTRLV is used
      // AND 'b' is present in 'cpoptions'.
      if (((flags & FLAG_POC_CPO_BAR)
           || !(CMDDEF(type).flags & USECTRLV)) && *(p - 1) == '\\') {
        STRMOVE(p - 1, p);              // remove the '\'
        p--;
      } else {
        char_u *nextcmd = check_nextcmd(p);
        if (nextcmd != NULL) {
          *next_cmd_offset += (nextcmd - p);
        }
        *p = NUL;
        break;
      }
    }
  }

  if (!(CMDDEF(type).flags & NOTRLCOM)) // remove trailing spaces
    del_trailing_spaces(p);

  return OK;
}

/// Parses one command
///
/// @param[in,out] pp        Command to parse
/// @param[out]    node      Parsing result. Should be freed with free_cmd
/// @param[in]     flags     Flags that control parsing behavior
/// @parblock
///   Flag                 | Description
///   -------------------- | -------------------------------------------------
///   FLAG_POC_EXMODE      | Is set if parser is called for Ex mode
///   FLAG_POC_CPO_STAR    | Is set if CPO_STAR flag is present in &cpo
///   FLAG_POC_CPO_SPECI   | Is set if CPO_SPECI flag is present in &cpo
///   FLAG_POC_CPO_KEYCODE | Is set if CPO_KEYCODE flag is present in &cpo
///   FLAG_POC_CPO_BAR     | Is set if CPO_BAR flag is present in &cpo
///   FLAG_POC_ALTKEYMAP   | Is set if &altkeymap option is set
///   FLAG_POC_RL          | Is set if &rl option is set
/// @endparblock
/// @param[in]     fgetline  Function used to obtain the next line
/// @parblock
///   @note This function must return string in allocated memory. Only parser 
///         thread must have access to strings returned by fgetline.
/// @endparblock
/// @param[in,out] cookie    Second argument to fgetline
///
/// @return  OK if everything was parsed correctly, FAIL if out of memory, 
///          NOTDONE for parser error
int parse_one_cmd(char_u **pp,
                  CommandNode **node,
                  uint_least8_t flags,
                  CommandPosition *position,
                  line_getter fgetline,
                  void *cookie)
  FUNC_ATTR_NONNULL_ALL
{
  CommandNode **next_node = node;
  CommandParserError error;
  CommandType type = kCmdUnknown;
  Range range;
  Range current_range;
  char_u *p;
  char_u *s = *pp;
  char_u *nextcmd = NULL;
  char_u *name = NULL;
  char_u *range_start = NULL;
  bool bang = FALSE;
  uint_least8_t exflags = 0;
  size_t len;
  size_t i;
  args_parser parser = NULL;
  CountType cnt_type = kCntMissing;
  int count = 0;

  memset(&range, 0, sizeof(Range));
  memset(&current_range, 0, sizeof(Range));
  memset(&error, 0, sizeof(CommandParserError));

  if (((*pp)[0] == '#') &&
      ((*pp)[1] == '!')) {
    if ((*next_node = cmd_alloc(kCmdHashbangComment)) == NULL)
      return FAIL;
    p = *pp + 2;
    len = STRLEN(p);
    if (((*next_node)->args[0].arg.str = vim_strnsave(p, len)) == NULL)
      return FAIL;
    *pp = p + len;
    return OK;
  }

  p = *pp;
  for (;;) {
    char_u *pstart;
    // 1. skip comment lines and leading white space and colons
    while (*p == ' ' || *p == TAB || *p == ':')
      p++;
    // in ex mode, an empty line works like :+ (switch to next line)
    if (*p == NUL && flags&FLAG_POC_EXMODE) {
      AddressFollowup *fw;
      if ((*next_node = cmd_alloc(kCmdHashbangComment)) == NULL)
        return FAIL;
      (*next_node)->range.address.type = kAddrCurrent;
      if ((fw = address_followup_alloc(type)) == NULL) {
        free_cmd(*next_node);
        *next_node = NULL;
        return FAIL;
      }
      fw->type = kAddressFollowupShift;
      fw->data.shift = 1;
      (*next_node)->range.followups = fw;
      return OK;
    }

    if (*p == '"') {
      if ((*next_node = cmd_alloc(kCmdComment)) == NULL)
        return FAIL;
      p++;
      len = STRLEN(p);
      if (((*next_node)->args[0].arg.str = vim_strnsave(p, len)) == NULL)
        return FAIL;
      *pp = p + len;
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

      for (i = cmdidxs[(int) (*p - 'a')]; *(CMDDEF(i).name) == *p; i++) {
        if (CMDDEF(i).flags & ISMODIFIER) {
          size_t common_len = 0;
          if (i > 0) {
            char_u *name = CMDDEF(i).name;
            char_u *prev_name = CMDDEF(i - 1).name;
            common_len++;
            // FIXME: Precompute and record this in cmddefs
            while (name[common_len] == prev_name[common_len])
              common_len++;
          }
          if (checkforcmd(&p, CMDDEF(i).name, common_len + 1)) {
            type = (CommandType) i;
            break;
          }
        }
      }
      if (type != kCmdUnknown) {
        if (VIM_ISDIGIT(*pstart) && !(CMDDEF(type).flags) & COUNT) {
          error.message = e_norange;
          error.position = pstart;
          create_error_node(next_node, &error, position, s);
          return NOTDONE;
        }
        if (*p == '!' && !(CMDDEF(type).flags & BANG)) {
          error.message = e_nobang;
          error.position = p;
          create_error_node(next_node, &error, position, s);
          return NOTDONE;
        }
        if ((*next_node = cmd_alloc(type)) == NULL)
          return FAIL;
        if (VIM_ISDIGIT(*pstart)) {
          (*next_node)->cnt_type = kCntCount;
          (*next_node)->cnt.count = getdigits(&pstart);
        }
        if (*p == '!') {
          (*next_node)->bang = TRUE;
          p++;
        }
        next_node = &((*next_node)->args[0].arg.cmd);
        type = kCmdUnknown;
      } else {
        p = pstart;
        break;
      }
    } else {
      p = pstart;
      break;
    }
  }
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
      if (error.message == NULL)
        return FAIL;
      create_error_node(next_node, &error, position, s);
      free_range_data(&current_range);
      return NOTDONE;
    }
    if (get_address_followups(&p, &error, &current_range.followups) == FAIL) {
      if (error.message == NULL)
        return FAIL;
      create_error_node(next_node, &error, position, s);
      free_range_data(&current_range);
      return NOTDONE;
    }
    p = skipwhite(p);
    if (current_range.followups != NULL) {
      if (current_range.address.type == kAddrMissing)
        current_range.address.type = kAddrCurrent;
    } else if (range.address.type == kAddrMissing
               && current_range.address.type == kAddrMissing) {
      if (*p == '%') {
        current_range.address.type = kAddrFixed;
        current_range.address.data.lnr = 1;
        if ((current_range.next = ALLOC_CLEAR_NEW(Range, 1)) == NULL) {
          free_range_data(&range);
          free_range_data(&current_range);
          return FAIL;
        }
        current_range.next->address.type = kAddrEnd;
        range = current_range;
        p++;
        break;
      } else if (*p == '*' && !(flags&FLAG_POC_CPO_STAR)) {
        current_range.address.type = kAddrMark;
        current_range.address.data.mark = '<';
        if ((current_range.next = ALLOC_CLEAR_NEW(Range, 1)) == NULL) {
          free_range_data(&range);
          free_range_data(&current_range);
          return FAIL;
        }
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
      if ((*target = ALLOC_CLEAR_NEW(Range, 1)) == NULL) {
        free_range_data(&range);
        free_range_data(&current_range);
        return FAIL;
      }
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
    if (*p == '|' || (flags&FLAG_POC_EXMODE
                      && range.address.type != kAddrMissing)) {
      if ((*next_node = cmd_alloc(kCmdPrint)) == NULL) {
        free_range_data(&range);
        return FAIL;
      }
      (*next_node)->range = range;
      p++;
      *pp = p;
      return OK;
    } else if (*p == '"') {
      free_range_data(&range);
      if ((*next_node = cmd_alloc(kCmdComment)) == NULL)
        return FAIL;
      p++;
      len = STRLEN(p);
      if (((*next_node)->args[0].arg.str = vim_strnsave(p, len)) == NULL)
        return FAIL;
      *pp = p + len;
      return OK;
    } else {
      if ((*next_node = cmd_alloc(kCmdMissing)) == NULL) {
        free_range_data(&range);
        return FAIL;
      }
      (*next_node)->range = range;

      *pp = (nextcmd == NULL
             ? p
             : nextcmd);
      return OK;
    }
  }

  if (find_command(&p, &type, &name, &error) == FAIL) {
    if (error.message == NULL)
      return FAIL;
    free_range_data(&range);
    create_error_node(next_node, &error, position, s);
    return NOTDONE;
  }

  // Here used to be :Ni! egg. It was removed

  if (*p == '!') {
    if (type ==  kCmdUSER || CMDDEF(type).flags & BANG) {
      p++;
      bang = TRUE;
    } else {
      error.message = e_nobang;
      error.position = p;
      create_error_node(next_node, &error, position, s);
      return NOTDONE;
    }
  }

  if (range.address.type != kAddrMissing) {
    if (!(type == kCmdUSER || CMDDEF(type).flags & RANGE)) {
      error.message = e_norange;
      error.position = range_start;
      create_error_node(next_node, &error, position, s);
      return NOTDONE;
    }
  }

  // Skip to start of argument.
  // Don't do this for the ":!" command, because ":!! -l" needs the space.
  if (type != kCmdBang)
    p = skipwhite(p);

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

  if (!(CMDDEF(type).flags & EXTRA)
      && *p != NUL
      && *p != '"'
      && (*p != '|' || !(CMDDEF(type).flags & TRLBAR))) {
    error.message = e_trailing;
    error.position = p;
    create_error_node(next_node, &error, position, s);
    return NOTDONE;
  }

  if ((*next_node = cmd_alloc(type)) == NULL) {
    free_range_data(&range);
    return FAIL;
  }
  (*next_node)->bang = bang;
  (*next_node)->range = range;
  (*next_node)->name = name;
  (*next_node)->exflags = exflags;
  (*next_node)->cnt_type = cnt_type;
  (*next_node)->cnt.count = count;

  if (type == kCmdUSER) {
    len = STRLEN(p);
    if (((*next_node)->args[0].arg.str = vim_strnsave(p, len)) == NULL)
      return FAIL;
    *pp = p + len;
    return OK;
  } else {
    parser = CMDDEF(type).parse;

    if (parser != NULL) {
      int ret;
      bool used_get_cmd_arg = FALSE;
      char_u *cmd_arg = p;
      char_u *cmd_arg_start = p;
      size_t next_cmd_offset = 0;
      // XFILE commands may have bangs inside `=…`
      // ISGREP commands may have bangs inside patterns
      if (!(CMDDEF(type).flags & (XFILE|ISGREP))) {
        used_get_cmd_arg = TRUE;
        if (get_cmd_arg(type, flags, p, &cmd_arg_start, &next_cmd_offset)
            == FAIL) {
          free_cmd(*next_node);
          *next_node = NULL;
          return FAIL;
        }
        cmd_arg = cmd_arg_start;
      }
      if ((ret = parser(&cmd_arg, *next_node, flags, position, fgetline, cookie))
          == FAIL) {
        if (used_get_cmd_arg)
          vim_free(cmd_arg_start);
        free_cmd(*next_node);
        *next_node = NULL;
        return FAIL;
      }
      if (used_get_cmd_arg) {
        if (*cmd_arg != NUL) {
          free_cmd(*next_node);
          *next_node = NULL;
          error.message = e_trailing;
          error.position = p + (cmd_arg - cmd_arg_start);
          create_error_node(next_node, &error, position, s);
          return NOTDONE;
        }
        vim_free(cmd_arg_start);
        p += next_cmd_offset;
      } else {
        p = cmd_arg;
      }
      *pp = p;
      return ret;
    }
  }

  *pp = p;
  return OK;
}

static char_u *fgetline_test(int c, char_u **arg, int indent)
{
  size_t len = 0;
  char_u *result;

  if (**arg == '\0')
    return NULL;

  while ((*arg)[len] != '\n' && (*arg)[len] != '\0')
    len++;

  result = vim_strnsave(*arg, len);

  if ((*arg)[len] == '\0')
    *arg += len;
  else
    *arg += len + 1;

  return result;
}

static size_t regex_repr_len(Regex *regex)
{
  size_t len = 0;
  // FIXME
  return len;
}

static void regex_repr(Regex *regex, char **pp)
{
  // FIXME
  return;
}

static size_t number_repr_len(intmax_t number)
{
  size_t len = 1;
  intmax_t i = number;

  if (i < 0)
    i = -i;

  do {
    len++;
    i = i >> 4;
  } while (i);

  return len;
}

static size_t unumber_repr_len(uintmax_t number)
{
  size_t len = 0;
  uintmax_t i = number;

  do {
    len++;
    i = i >> 4;
  } while (i);

  return len;
}

static void number_repr(intmax_t number, char **pp)
{
  char *p = *pp;
  size_t i = number_repr_len(number) - 1;
  intmax_t num = number;

  if (num < 0)
    num = -num;

  *p++ = (number >= 0 ? '+' : '-');

  do {
    intmax_t digit = (num >> ((i - 1) * 4)) & 0xF;
    *p++ = (digit < 0xA ? ('0' + digit) : ('A' + (digit - 0xA)));
  } while(--i);

  *pp = p;
}

static void unumber_repr(uintmax_t number, char **pp)
{
  char *p = *pp;
  size_t i = unumber_repr_len(number);

  do {
    uintmax_t digit = (number >> ((i - 1) * 4)) & 0xF;
    *p++ = (digit < 0xA ? ('0' + digit) : ('A' + (digit - 0xA)));
  } while(--i);

  *pp = p;
}

static size_t address_followup_repr_len(AddressFollowup *followup)
{
  size_t len = 5;

  if (followup == NULL)
    return 0;

  switch (followup->type) {
    case kAddressFollowupMissing: {
      return 0;
    }
    case kAddressFollowupShift: {
      len += number_repr_len((intmax_t) followup->data.shift);
      break;
    }
    case kAddressFollowupForwardPattern:
    case kAddressFollowupBackwardPattern: {
      len += 2 + regex_repr_len(followup->data.regex);
      break;
    }
  }

  len += address_followup_repr_len(followup->next);

  return len;
}

static void address_followup_repr(AddressFollowup *followup, char **pp)
{
  char *p = *pp;

  if (followup == NULL)
    return;

  switch (followup->type) {
    case kAddressFollowupMissing: {
      return;
    }
    case kAddressFollowupShift: {
      number_repr((intmax_t) followup->data.shift, &p);
      break;
    }
    case kAddressFollowupForwardPattern:
    case kAddressFollowupBackwardPattern: {
      char_u ch = followup->type == kAddressFollowupForwardPattern
                  ? '/'
                  : '?';
      *p++ = ch;
      regex_repr(followup->data.regex, &p);
      *p++ = ch;
      break;
    }
  }

  address_followup_repr(followup->next, &p);

  *pp = p;
}

static size_t address_repr_len(Address *address)
{
  size_t len = 0;

  if (address == NULL)
    return 0;

  switch (address->type) {
    case kAddrMissing: {
      return 0;
    }
    case kAddrFixed: {
      len += unumber_repr_len((uintmax_t) address->data.lnr);
      break;
    }
    case kAddrEnd:
    case kAddrCurrent: {
      len++;
      break;
    }
    case kAddrMark: {
      len += 2;
      break;
    }
    case kAddrForwardSearch:
    case kAddrBackwardSearch: {
      len += regex_repr_len(address->data.regex) + 2;
      break;
    }
    case kAddrForwardPreviousSearch:
    case kAddrBackwardPreviousSearch:
    case kAddrSubstituteSearch: {
      len += 2;
      break;
    }
  }

  return len;
}

static void address_repr(Address *address, char **pp)
{
  char *p = *pp;

  if (address == NULL)
    return;

  switch (address->type) {
    case kAddrMissing: {
      return;
    }
    case kAddrFixed: {
      unumber_repr((uintmax_t) address->data.lnr, &p);
      break;
    }
    case kAddrEnd: {
      *p++ = '$';
      break;
    }
    case kAddrCurrent: {
      *p++ = '.';
      break;
    }
    case kAddrMark: {
      *p++ = '\'';
      *p++ = address->data.mark;
      break;
    }
    case kAddrForwardSearch:
    case kAddrBackwardSearch: {
      char_u ch = address->type == kAddrForwardSearch
                  ? '/'
                  : '?';
      *p++ = ch;
      regex_repr(address->data.regex, &p);
      *p++ = ch;
      break;
    }
    case kAddrForwardPreviousSearch: {
      *p++ = '\\';
      *p++ = '/';
      break;
    }
    case kAddrBackwardPreviousSearch: {
      *p++ = '\\';
      *p++ = '?';
      break;
    }
    case kAddrSubstituteSearch: {
      *p++ = '\\';
      *p++ = '&';
      break;
    }
  }

  *pp = p;
}

static size_t range_repr_len(Range *range)
{
  size_t len = 0;

  if (range->address.type == kAddrMissing)
    return 0;

  len += address_repr_len(&(range->address));
  len += address_followup_repr_len(range->followups);
  if (range->next != NULL) {
    len++;
    len += range_repr_len(range->next);
  }

  return len;
}

static void range_repr(Range *range, char **pp)
{
  char *p = *pp;

  if (range->address.type == kAddrMissing)
    return;

  address_repr(&(range->address), &p);
  address_followup_repr(range->followups, &p);
  if (range->next != NULL) {
    *p++ = (range->setpos ? ';' : ',');
    range_repr(range->next, &p);
  }

  *pp = p;
}

static size_t node_repr_len(CommandNode *node)
{
  size_t len = 0;

  if (node == NULL)
    return 0;

  len += range_repr_len(&(node->range));

  if (node->type == kCmdUSER)
    len += STRLEN(node->name);
  else if (CMDDEF(node->type).name == NULL)
    len += 0;
  else
    len += STRLEN(CMDDEF(node->type).name);

  if (node->type == kCmdSyntaxError)
    // len("\\ error:\n")
    len += 9
         + 2 * (1 + STRLEN(node->args[ARG_ERROR_LINESTR].arg.str))
         + STRLEN(node->args[ARG_ERROR_MESSAGE].arg.str);

  if (node->bang)
    len++;

  switch (node->cnt_type) {
    case kCntMissing: {
      break;
    }
    case kCntCount:
    case kCntBuffer: {
      len += 1 + unumber_repr_len((uintmax_t) node->cnt.count);
      break;
    }
    case kCntReg: {
      len += 2;
      break;
    }
    case kCntExprReg: {
      // FIXME
      break;
    }
  }

  if (node->exflags)
    len++;
  if (node->exflags & FLAG_EX_LIST)
    len++;
  if (node->exflags & FLAG_EX_LNR)
    len++;
  if (node->exflags & FLAG_EX_PRINT)
    len++;

  if (CMDDEF(node->type).flags & ISMODIFIER) {
    len += node_repr_len(node->args[0].arg.cmd) + 1;
  } else if (CMDDEF(node->type).parse == &parse_append) {
    garray_T *ga = &(node->args[ARG_APPEND_LINES].arg.strs);
    int i = ga->ga_len;

    while (i--) {
      len += 1 + STRLEN(((char_u **)ga->ga_data)[i]);
    }

    len += 2;
  } else if (CMDDEF(node->type).parse == &parse_map) {
    uint_least32_t map_flags = node->args[ARG_MAP_FLAGS].arg.flags;

    if (map_flags)
      len++;

    if (map_flags & FLAG_MAP_BUFFER)
      len += 8;
    if (map_flags & FLAG_MAP_NOWAIT)
      len += 8;
    if (map_flags & FLAG_MAP_SILENT)
      len += 8;
    if (map_flags & FLAG_MAP_SPECIAL)
      len += 9;
    if (map_flags & FLAG_MAP_SCRIPT)
      len += 8;
    if (map_flags & FLAG_MAP_EXPR)
      len += 6;
    if (map_flags & FLAG_MAP_UNIQUE)
      len += 8;

    if (node->args[ARG_MAP_LHS].arg.str != NULL) {
      len += 1 + STRLEN(node->args[ARG_MAP_LHS].arg.str);
      if (node->args[ARG_MAP_RHS].arg.str != NULL)
        len += 1 + STRLEN(node->args[ARG_MAP_RHS].arg.str);
    }
    // FIXME untranslate mappings
#if 0
    char_u *translated;

    if (node->args[ARG_MAP_LHS].arg.str != NULL) {
      len++;

      translated = translate_mapping(
          node->args[ARG_MAP_LHS].arg.str, FALSE,
          0);
      len += STRLEN(translated);
      vim_free(translated);

      if (node->args[ARG_MAP_RHS].arg.str != NULL) {
        len++;

        translated = translate_mapping(
            node->args[ARG_MAP_LHS].arg.str, FALSE,
            0);
        len += STRLEN(translated);
        vim_free(translated);
      }
    }
#endif
  }

  return len;
}

static void node_repr(CommandNode *node, char **pp)
{
  char *p = *pp;
  size_t len = 0;
  char_u *name;

  if (node == NULL)
    return;

  range_repr(&(node->range), &p);

  if (node->type == kCmdUSER)
    name = node->name;
  else if (CMDDEF(node->type).name == NULL)
    name = (char_u *) "";
  else
    name = CMDDEF(node->type).name;

  len = STRLEN(name);
  if (len)
    memcpy(p, name, len);
  p += len;

  if (node->type == kCmdSyntaxError) {
    memcpy(p, "\\ error:\n", 9);
    p += 9;
    len = STRLEN(node->args[ARG_ERROR_LINESTR].arg.str);
    memcpy(p, node->args[ARG_ERROR_LINESTR].arg.str, len);
    p += len;
    *p++ = '\n';
    memset(p, (int) ' ', len);
    p[node->args[ARG_ERROR_OFFSET].arg.flags] = '^';
    p += len;
    *p++ = '\n';
    len = STRLEN(node->args[ARG_ERROR_MESSAGE].arg.str);
    memcpy(p, node->args[ARG_ERROR_MESSAGE].arg.str, len);
    p+= len;
  }

  if (node->bang)
    *p++ = '!';

  switch (node->cnt_type) {
    case kCntMissing: {
      break;
    }
    case kCntCount:
    case kCntBuffer: {
      *p++ = ' ';
      unumber_repr((uintmax_t) node->cnt.count, &p);
      break;
    }
    case kCntReg: {
      *p++ = ' ';
      *p++ = node->cnt.reg;
      break;
    }
    case kCntExprReg: {
      // FIXME
      break;
    }
  }

  if (node->exflags)
    *p++ = ' ';
  if (node->exflags & FLAG_EX_LIST)
    *p++ = 'l';
  if (node->exflags & FLAG_EX_LNR)
    *p++ = '#';
  if (node->exflags & FLAG_EX_PRINT)
    *p++ = 'p';

  if (CMDDEF(node->type).flags & ISMODIFIER) {
    *p++ = ' ';
    node_repr(node->args[0].arg.cmd, &p);
  } else if (CMDDEF(node->type).parse == &parse_append) {
    garray_T *ga = &(node->args[ARG_APPEND_LINES].arg.strs);
    int ga_len = ga->ga_len;
    int i;

    for (i = 0; i < ga_len ; i++) {
      *p++ = '\n';
      len = STRLEN(((char_u **)ga->ga_data)[i]);
      memcpy(p, ((char_u **)ga->ga_data)[i], len);
      p += len;
    }
    *p++ = '\n';
    *p++ = '.';
  } else if (CMDDEF(node->type).parse == &parse_map) {
    char_u *hs;
    uint_least32_t map_flags = node->args[ARG_MAP_FLAGS].arg.flags;

    if (map_flags)
      *p++ = ' ';

    if (map_flags & FLAG_MAP_BUFFER) {
      memcpy(p, "<buffer>", 8);
      p += 8;
    }
    if (map_flags & FLAG_MAP_NOWAIT) {
      memcpy(p, "<nowait>", 8);
      p += 8;
    }
    if (map_flags & FLAG_MAP_SILENT) {
      memcpy(p, "<silent>", 8);
      p += 8;
    }
    if (map_flags & FLAG_MAP_SPECIAL) {
      memcpy(p, "<special>", 9);
      p += 9;
    }
    if (map_flags & FLAG_MAP_SCRIPT) {
      memcpy(p, "<script>", 8);
      p += 8;
    }
    if (map_flags & FLAG_MAP_EXPR) {
      memcpy(p, "<expr>", 6);
      p += 6;
    }
    if (map_flags & FLAG_MAP_UNIQUE) {
      memcpy(p, "<unique>", 8);
      p += 8;
    }

    if (node->args[ARG_MAP_LHS].arg.str != NULL) {
      *p++ = ' ';

      hs = node->args[ARG_MAP_LHS].arg.str;
      len = STRLEN(hs);
      memcpy(p, hs, len);
      p += len;

      if (node->args[ARG_MAP_RHS].arg.str != NULL) {
        *p++ = ' ';

        hs = node->args[ARG_MAP_RHS].arg.str;
        len = STRLEN(hs);
        memcpy(p, hs, len);
        p += len;
      }
    }
    // FIXME untranslate mappings
#if 0
    if (node->args[ARG_MAP_LHS].arg.str != NULL) {
      *p++ = ' ';

      hs = translate_mapping(
          node->args[ARG_MAP_LHS].arg.str, FALSE,
          0);
      len = STRLEN(hs);
      memcpy(p, hs, len);
      p += len;
      vim_free(hs);

      if (node->args[ARG_MAP_RHS].arg.str != NULL) {
        *p++ = ' ';

        hs = translate_mapping(
            node->args[ARG_MAP_LHS].arg.str, FALSE,
            0);
        len = STRLEN(hs);
        memcpy(p, hs, len);
        p += len;
        vim_free(hs);
      }
    }
#endif
  }

  *pp = p;
}

char *parse_one_cmd_test(char_u *arg, uint_least8_t flags)
{
  char_u **pp;
  char_u *p;
  char_u *line;
  CommandNode *node;
  CommandPosition position = {1, 1, (char_u *) "<test input>"};
  char *repr;
  char *r;
  size_t len;

  pp = &arg;
  line = fgetline_test(0, pp, 0);
  p = line;
  if ((parse_one_cmd(&p, &node, flags, &position, (line_getter) fgetline_test,
                     pp)) == FAIL)
    ;
  vim_free(line);

  len = node_repr_len(node);

  if ((repr = ALLOC_CLEAR_NEW(char, len + 1)) == NULL) {
    free_cmd(node);
    return NULL;
  }

  r = repr;

  node_repr(node, &r);

  free_cmd(node);
  return repr;
}
