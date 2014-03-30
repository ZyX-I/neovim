// vim: ts=8 sts=2 sw=2 tw=80

//
// Copyright 2014 Nikolay Pavlov

// cmd.c: Ex commands parsing

#include <stddef.h>

#include "nvim/vim.h"
#include "nvim/types.h"
#include "nvim/misc2.h"
#include "nvim/charset.h"
#include "nvim/globals.h"
#include "nvim/garray.h"
#include "nvim/term.h"
#include "nvim/farsi.h"
#include "nvim/menu.h"
#include "nvim/memory.h"

#include "nvim/translator/parser/expressions.h"
#include "nvim/translator/parser/ex_commands.h"

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
# include "translator/parser/ex_commands.c.generated.h"
#endif

#include "nvim/translator/parser/command_definitions.h"
#define DO_DECLARE_EXCMD
#include "nvim/translator/parser/command_definitions.h"
#undef DO_DECLARE_EXCMD

#define NODE_IS_ALLOCATED(node) ((node) != NULL && (node) != &nocmd)

#define ENDS_EXCMD(ch) ((ch) == NUL || (ch) == '|' || (ch) == '"' \
                        || (ch) == '\n')

/// Allocate new command node and assign its type property
///
/// Uses type argument to determine how much memory it should allocate.
///
/// @param[in]  type  Node type.
///
/// @return Pointer to allocated block of memory or NULL in case of error.
static CommandNode *cmd_alloc(CommandType type, CommandPosition *position)
{
  // XXX May allocate less space then needed to hold the whole struct: less by 
  // one size of CommandArg.
  size_t size = offsetof(CommandNode, args);
  CommandNode *node;

  if (type != kCmdUnknown)
    size += sizeof(CommandArg) * CMDDEF(type).num_args;

  if ((node = (CommandNode *) alloc_clear(size)) != NULL) {
    char_u *fname;
    if ((fname = vim_strsave(position->fname)) == NULL) {
      vim_free(node);
      return NULL;
    }
    node->type = type;
    node->position = *position;
    node->position.fname = fname;
  }

  return node;
}

/// Allocate new address followup structure and set its type
///
/// @param[in]  type  Followup type.
///
/// @return Pointer to allocated block of memory or NULL in case of error.
static AddressFollowup *address_followup_alloc(AddressFollowupType type)
{
  AddressFollowup *followup;

  if ((followup = ALLOC_CLEAR_NEW(AddressFollowup, 1)) != NULL)
    followup->type = type;

  return followup;
}

static void free_menu_item(MenuItem *menu_item)
{
  if (menu_item == NULL)
    return;

  free_menu_item(menu_item->subitem);
  vim_free(menu_item->name);
  vim_free(menu_item);
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
  free_address_followup(address->followups);
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

  if (!NODE_IS_ALLOCATED(node))
    return;

  numargs = CMDDEF(node->type).num_args;

  if (node->type != kCmdUnknown)
    for (i = 0; i < numargs; i++)
      free_cmd_arg(&(node->args[i]), CMDDEF(node->type).arg_types[i]);

  free_cmd(node->next);
  free_cmd(node->children);
  free_range_data(&(node->range));
  vim_free(node->name);
  vim_free(node);
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
                             CommandPosition *position, char_u *s)
{
  if (error->message != NULL) {
    char_u *line;
    char_u *message;

    if ((line = vim_strsave(s)) == NULL)
      return FAIL;
    if ((message = vim_strsave((char_u *) error->message)) == NULL) {
      vim_free(line);
      return FAIL;
    }
    if ((*node = cmd_alloc(kCmdSyntaxError, position)) == NULL) {
      vim_free(line);
      vim_free(message);
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

static int get_regex(char_u **pp, CommandParserError *error, Regex **target,
                     char_u endch)
  FUNC_ATTR_NONNULL_ALL FUNC_ATTR_WARN_UNUSED_RESULT
{
  // FIXME compile regex
  char_u *p = *pp;
  char_u *s = p;
  char_u c = p[-1];

  while (*p != c && *p != NUL && *p != endch)
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

  if (*p == endch)
    p++;

  *pp = p;

  return OK;
}

static int parse_append(char_u **pp,
                        CommandNode *node,
                        CommandParserError *error,
                        CommandParserOptions o,
                        CommandPosition *position,
                        line_getter fgetline,
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
/// @param[in]      special  TRUE if explicit <special> was supplied.
/// @param[in]      expr     TRUE if it is <expr>-type mapping.
/// @parblock
///   @note If this argument is always FALSE then you do not need to care about 
///         rhs_idx + 1 and node->children.
/// @endparblock
/// @param[in]      o         Options that control parsing behavior.
/// @param[in]      position  Position of input.
///
/// @return FAIL when out of memory, OK otherwise.
static int set_node_rhs(char_u *rhs, size_t rhs_idx, CommandNode *node,
                        bool special, bool expr, CommandParserOptions o,
                        CommandPosition *position)
{
  char_u *rhs_buf;

  rhs = replace_termcodes(rhs, &rhs_buf, FALSE, TRUE, special,
                          FLAG_POC_TO_FLAG_CPO(o.flags));
  if (rhs_buf == NULL)
    return FAIL;

  if ((o.flags&FLAG_POC_ALTKEYMAP) && (o.flags&FLAG_POC_RL))
    lrswap(rhs);

  if (expr) {
    ExpressionParserError expr_error;
    ExpressionNode *expr = NULL;
    char_u *rhs_end = rhs;

    expr_error.position = NULL;
    expr_error.message = NULL;

    if ((expr = parse0_err(&rhs_end, &expr_error)) == NULL) {
      CommandParserError error;
      if (expr_error.message == NULL) {
        vim_free(rhs);
        return FAIL;
      }
      error.position = expr_error.position;
      error.message = expr_error.message;
      if (create_error_node(&(node->children), &error, position, rhs)
          == FAIL) {
        vim_free(rhs);
        return FAIL;
      }
    } else if (*rhs_end != NUL) {
      CommandParserError error;

      free_expr(expr);

      error.position = rhs_end;
      error.message = N_("E15: trailing characters");

      if (create_error_node(&(node->children), &error, position, rhs)
          == FAIL) {
        vim_free(rhs);
        return FAIL;
      }
    } else {
      node->args[rhs_idx + 1].arg.expr = expr;
    }
  }
  node->args[rhs_idx].arg.str = rhs;
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
static int do_parse_map(char_u **pp,
                        CommandNode *node,
                        CommandParserError *error,
                        CommandParserOptions o,
                        CommandPosition *position,
                        line_getter fgetline,
                        void *cookie,
                        bool unmap)
{
  uint_least32_t map_flags = 0;
  char_u *p = *pp;
  char_u *lhs;
  char_u *lhs_end;
  char_u *rhs;
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
    char_u saved = *lhs_end;
    // Note: type of the abbreviation is not checked because it depends on the 
    //       &iskeyword option. Unlike $ENV parsing (which depends on the 
    //       options too) it is not unlikely that both 1. file will be parsed 
    //       before result is actually used and 2. option value at the execution 
    //       stage will make results invalid.
    *lhs_end = NUL;
    lhs = replace_termcodes(lhs, &lhs_buf, TRUE, TRUE,
                            map_flags&FLAG_MAP_SPECIAL,
                            FLAG_POC_TO_FLAG_CPO(o.flags));
    *lhs_end = saved;
    if (lhs_buf == NULL)
      return FAIL;
    node->args[ARG_MAP_LHS].arg.str = lhs;
  }

  if (*rhs != NUL) {
    assert(!unmap);
    if (STRICMP(rhs, "<nop>") == 0) {
      // Empty string
      rhs = ALLOC_CLEAR_NEW(char_u, 1);
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

static int parse_map(char_u **pp,
                     CommandNode *node,
                     CommandParserError *error,
                     CommandParserOptions o,
                     CommandPosition *position,
                     line_getter fgetline,
                     void *cookie)
{
  return do_parse_map(pp, node, error, o, position, fgetline, cookie, FALSE);
}

static int parse_unmap(char_u **pp,
                       CommandNode *node,
                       CommandParserError *error,
                       CommandParserOptions o,
                       CommandPosition *position,
                       line_getter fgetline,
                       void *cookie)
{
  return do_parse_map(pp, node, error, o, position, fgetline, cookie, TRUE);
}

static int parse_mapclear(char_u **pp,
                          CommandNode *node,
                          CommandParserError *error,
                          CommandParserOptions o,
                          CommandPosition *position,
                          line_getter fgetline,
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

static int parse_menu(char_u **pp,
                      CommandNode *node,
                      CommandParserError *error,
                      CommandParserOptions o,
                      CommandPosition *position,
                      line_getter fgetline,
                      void *cookie)
{
  // FIXME "menu *" parses to something weird
  uint_least32_t menu_flags = 0;
  char_u *p = *pp;
  size_t i;
  char_u *s;
  char_u *menu_path;
  char_u *menu_path_end;
  char_u *map_to;
  char_u *text = NULL;
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
    icon = p;

    while (*p != NUL && *p != ' ') {
      if (*p == '\\')
        p++;
      mb_ptr_adv(p);
    }

    if ((icon = vim_strnsave(icon, p - icon)) == NULL)
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
      if ((pris = ALLOC_CLEAR_NEW(int, i + 1)) == NULL)
        return FAIL;
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
  while (*p && !vim_iswhite(*p)) {
    if ((*p == '\\' || *p == Ctrl_V) && p[1] != NUL) {
      p++;
      if (*p == TAB) {
        text = p + 1;
        menu_path_end = p - 2;
      }
    } else if (STRNICMP(p, "<TAB>", 5) == 0) {
      menu_path_end = p - 1;
      p += 4;
      text = p + 1;
    } else if (*p == '.' && text == NULL) {
      menu_path_end = p - 1;
      if (menu_path_end == menu_path) {
        error->message = N_("E792: Empty menu name");
        error->position = p;
        return NOTDONE;
      }
    } else if ((!p[1] || vim_iswhite(p[1])) && p != menu_path && text == NULL) {
      menu_path_end = p;
    }
    if (menu_path_end != NULL) {
      if ((sub = ALLOC_CLEAR_NEW(MenuItem, 1)) == NULL)
        return FAIL;

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

  if (text != NULL) {
    if (node->args[ARG_MENU_NAME].arg.menu_item == NULL) {
      error->message = N_("E792: Empty menu name");
      error->position = text;
      return NOTDONE;
    }

    if ((text = vim_strnsave(text, p - text)) == NULL)
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
                     FALSE, o, position) == FAIL)
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
static int do_parse_expr(char_u **pp,
                         CommandNode *node,
                         CommandParserError *error,
                         CommandParserOptions o,
                         bool multi)
{
  ExpressionNode *expr;
  ExpressionParserError expr_error;
  char_u *expr_str_start;
  char_u *expr_str;

  if ((expr_str = vim_strsave(*pp)) == NULL)
    return FAIL;

  node->args[ARG_EXPR_STR].arg.str = expr_str;
  expr_str_start = expr_str;

  if (multi)
    expr = parse_mult(&expr_str, &expr_error, &parse0_err, FALSE,
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

static int parse_expr(char_u **pp,
                      CommandNode *node,
                      CommandParserError *error,
                      CommandParserOptions o,
                      CommandPosition *position,
                      line_getter fgetline,
                      void *cookie)
{
  return do_parse_expr(pp, node, error, o, FALSE);
}

static int parse_exprs(char_u **pp,
                       CommandNode *node,
                       CommandParserError *error,
                       CommandParserOptions o,
                       CommandPosition *position,
                       line_getter fgetline,
                       void *cookie)
{
  if (!**pp)
    return OK;
  return do_parse_expr(pp, node, error, o, TRUE);
}

static int parse_rest_line(char_u **pp,
                           CommandNode *node,
                           CommandParserError *error,
                           CommandParserOptions o,
                           CommandPosition *position,
                           line_getter fgetline,
                           void *cookie)
{
  size_t len = STRLEN(*pp);
  if ((node->args[0].arg.str = vim_strnsave(*pp, len)) == NULL)
    return FAIL;
  *pp += len;
  return OK;
}

static char_u *do_fgetline(int c, char_u **arg, int indent)
{
  if (*arg) {
    char_u *result;
    result = vim_strsave(*arg);
    *arg = NULL;
    return result;
  } else {
    return NULL;
  }
}

static int parse_do(char_u **pp,
                    CommandNode *node,
                    CommandParserError *error,
                    CommandParserOptions o,
                    CommandPosition *position,
                    line_getter fgetline,
                    void *cookie)
{
  CommandNode *cmd;
  char_u *arg = *pp;
  CommandPosition new_position = {
    1,
    1,
    (char_u *) "<*do argument>",
  };

  if ((cmd = parse_cmd_sequence(o, new_position, (line_getter) &do_fgetline,
                                &arg))
      == NULL)
    return FAIL;

  node->children = cmd;

  *pp += STRLEN(*pp);

  return OK;
}

static bool check_lval(ExpressionNode *expr, CommandParserError *error,
                       bool allow_list, bool allow_lower)
{
  switch (expr->type) {
    case kTypeSimpleVariableName: {
      if (!allow_lower && ASCII_ISLOWER(*expr->position)) {
        error->message =
            N_("E128: Function name must start with a capital "
               "or contain a colon or a hash");
        error->position = expr->position;
        return TRUE;
      }
      break;
    }
    case kTypeVariableName: {
      if (!allow_lower && expr->children->type == kTypeVariableName
          && ASCII_ISLOWER(*expr->children->position)) {
        error->message =
            N_("E128: Function name must start with a capital "
               "or contain a colon or a hash");
        error->position = expr->children->position;
        return TRUE;
      }
      break;
    }
    case kTypeConcatOrSubscript:
    case kTypeSubscript: {
      ExpressionNode *root = expr;
      while (root->children != NULL)
        root = root->children;

      if (root->type != kTypeVariableName
          && root->type != kTypeSimpleVariableName) {
        error->message =
            N_("E475: Expected variable name or a list of variable names");
        error->position = root->position;
        return TRUE;
      }
      break;
    }
    case kTypeList: {
      if (allow_list) {
        ExpressionNode *item = expr->children;

        if (item == NULL) {
          error->message =
              N_("E475: Expected non-empty list of variable names");
          error->position = expr->position;
          return TRUE;
        }

        while (item != NULL) {
          if (check_lval(item, error, FALSE, allow_lower))
            return TRUE;
          item = item->next;
        }
      } else {
        error->message = N_("E475: Expected variable name");
        error->position = expr->position;
        return TRUE;
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
      return TRUE;
    }
  }
  return FALSE;
}

#define FLAG_PLVAL_SPACEMULT 0x01
#define FLAG_PLVAL_LISTMULT  0x02
#define FLAG_PLVAL_NOLOWER   0x04

/// Parse left value of assignment
///
/// @param[in,out]  pp          Parsed string. Is advanced to the next character 
///                             after parsed expression.
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
///
///   @note If both FLAG_PLVAL_LISTMULT and FLAG_PLVAL_SPACEMULT were specified 
///         then only either space-separated values or list will be allowed, but 
///         not both.
/// @endparblock
///
/// @return OK if parsing was successfull, NOTDONE if it was not, FAIL when out 
///         of memory.
static int parse_lval(char_u **pp,
                      CommandParserError *error,
                      CommandParserOptions o,
                      ExpressionNode **expr,
                      int flags)
{
  ExpressionParserError expr_error;
  ExpressionNode *next;
  char_u *expr_start = *pp;
  bool allow_list = (bool) (flags&FLAG_PLVAL_LISTMULT);

  if (flags&FLAG_PLVAL_SPACEMULT)
    *expr = parse_mult(pp, &expr_error, &parse7_nofunc, TRUE,
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

  if ((*expr)->next != NULL)
    allow_list = FALSE;

  next = *expr;
  while (next != NULL) {
    if (check_lval(next, error, allow_list, !(flags&FLAG_PLVAL_NOLOWER))) {
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

static int parse_lvals(char_u **pp,
                       CommandNode *node,
                       CommandParserError *error,
                       CommandParserOptions o,
                       CommandPosition *position,
                       line_getter fgetline,
                       void *cookie)
{
  ExpressionNode *expr;
  char_u *expr_str;
  char_u *expr_str_start;
  int ret;

  if ((expr_str = vim_strsave(*pp)) == NULL)
    return FAIL;

  node->args[ARG_EXPRS_STR].arg.str = expr_str;

  expr_str_start = expr_str;

  if ((ret = parse_lval(&expr_str, error, o, &expr,
                        node->type == kCmdDelfunction
                        ? FLAG_PLVAL_NOLOWER
                        : FLAG_PLVAL_SPACEMULT)) == FAIL)
    return FAIL;

  node->args[ARG_EXPRS_EXPRS].arg.expr = expr;

  if (ret == NOTDONE) {
    error->position = *pp + (((char_u *) error->position) - expr_str_start);
    return NOTDONE;
  }

  node->args[ARG_EXPRS_EXPRS].arg.expr = expr;

  *pp += expr_str - expr_str_start;

  return OK;
}

static int parse_lockvar(char_u **pp,
                         CommandNode *node,
                         CommandParserError *error,
                         CommandParserOptions o,
                         CommandPosition *position,
                         line_getter fgetline,
                         void *cookie)
{
  if (VIM_ISDIGIT(**pp))
    node->args[ARG_LOCKVAR_DEPTH].arg.unumber = (unsigned) getdigits(pp);

  return parse_lvals(pp, node, error, o, position, fgetline, cookie);
}

static int parse_for(char_u **pp,
                     CommandNode *node,
                     CommandParserError *error,
                     CommandParserOptions o,
                     CommandPosition *position,
                     line_getter fgetline,
                     void *cookie)
{
  ExpressionNode *expr;
  ExpressionNode *list_expr;
  ExpressionParserError expr_error;
  char_u *expr_str;
  char_u *expr_str_start;
  int ret;

  if ((expr_str = vim_strsave(*pp)) == NULL)
    return FAIL;

  node->args[ARG_FOR_STR].arg.str = expr_str;

  expr_str_start = expr_str;

  if ((ret = parse_lval(&expr_str, error, o, &expr, FLAG_PLVAL_LISTMULT))
      == FAIL)
    return FAIL;

  node->args[ARG_FOR_LHS].arg.expr = expr;

  if (ret == NOTDONE) {
    error->position = *pp + (((char_u *) error->position) - expr_str_start);
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
    error->position = *pp + (((char_u *) expr_error.position) - expr_str_start);
    return NOTDONE;
  }

  node->args[ARG_FOR_RHS].arg.expr = list_expr;

  *pp += expr_str - expr_str_start;

  return OK;
}

static int parse_function(char_u **pp,
                          CommandNode *node,
                          CommandParserError *error,
                          CommandParserOptions o,
                          CommandPosition *position,
                          line_getter fgetline,
                          void *cookie)
{
  char_u *p = *pp;
  ExpressionNode *expr;
  int ret;
  char_u *expr_str;
  char_u *expr_str_start;
  garray_T *args = &(node->args[ARG_FUNC_ARGS].arg.strs);
  uint_least32_t flags = 0;
  bool mustend = FALSE;

  if (ENDS_EXCMD(*p))
    return OK;

  if (*p == '/')
    return get_regex(&p, error, &(node->args[ARG_FUNC_REG].arg.reg), '/');

  if ((expr_str = vim_strsave(*pp)) == NULL)
    return FAIL;

  node->args[ARG_FUNC_STR].arg.str = expr_str;

  expr_str_start = expr_str;

  if ((ret = parse_lval(&expr_str, error, o, &expr, FLAG_PLVAL_NOLOWER))
      == FAIL)
    return FAIL;

  node->args[ARG_FUNC_NAME].arg.expr = expr;

  if (ret == NOTDONE) {
    error->position = *pp + (((char_u *) error->position) - expr_str_start);
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

  ga_init2(args, (int) sizeof(char_u *), 3);

  while (*p != ')') {
    char *notend_message = N_("E475: Expected end of arguments list");
    if (p[0] == '.' && p[1] == '.' && p[2] == '.') {
      flags |= FLAG_FUNC_VARARGS;
      p += 3;
      mustend = TRUE;
    } else {
      char_u *arg_start = p;
      char_u *arg;
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

      if (ga_grow(args, 1) == FAIL) {
        ga_clear_strings(args);
        vim_free(arg);
        return FAIL;
      }

      ((char_u **)(args->ga_data))[args->ga_len++] = arg;
    }
    if (*p == ',') {
      p++;
    } else {
      mustend = TRUE;
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

static int parse_let(char_u **pp,
                     CommandNode *node,
                     CommandParserError *error,
                     CommandParserOptions o,
                     CommandPosition *position,
                     line_getter fgetline,
                     void *cookie)
{
  ExpressionNode *expr;
  ExpressionParserError expr_error;
  int ret;
  char_u *expr_str;
  char_u *expr_str_start;
  ExpressionNode *rval_expr;
  LetAssignmentType ass_type = 0;

  if (ENDS_EXCMD(**pp))
    return OK;

  if ((expr_str = vim_strsave(*pp)) == NULL)
    return FAIL;

  node->args[ARG_LET_STR].arg.str = expr_str;

  expr_str_start = expr_str;

  if ((ret = parse_lval(&expr_str, error, o, &expr,
                        FLAG_PLVAL_LISTMULT|FLAG_PLVAL_SPACEMULT))
      == FAIL)
    return FAIL;

  node->args[ARG_LET_LHS].arg.expr = expr;

  if (ret == NOTDONE) {
    error->position = *pp + (((char_u *) error->position) - expr_str_start);
    return NOTDONE;
  }

  expr_str = skipwhite(expr_str);

  if (ENDS_EXCMD(*expr_str)) {
    if (expr->type == kTypeList) {
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
    error->position = *pp + (((char_u *) expr_error.position) - expr_str_start);
    return NOTDONE;
  }

  node->args[ARG_LET_RHS].arg.expr = rval_expr;

  *pp += expr_str - expr_str_start;
  return OK;
}

/// Check for an Ex command with optional tail.
///
/// @param[in,out]  pp   Start of the command. Is advanced to the command 
///                      argument if requested command was found.
/// @param[in]      cmd  Name of the command which is checked for.
/// @param[in]      len  Minimal length required to accept a match.
///
/// @return TRUE if requested command was found, FALSE otherwise.
static int checkforcmd(char_u **pp, char_u *cmd, int len)
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

/// Get a single Ex adress
///
/// @param[in,out]  pp       Parsed string. Is advanced to the next character 
///                          after parsed address. May point at whitespace.
/// @param[out]     address  Structure where result will be saved.
/// @param[out]     error    Structure where errors are saved.
///
/// @return OK when parsing was successfull, FAIL otherwise.
static int get_address(char_u **pp, Address *address,
                       CommandParserError *error)
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
      address->data.mark = *p;
      if (*p != NUL)
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
        if (get_regex(pp, error, &(fw->data.regex),
                      type == kAddressFollowupForwardPattern ? '/' : '?')
            == FAIL) {
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

/// Check if p points to a separator between Ex commands (possibly with spaces)
///
/// @param[in]  p  Checked string
///
/// @return First character of next command (last character after command 
///         separator), NULL if no separator was found.
static char_u *check_nextcmd(char_u *p)
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
static int get_cmd_arg(CommandType type, CommandParserOptions o, char_u *start,
                       char_u **arg, size_t *next_cmd_offset)
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
        char_u *nextcmd = check_nextcmd(p);
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
int parse_one_cmd(char_u **pp,
                  CommandNode **node,
                  CommandParserOptions o,
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
  CommandArgsParser parse = NULL;
  CountType cnt_type = kCntMissing;
  int count = 0;

  memset(&range, 0, sizeof(Range));
  memset(&current_range, 0, sizeof(Range));
  memset(&error, 0, sizeof(CommandParserError));

  if (((*pp)[0] == '#') &&
      ((*pp)[1] == '!') &&
      position->col == 1) {
    if ((*next_node = cmd_alloc(kCmdHashbangComment, position)) == NULL)
      return FAIL;
    p = *pp + 2;
    len = STRLEN(p);
    if (((*next_node)->args[0].arg.str = vim_strnsave(p, len)) == NULL)
      return FAIL;
    *pp = p + len;
    (*next_node)->end_col = position->col + (*pp - s);
    return OK;
  }

  p = *pp;
  for (;;) {
    char_u *pstart;
    // 1. skip comment lines and leading white space and colons
    while (*p == ' ' || *p == TAB || *p == ':')
      p++;
    // in ex mode, an empty line works like :+ (switch to next line)
    if (*p == NUL && o.flags&FLAG_POC_EXMODE) {
      AddressFollowup *fw;
      if ((*next_node = cmd_alloc(kCmdHashbangComment, position)) == NULL)
        return FAIL;
      (*next_node)->range.address.type = kAddrCurrent;
      if ((fw = address_followup_alloc(kAddressFollowupShift)) == NULL) {
        free_cmd(*next_node);
        *next_node = NULL;
        return FAIL;
      }
      fw->data.shift = 1;
      (*next_node)->range.address.followups = fw;
      (*next_node)->end_col = position->col + (*pp - s);
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
      (*next_node)->end_col = position->col + (*pp - s);
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
      char_u *mod_start = p;

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
        if (VIM_ISDIGIT(*pstart) && !((CMDDEF(type).flags) & COUNT)) {
          error.message = (char *) e_norange;
          error.position = pstart;
          if (create_error_node(next_node, &error, position, s) == FAIL)
            return FAIL;
          return NOTDONE;
        }
        if (*p == '!' && !(CMDDEF(type).flags & BANG)) {
          error.message = (char *) e_nobang;
          error.position = p;
          if (create_error_node(next_node, &error, position, s) == FAIL)
            return FAIL;
          return NOTDONE;
        }
        if ((*next_node = cmd_alloc(type, position)) == NULL)
          return FAIL;
        if (VIM_ISDIGIT(*pstart)) {
          (*next_node)->cnt_type = kCntCount;
          (*next_node)->cnt.count = getdigits(&pstart);
        }
        if (*p == '!') {
          (*next_node)->bang = TRUE;
          p++;
        }
        (*next_node)->position.col = position->col + (mod_start - s);
        (*next_node)->end_col = position->col + (p - s - 1);
        next_node = &((*next_node)->children);
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

  char_u *modifiers_end = p;
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
        if ((current_range.next = ALLOC_CLEAR_NEW(Range, 1)) == NULL) {
          free_range_data(&range);
          free_range_data(&current_range);
          return FAIL;
        }
        current_range.next->address.type = kAddrEnd;
        range = current_range;
        p++;
        break;
      } else if (*p == '*' && !(o.flags&FLAG_POC_CPO_STAR)) {
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
    if (*p == '|' || (o.flags&FLAG_POC_EXMODE
                      && range.address.type != kAddrMissing)) {
      if (NODE_IS_ALLOCATED(*next_node))
        (*next_node)->end_col = position->col + (p - modifiers_end);
      if ((*next_node = cmd_alloc(kCmdPrint, position)) == NULL) {
        free_range_data(&range);
        return FAIL;
      }
      (*next_node)->range = range;
      p++;
      *pp = p;
      (*next_node)->end_col = position->col + (*pp - s);
      return OK;
    } else if (*p == '"') {
      free_range_data(&range);
      if (NODE_IS_ALLOCATED(*next_node))
        (*next_node)->end_col = position->col + (p - modifiers_end);
      if ((*next_node = cmd_alloc(kCmdComment, position)) == NULL)
        return FAIL;
      p++;
      len = STRLEN(p);
      if (((*next_node)->args[0].arg.str = vim_strnsave(p, len)) == NULL)
        return FAIL;
      *pp = p + len;
      (*next_node)->end_col = position->col + (*pp - s);
      return OK;
    } else {
      if (NODE_IS_ALLOCATED(*next_node))
        (*next_node)->end_col = position->col + (p - modifiers_end);
      if ((*next_node = cmd_alloc(kCmdMissing, position)) == NULL) {
        free_range_data(&range);
        return FAIL;
      }
      (*next_node)->range = range;

      *pp = (nextcmd == NULL
             ? p
             : nextcmd);
      (*next_node)->end_col = position->col + (*pp - s);
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
      bang = TRUE;
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
    free_range_data(&range);
    error.message = (char *) e_trailing;
    error.position = p;
    if (create_error_node(next_node, &error, position, s) == FAIL)
      return FAIL;
    return NOTDONE;
  }

  if (NODE_IS_ALLOCATED(*next_node))
    (*next_node)->end_col = position->col + (p - modifiers_end);
  if ((*next_node = cmd_alloc(type, position)) == NULL) {
    free_range_data(&range);
    return FAIL;
  }
  (*next_node)->bang = bang;
  (*next_node)->range = range;
  (*next_node)->name = name;
  (*next_node)->exflags = exflags;
  (*next_node)->cnt_type = cnt_type;
  (*next_node)->cnt.count = count;

  parse = CMDDEF(type).parse;

  if (parse != NULL) {
    int ret;
    bool used_get_cmd_arg = FALSE;
    char_u *cmd_arg = p;
    char_u *cmd_arg_start = p;
    size_t next_cmd_offset = 0;
    // XFILE commands may have bangs inside `=…`
    // ISGREP commands may have bangs inside patterns
    // ISEXPR commands may have bangs inside "" or as logical OR
    if (!(CMDDEF(type).flags & (XFILE|ISGREP|ISEXPR|LITERAL))) {
      used_get_cmd_arg = TRUE;
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
        vim_free(cmd_arg_start);
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
      vim_free(cmd_arg_start);
      p += next_cmd_offset;
    } else {
      p = cmd_arg;
    }
    *pp = p;
    (*next_node)->end_col = position->col + (*pp - s);
    return ret;
  }

  *pp = p;
  (*next_node)->end_col = position->col + (*pp - s);
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
      bo->push_stack = TRUE;
      break;
    }
    case kCmdElse: {
      bo->no_start_message  = N_("E581: :else without :if");
      bo->duplicate_message = N_("E583: multiple :else");
      bo->find_in_stack_2 = kCmdElseif;
      bo->find_in_stack   = kCmdIf;
      bo->push_stack = TRUE;
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
      bo->push_stack = TRUE;
      break;
    }
    case kCmdCatch: {
      bo->not_after_message = N_("E604: :catch after :finally");
      bo->no_start_message  = N_("E603: :catch without :try");
      bo->find_in_stack_2 = kCmdCatch;
      bo->find_in_stack   = kCmdTry;
      bo->not_after = kCmdFinally;
      bo->push_stack = TRUE;
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
      bo->push_stack = TRUE;
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
      assert(FALSE);
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
    FALSE
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
  FALSE,
  {
    {{0}}
  }
};

#define NEW_ERROR_NODE(prev_node, error_message, error_position, line_start) \
        { \
          CommandParserError error; \
          error.message = error_message; \
          error.position = error_position; \
          assert(prev_node->next == NULL); \
          if (create_error_node(&(prev_node->next), &error, &position, \
                                line_start) \
              == FAIL) { \
            free_cmd(result); \
            vim_free(line_start); \
            return FAIL; \
          } \
          prev_node->next->prev = prev_node; \
        }

/// Parses sequence of commands
///
/// @param[in]      o         Options that control parsing behavior. In addition 
///                           to flags documented in parse_one_command 
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
                                line_getter fgetline,
                                void *cookie)
{
  char_u *line_start, *line;
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
      char_u *parse_start = line;
      CommandBlockOptions bo;
      int ret;
      CommandType block_type;
      CommandNode *block_command_node;

      position.col = line - line_start + 1;
      assert(!NODE_IS_ALLOCATED(*next_node));
      if ((ret = parse_one_cmd(&line, next_node, o, &position, fgetline,
                               cookie))
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

      block_type = block_command_node->type;

      get_block_options(block_type, &bo);

      if (block_type == kCmdFunction)
        if (block_command_node->args[ARG_FUNC_ARGS].arg.strs.ga_growsize == 0)
          memset(&bo, 0, sizeof(CommandBlockOptions));

      if (bo.find_in_stack != kCmdUnknown) {
        size_t initial_blockstack_len = blockstack_len;

        while (blockstack_len) {
          CommandType last_block_type = blockstack[blockstack_len - 1].type;
          if (bo.not_after != kCmdUnknown && last_block_type == bo.not_after) {
            free_cmd(*next_node);
            *next_node = NULL;
            NEW_ERROR_NODE(blockstack[blockstack_len - 1].node,
                           bo.not_after_message, line, line_start)
            break;
          } else if (bo.duplicate_message != NULL
                     && last_block_type == (*next_node)->type) {
            free_cmd(*next_node);
            *next_node = NULL;
            NEW_ERROR_NODE(blockstack[blockstack_len - 1].node,
                           bo.duplicate_message, line, line_start)
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
            NEW_ERROR_NODE(prev_node, bo.no_start_message, line, line_start)
            prev_node = prev_node->next;
            next_node = &(prev_node->next);
            break;
          }
        }
      } else {
        (*next_node)->prev = prev_node;
        if (bo.push_stack) {
          blockstack_len++;
          if (blockstack_len >= MAX_NEST_BLOCKS) {
            CommandParserError error;
            // FIXME Make message with error code
            error.message = N_("too many nested blocks");
            error.position = line;
            if (create_error_node(next_node, &error, &position, line_start)
                == FAIL) {
              free_cmd(result);
              vim_free(line_start);
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
      if (*line == '|' || *line == '\n' || *line == '"')
        line++;
    }
    position.lnr++;
    vim_free(line_start);
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
  } while (--i);

  *pp = p;
}

static void unumber_repr(uintmax_t number, char **pp)
{
  char *p = *pp;
  size_t i = unumber_repr_len(number);

  do {
    uintmax_t digit = (number >> ((i - 1) * 4)) & 0xF;
    *p++ = (digit < 0xA ? ('0' + digit) : ('A' + (digit - 0xA)));
  } while (--i);

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
  len += address_followup_repr_len(range->address.followups);
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
  address_followup_repr(range->address.followups, &p);
  if (range->next != NULL) {
    *p++ = (range->setpos ? ';' : ',');
    range_repr(range->next, &p);
  }

  *pp = p;
}

static size_t node_repr_len(CommandNode *node, size_t indent, bool barnext)
{
  size_t len = 0;
  size_t start_from_arg;
  bool do_arg_dump = FALSE;

  assert(node->end_col >= node->position.col
         || node->type == kCmdSyntaxError);

  assert(node->next == NULL || node->next->prev == node);

  assert(node->prev == NULL || node->prev->next == node);

  if (node == NULL)
    return 0;

  if (!barnext)
    len += indent;
  len += range_repr_len(&(node->range));

  if (node->name != NULL)
    len += STRLEN(node->name);
  else if (CMDDEF(node->type).name == NULL)
    len += 0;
  else
    len += STRLEN(CMDDEF(node->type).name);

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

  if (node->type == kCmdSyntaxError) {
    char_u *line = node->args[ARG_ERROR_LINESTR].arg.str;
    char_u *message = node->args[ARG_ERROR_MESSAGE].arg.str;
    size_t offset = node->args[ARG_ERROR_OFFSET].arg.flags;

    // len("\\ error: ")
    len += 9;
    len += STRLEN(message);
    len += 2;
    len += STRLEN(line);
    if (offset < len)
      len += 4;
    else
      len += 3;
  } else if (CMDDEF(node->type).parse == &parse_append) {
    garray_T *ga = &(node->args[ARG_APPEND_LINES].arg.strs);
    int i = ga->ga_len;

    while (i--)
      len += 1 + STRLEN(((char_u **)ga->ga_data)[i]);

    len += 2;
  } else if (CMDDEF(node->type).parse == &parse_map
             || CMDDEF(node->type).parse == &parse_unmap) {
    uint_least32_t map_flags = node->args[ARG_MAP_FLAGS].arg.flags;
    bool unmap = CMDDEF(node->type).parse == &parse_unmap;

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
      if (unmap) {
      } else if (node->args[ARG_MAP_EXPR].arg.expr != NULL) {
        len += 1 + expr_node_dump_len(node->args[ARG_MAP_EXPR].arg.expr);
      } else if (node->children != NULL) {
        len += 1 + node_repr_len(node->children, indent, barnext);
      } else if (node->args[ARG_MAP_RHS].arg.str != NULL) {
        len += 1 + STRLEN(node->args[ARG_MAP_RHS].arg.str);
      }
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
  } else if (CMDDEF(node->type).parse == &parse_mapclear) {
    if (node->args[ARG_CLEAR_BUFFER].arg.flags)
      len += 1 + 8;
  } else if (CMDDEF(node->type).parse == &parse_menu) {
    uint_least32_t menu_flags = node->args[ARG_MENU_FLAGS].arg.flags;

    if (menu_flags & (FLAG_MENU_SILENT|FLAG_MENU_SPECIAL|FLAG_MENU_SCRIPT))
      len++;

    if (menu_flags & FLAG_MENU_SILENT)
      len += 8;
    if (menu_flags & FLAG_MENU_SPECIAL)
      len += 9;
    if (menu_flags & FLAG_MENU_SCRIPT)
      len += 8;

    if (node->args[ARG_MENU_ICON].arg.str != NULL)
      len += 1 + 5 + STRLEN(node->args[ARG_MENU_ICON].arg.str);

    if (node->args[ARG_MENU_PRI].arg.numbers != NULL) {
      int *number = node->args[ARG_MENU_PRI].arg.numbers;

      len++;

      while (*number) {
        if (*number != MENU_DEFAULT_PRI)
          len += unumber_repr_len((uintmax_t) *number);
        number++;
        if (*number)
          len++;
      }
    }

    if (menu_flags & FLAG_MENU_DISABLE)
      len += 1 + 7;
    if (menu_flags & FLAG_MENU_ENABLE)
      len += 1 + 6;

    if (node->args[ARG_MENU_NAME].arg.menu_item != NULL) {
      MenuItem *cur = node->args[ARG_MENU_NAME].arg.menu_item;

      while (cur != NULL) {
        // Space or dot + STRLEN
        len += 1 + STRLEN(cur->name);
        cur = cur->subitem;
      }

      if (node->args[ARG_MENU_TEXT].arg.str != NULL)
        len += 5 + STRLEN(node->args[ARG_MENU_TEXT].arg.str);
    }

    start_from_arg = ARG_MENU_RHS;
    do_arg_dump = TRUE;
  } else if (CMDDEF(node->type).parse == &parse_for) {
    len++;
    len += expr_node_dump_len(node->args[ARG_FOR_LHS].arg.expr);
    len += 4;
    len += expr_node_dump_len(node->args[ARG_FOR_RHS].arg.expr);
  } else if (CMDDEF(node->type).parse == &parse_expr
             || CMDDEF(node->type).parse == &parse_exprs
             || CMDDEF(node->type).parse == &parse_lvals) {
    start_from_arg = 1;
    do_arg_dump = TRUE;
  } else if (CMDDEF(node->type).parse == &parse_lockvar) {
    if (node->args[ARG_LOCKVAR_DEPTH].arg.number)
      len += 1 + unumber_repr_len(node->args[ARG_LOCKVAR_DEPTH].arg.number);
    len += 1 + expr_node_dump_len(node->args[ARG_EXPRS_EXPRS].arg.expr);
  } else if (CMDDEF(node->type).parse == &parse_function) {
    if (node->args[ARG_FUNC_REG].arg.reg == NULL) {
      if (node->args[ARG_FUNC_NAME].arg.expr != NULL) {
        len++;
        len += expr_node_dump_len(node->args[ARG_FUNC_NAME].arg.expr);
        if (node->args[ARG_FUNC_ARGS].arg.strs.ga_itemsize != 0) {
          uint_least32_t flags = node->args[ARG_FUNC_FLAGS].arg.flags;
          garray_T *ga = &(node->args[ARG_FUNC_ARGS].arg.strs);

          len++;
          for (int i = 0; i < ga->ga_len; i++) {
            len += STRLEN(((char_u **)ga->ga_data)[i]);
            if (i < ga->ga_len - 1 || flags&FLAG_FUNC_VARARGS)
              len += 2;
          }

          if (flags&FLAG_FUNC_VARARGS)
            len += 3;

          len++;

          if (flags&FLAG_FUNC_RANGE)
            len += 1 + 5;
          if (flags&FLAG_FUNC_DICT)
            len += 1 + 4;
          if (flags&FLAG_FUNC_ABORT)
            len += 1 + 5;
        }
      }
    }
  } else if (CMDDEF(node->type).parse == &parse_let) {
    if (node->args[ARG_LET_LHS].arg.expr != NULL) {
      LetAssignmentType ass_type =
          (LetAssignmentType) node->args[ARG_LET_ASS_TYPE].arg.flags;
      bool add_rval = TRUE;

      len++;
      len += expr_node_dump_len(node->args[ARG_LET_LHS].arg.expr);
      switch (ass_type) {
        case VAL_LET_NO_ASS: {
          add_rval = FALSE;
          break;
        }
        case VAL_LET_ASSIGN: {
          len += 2;
          break;
        }
        case VAL_LET_ADD:
        case VAL_LET_SUBTRACT:
        case VAL_LET_APPEND: {
          len += 3;
          break;
        }
      }
      if (add_rval) {
        assert(node->args[ARG_LET_RHS].arg.expr != NULL);
        len++;
        len += expr_node_dump_len(node->args[ARG_LET_RHS].arg.expr);
      }
      assert(add_rval || node->args[ARG_LET_RHS].arg.expr == NULL);
    }
  } else {
    start_from_arg = 0;
    do_arg_dump = TRUE;
  }

  if (do_arg_dump) {
    size_t i;

    for (i = start_from_arg; i < CMDDEF(node->type).num_args; i++) {
      switch (CMDDEF(node->type).arg_types[i]) {
        case kArgExpressions:
        case kArgExpression: {
          if (node->args[i].arg.expr != NULL)
            len += 1 + expr_node_dump_len(node->args[i].arg.expr);
          break;
        }
        case kArgString: {
          if (node->args[i].arg.str != NULL)
            len += 1 + STRLEN(node->args[i].arg.str);
        }
        default: {
          break;
        }
      }
    }
  }

  if (node->children) {
    if (CMDDEF(node->type).flags & ISMODIFIER) {
      len += 1 + node_repr_len(node->children, indent, barnext);
    } else if (CMDDEF(node->type).parse == &parse_do) {
      len += 1 + node_repr_len(node->children, indent, TRUE);
    } else {
      if (barnext)
        len += 3;
      else
        len++;
      len += node_repr_len(node->children, indent + 2, barnext);
    }
  }

  if (node->next != NULL) {
    if (barnext)
      len += 3;
    else
      len++;
    len += node_repr_len(node->next, indent, barnext);
  }

  return len;
}

static void node_repr(CommandNode *node, size_t indent, bool barnext, char **pp)
{
  char *p = *pp;
  size_t len = 0;
  size_t start_from_arg;
  bool do_arg_dump = FALSE;
  char_u *name;

  if (node == NULL)
    return;

  if (!barnext) {
    memset(p, ' ', indent);
    p += indent;
  }

  range_repr(&(node->range), &p);

  if (node->name != NULL)
    name = node->name;
  else if (CMDDEF(node->type).name == NULL)
    name = (char_u *) "";
  else
    name = CMDDEF(node->type).name;

  len = STRLEN(name);
  if (len)
    memcpy(p, name, len);
  p += len;

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

  if (node->type == kCmdSyntaxError) {
    char_u *line = node->args[ARG_ERROR_LINESTR].arg.str;
    char_u *message = node->args[ARG_ERROR_MESSAGE].arg.str;
    size_t offset = node->args[ARG_ERROR_OFFSET].arg.flags;

    memcpy(p, "\\ error: ", 9);
    p += 9;

    len = STRLEN(message);
    memcpy(p, node->args[ARG_ERROR_MESSAGE].arg.str, len);
    p+= len;

    *p++ = ':';
    *p++ = ' ';

    len = STRLEN(line);

    if (offset < len) {
      if (offset) {
        memcpy(p, line, offset);
        p += offset;
      }
      *p++ = '!';
      *p++ = '!';
      *p++ = line[offset];
      *p++ = '!';
      *p++ = '!';

      memcpy(p, line + offset + 1, len - offset - 1);
      p += len - offset - 1;
    } else {
      memcpy(p, line, len);
      p += len;
      *p++ = '!';
      *p++ = '!';
      *p++ = '!';
    }
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
  } else if (CMDDEF(node->type).parse == &parse_map
             || CMDDEF(node->type).parse == &parse_unmap) {
    char_u *hs;
    uint_least32_t map_flags = node->args[ARG_MAP_FLAGS].arg.flags;
    bool unmap = CMDDEF(node->type).parse == &parse_unmap;

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

      if (unmap) {
      } else if (node->args[ARG_MAP_EXPR].arg.expr != NULL) {
        *p++ = ' ';
        expr_node_dump(node->args[ARG_MAP_EXPR].arg.expr, &p);
      } else if (node->children != NULL) {
        *p++ = '\n';
        node_repr(node->children, indent, barnext, &p);
      } else if (node->args[ARG_MAP_RHS].arg.str != NULL) {
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
  } else if (CMDDEF(node->type).parse == &parse_mapclear) {
    if (node->args[ARG_CLEAR_BUFFER].arg.flags) {
      *p++ = ' ';
      memcpy(p, "<buffer>", 8);
      p += 8;
    }
  } else if (CMDDEF(node->type).parse == &parse_menu) {
    uint_least32_t menu_flags = node->args[ARG_MENU_FLAGS].arg.flags;

    if (menu_flags & (FLAG_MENU_SILENT|FLAG_MENU_SPECIAL|FLAG_MENU_SCRIPT))
      *p++ = ' ';

    if (menu_flags & FLAG_MENU_SILENT) {
      memcpy(p, "<silent>", 8);
      p += 8;
    }
    if (menu_flags & FLAG_MENU_SPECIAL) {
      memcpy(p, "<special>", 9);
      p += 9;
    }
    if (menu_flags & FLAG_MENU_SCRIPT) {
      memcpy(p, "<script>", 8);
      p += 8;
    }

    if (node->args[ARG_MENU_ICON].arg.str != NULL) {
      *p++ = ' ';
      memcpy(p, "icon=", 5);
      p += 5;
      len = STRLEN(node->args[ARG_MENU_ICON].arg.str);
      memcpy(p, node->args[ARG_MENU_ICON].arg.str, len);
      p += len;
    }

    if (node->args[ARG_MENU_PRI].arg.numbers != NULL) {
      int *number = node->args[ARG_MENU_PRI].arg.numbers;

      *p++ = ' ';

      while (*number) {
        if (*number != MENU_DEFAULT_PRI)
          unumber_repr((uintmax_t) *number, &p);
        number++;
        if (*number)
          *p++ = '.';
      }
    }

    if (menu_flags & FLAG_MENU_DISABLE) {
      *p++ = ' ';
      memcpy(p, "disable", 7);
      p += 7;
    }
    if (menu_flags & FLAG_MENU_ENABLE) {
      *p++ = ' ';
      memcpy(p, "enable", 6);
      p += 6;
    }

    if (node->args[ARG_MENU_NAME].arg.menu_item != NULL) {
      MenuItem *cur = node->args[ARG_MENU_NAME].arg.menu_item;

      while (cur != NULL) {
        if (cur == node->args[ARG_MENU_NAME].arg.menu_item)
          *p++ = ' ';
        else
          *p++ = '.';
        len = STRLEN(cur->name);
        memcpy(p, cur->name, len);
        p += len;
        cur = cur->subitem;
      }

      if (node->args[ARG_MENU_TEXT].arg.str != NULL) {
        *p++ = '<';
        *p++ = 'T';
        *p++ = 'a';
        *p++ = 'b';
        *p++ = '>';
        len = STRLEN(node->args[ARG_MENU_TEXT].arg.str);
        memcpy(p, node->args[ARG_MENU_TEXT].arg.str, len);
        p += len;
      }
    }

    start_from_arg = ARG_MENU_RHS;
    do_arg_dump = TRUE;
  } else if (CMDDEF(node->type).parse == &parse_for) {
    *p++ = ' ';
    expr_node_dump(node->args[ARG_FOR_LHS].arg.expr, &p);
    memcpy(p, " in ", 4);
    p += 4;
    expr_node_dump(node->args[ARG_FOR_RHS].arg.expr, &p);
  } else if (CMDDEF(node->type).parse == &parse_expr
             || CMDDEF(node->type).parse == &parse_exprs
             || CMDDEF(node->type).parse == &parse_lvals) {
    start_from_arg = 1;
    do_arg_dump = TRUE;
  } else if (CMDDEF(node->type).parse == &parse_lockvar) {
    if (node->args[ARG_LOCKVAR_DEPTH].arg.number) {
      *p++ = ' ';
      unumber_repr(node->args[ARG_LOCKVAR_DEPTH].arg.number, &p);
    }
    *p++ = ' ';
    expr_node_dump(node->args[ARG_EXPRS_EXPRS].arg.expr, &p);
  } else if (CMDDEF(node->type).parse == &parse_function) {
    if (node->args[ARG_FUNC_REG].arg.reg == NULL) {
      if (node->args[ARG_FUNC_NAME].arg.expr != NULL) {
        *p++ = ' ';
        expr_node_dump(node->args[ARG_FUNC_NAME].arg.expr, &p);
        if (node->args[ARG_FUNC_ARGS].arg.strs.ga_itemsize != 0) {
          uint_least32_t flags = node->args[ARG_FUNC_FLAGS].arg.flags;
          garray_T *ga = &(node->args[ARG_FUNC_ARGS].arg.strs);
          *p++ = '(';
          for (int i = 0; i < ga->ga_len; i++) {
            len = STRLEN(((char_u **)ga->ga_data)[i]);
            memcpy(p, ((char_u **)ga->ga_data)[i], len);
            p += len;
            if (i < ga->ga_len - 1 || flags&FLAG_FUNC_VARARGS) {
              *p++ = ',';
              *p++ = ' ';
            }
          }
          if (flags&FLAG_FUNC_VARARGS) {
            *p++ = '.';
            *p++ = '.';
            *p++ = '.';
          }
          *p++ = ')';
          if (flags&FLAG_FUNC_RANGE) {
            *p++ = ' ';
            memcpy(p, "range", 5);
            p += 5;
          }
          if (flags&FLAG_FUNC_DICT) {
            *p++ = ' ';
            memcpy(p, "dict", 4);
            p += 4;
          }
          if (flags&FLAG_FUNC_ABORT) {
            *p++ = ' ';
            memcpy(p, "abort", 5);
            p += 5;
          }
        }
      }
    }
  } else if (CMDDEF(node->type).parse == &parse_let) {
    if (node->args[ARG_LET_LHS].arg.expr != NULL) {
      LetAssignmentType ass_type =
          (LetAssignmentType) node->args[ARG_LET_ASS_TYPE].arg.flags;
      bool add_rval = TRUE;

      *p++ = ' ';
      expr_node_dump(node->args[ARG_LET_LHS].arg.expr, &p);
      switch (ass_type) {
        case VAL_LET_NO_ASS: {
          add_rval = FALSE;
          break;
        }
        case VAL_LET_ASSIGN: {
          *p++ = ' ';
          *p++ = '=';
          break;
        }
        case VAL_LET_ADD: {
          *p++ = ' ';
          *p++ = '+';
          *p++ = '=';
          break;
        }
        case VAL_LET_SUBTRACT: {
          *p++ = ' ';
          *p++ = '-';
          *p++ = '=';
          break;
        }
        case VAL_LET_APPEND: {
          *p++ = ' ';
          *p++ = '.';
          *p++ = '=';
          break;
        }
      }
      if (add_rval) {
        *p++ = ' ';
        expr_node_dump(node->args[ARG_LET_RHS].arg.expr, &p);
      }
    }
  } else {
    start_from_arg = 0;
    do_arg_dump = TRUE;
  }

  if (do_arg_dump) {
    size_t i;

    for (i = start_from_arg; i < CMDDEF(node->type).num_args; i++) {
      switch (CMDDEF(node->type).arg_types[i]) {
        case kArgExpressions:
        case kArgExpression: {
          if (node->args[i].arg.expr != NULL) {
            *p++ = ' ';
            expr_node_dump(node->args[i].arg.expr, &p);
          }
          break;
        }
        case kArgString: {
          if (node->args[i].arg.str != NULL) {
            *p++ = ' ';
            len = STRLEN(node->args[i].arg.str);
            memcpy(p, node->args[i].arg.str, len);
            p += len;
          }
          break;
        }
        default: {
          break;
        }
      }
    }
  }

  if (node->children) {
    if (CMDDEF(node->type).flags & ISMODIFIER) {
      *p++ = ' ';
      node_repr(node->children, indent, barnext, &p);
    } else if (CMDDEF(node->type).parse == &parse_do) {
      *p++ = ' ';
      node_repr(node->children, indent, TRUE, &p);
    } else {
      if (barnext) {
        memcpy(p, " | ", 3);
        p += 3;
      } else {
        *p++ = '\n';
      }
      node_repr(node->children, indent + 2, barnext, &p);
    }
  }

  if (node->next != NULL) {
    if (barnext) {
      memcpy(p, " | ", 3);
      p += 3;
    } else {
      *p++ = '\n';
    }
    node_repr(node->next, indent, barnext, &p);
  }

  *pp = p;
}

char *parse_cmd_test(char_u *arg, uint_least8_t flags, bool one)
{
  CommandNode *node = NULL;
  CommandPosition position = {1, 1, (char_u *) "<test input>"};
  CommandParserOptions o = {flags, FALSE};
  char *repr;
  char *r;
  size_t len;
  char_u **pp;

  pp = &arg;

  if (one) {
    char_u *p;
    char_u *line;
    line = fgetline_test(0, pp, 0);
    p = line;
    if (parse_one_cmd(&p, &node, o, &position, (line_getter) fgetline_test,
                      pp) == FAIL)
      return NULL;
    vim_free(line);
  } else {
    if ((node = parse_cmd_sequence(o, position, (line_getter) fgetline_test,
                                   pp)) == FAIL)
      return NULL;
  }

  len = node_repr_len(node, 0, FALSE);

  if ((repr = ALLOC_CLEAR_NEW(char, len + 1)) == NULL) {
    free_cmd(node);
    return NULL;
  }

  r = repr;

  node_repr(node, 0, FALSE, &r);

  free_cmd(node);
  return repr;
}
