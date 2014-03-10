// vim: ts=8 sts=2 sw=2 tw=80

//
// Copyright 2014 Nikolay Pavlov

// cmd.c: Ex commands parsing

#include "vim.h"
#include "types.h"
#include "cmd.h"
#include "expr.h"
#include "cmd_def.h"
#define DO_DECLARE_EXCMD
#include "cmd_def.h"
#undef DO_DECLARE_EXCMD
#include "misc2.h"
#include "charset.h"
#include "globals.h"


static CommandNode *cmd_alloc(CommandType type)
{
  // XXX May allocate less space then needed to hold the whole struct: less by 
  // one size of CommandArg.
  size_t size = sizeof(CommandNode)
              + (sizeof(CommandArg) * (cmddefs[type].num_args));
  CommandNode *node = (CommandNode *) alloc_clear(size);

  if (node != NULL)
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

  free_address_data(&(range->start));
  free_address_data(&(range->end));
  free_address_followup(range->start_followups);
  free_address_followup(range->end_followups);
}

static void free_cmd_arg(CommandArg *arg, CommandArgType type)
{
  switch(type) {
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
    case kArgStaticString:
    case kArgLine:
    case kArgColumn:
    case kArgFlags:
    case kArgNumber:
    case kArgAuEvent:
    case kArgChar: {
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
      char_u *s = *(arg->arg.strs);
      while (s != NULL) {
        vim_free(s);
        ++s;
      }
      vim_free(arg->arg.strs);
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
  size_t numargs = cmddefs[node->type].num_args;
  size_t i;

  if (node == NULL)
    return;

  for (i = 0; i < numargs; i++)
    free_cmd_arg(&(node->args[i]), cmddefs[node->type].arg_types[i]);

  free_range_data(&(node->range));
  vim_free(node);
}

/*
 * Check for an Ex command with optional tail.
 * If there is a match advance "pp" to the argument and return TRUE.
 */
static int checkforcmd(char_u **pp, /* start of command */
                       char_u *cmd, /* name of command */
                       int    len)  /* required length */
{
  int i;

  for (i = 0; cmd[i] != NUL; ++i)
    if (((char_u *)cmd)[i] != (*pp)[i])
      break;

  if ((i >= len) && !isalpha((*pp)[i])) {
    *pp = skipwhite(*pp + i);
    return TRUE;
  }
  return FALSE;
}

static int get_pattern(char_u **pp, CommandParserError *error, Regex **target)
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
      ++p;
  }

  if (*p != NUL)
    ++p;

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
{
  char_u *p;

  p = skipwhite(*pp);
  switch (*p) {
    case '.': {
      address->type = kAddrCurrent;
      ++p;
      break;
    }
    case '$': {
      address->type = kAddrEnd;
      ++p;
      break;
    }
    case '\'': {
      address->type = kAddrMark;
      ++p;
      if (*p == NUL) {
        // FIXME proper message
        error->message = (char_u *)"expected mark name, but got nothing";
        error->position = p - 1;
        return FAIL;
      }
      address->data.mark = *p;
      ++p;
      break;
    }
    case '/':
    case '?': {
      char_u c = *p;
      ++p;
      if (c == '/')
        address->type = kAddrForwardSearch;
      else
        address->type = kAddrBackwardSearch;
      if (get_pattern(pp, error, &(address->data.regex)) == FAIL)
        return FAIL;
      break;
    }
    case '\\': {
      ++p;
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
      address->data.line = getdigits(pp);
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
{
  AddressFollowup *followup;

  if ((followup = ALLOC_CLEAR_NEW(AddressFollowup, 1)) != NULL)
    followup->type = type;

  return followup;
}

static int get_address_followups(char_u **pp, CommandParserError *error,
                                 AddressFollowup **followup)
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
    ++p;
    if ((fw = address_followup_alloc(type)) == NULL)
      return FAIL;
    if (type == kAddressFollowupForwardPattern ||
        type == kAddressFollowupBackwardPattern) {
      if (get_pattern(pp, error, &(fw->data.regex)) == FAIL) {
        free_address_followup(fw);
        return FAIL;
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
                             linenr_T line, colnr_T col, char_u *s)
{
  if (error->message != NULL) {
    if ((*node = cmd_alloc(kCmdSyntaxError)) == NULL)
      return FAIL;
    (*node)->args[ARG_ERROR_LINE].arg.line = line;
    (*node)->args[ARG_ERROR_COLUMN].arg.col =
        col + ((colnr_T) (error->position - s));
    (*node)->args[ARG_ERROR_MESSAGE].arg.str = error->message;
    (*node)->args[ARG_ERROR_LINESTR].arg.str = vim_strsave(s);
  }
  return OK;
}

static int parse_one_cmd(char_u **pp,
                         CommandNode **node,
                         int sourcing,
                         linenr_T line,
                         colnr_T col,
                         line_getter fgetline,
                         void *cookie)
{
  CommandNode **next_node = node;
  CommandParserError error;
  Range range;
  char_u *p;
  char_u *s = *pp;
  size_t len;
  size_t i;

  memset(&range, 0, sizeof(Range));
  memset(&error, 0, sizeof(CommandParserError));

  if (((*pp)[0] == '#') &&
      ((*pp)[1] == '!')) {
    if ((*next_node = cmd_alloc(kCmdHashbangComment)) == NULL)
      return FAIL;
    p = *pp + 2;
    len = STRLEN(p);
    (*next_node)->args[0].arg.str = vim_strnsave(p, len);
    *pp = p + len;
    return OK;
  }
  for (;;) {
    // 1. skip comment lines and leading white space and colons
    p = *pp;
    while (*p == ' ' || *p == '\t' || *p == ':')
      ++p;
    // FIXME investigate WTF was this:
    // in ex mode, an empty line works like :+
    if (*p == NUL /*&& exmode_active && getline_equal(…)*/)
      ;

    if (*p == '"') {
      if ((*next_node = cmd_alloc(kCmdComment)) == NULL)
        return FAIL;
      ++p;
      len = STRLEN(p);
      (*next_node)->args[0].arg.str = vim_strnsave(p, len);
      *pp = p + len;
      return OK;
    }

    if (*p == NUL)
      return OK;

    // FIXME Why was this here?
    /* if (VIM_ISDIGIT(*ea.cmd)) { */
      /* p = skipwhite(skipdigits(ea.cmd)); */
    /* } */

    // 2. handle command modifiers.
    CommandType type = kCmdComment;  // It cannot be comment
    for (i = 0; i < (size_t) kCmdSIZE; i++) {
      if (cmddefs[i].cmd_argt & ISMODIFIER) {
        size_t common_len;
        if (i > 0) {
          char_u *cmd_name = cmddefs[i].cmd_name;
          char_u *prev_cmd_name = cmddefs[i - 1].cmd_name;
          common_len = 0;
          // FIXME: Precompute and record this in cmddefs
          while (cmd_name[common_len] == prev_cmd_name[common_len])
            common_len++;
        } else {
          common_len = 1;
        }
        if (checkforcmd(&p, cmddefs[i].cmd_name, common_len)) {
          type = (CommandType) i;
          break;
        }
      }
    }
    if (type != kCmdComment) {
      if ((*next_node = cmd_alloc(type)) == NULL)
        return FAIL;
      next_node = &((*next_node)->args[0].arg.cmd);
    } else {
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
    if (get_address(&p, &range.end, &error) == FAIL) {
      create_error_node(next_node, &error, line, col, s);
      return FAIL;
    }
    if (get_address_followups(&p, &error, &range.end_followups) == FAIL) {
      create_error_node(next_node, &error, line, col, s);
      return FAIL;
    }
    p = skipwhite(p);
    if (range.end_followups != NULL) {
      range.end.type = kAddrCurrent;
    } else if (range.end.type == kAddrMissing) {
      if (*p == '%') {
        range.start.type = kAddrFixed;
        range.start.data.line = 1;
        range.end.type = kAddrEnd;
        break;
      } else if (*p == '*' && vim_strchr(p_cpo, CPO_STAR) == NULL) {
        range.start.type = kAddrMark;
        range.start.data.mark = '<';
        range.end.type = kAddrMark;
        range.end.data.mark = '>';
        break;
      }
    }
    if (*p == ';' || *p == ',') {
      range.setpos = (*p == ';');
      range.start = range.end;
      range.start_followups = range.end_followups;
      range.end.type = kAddrMissing;
      range.end_followups = NULL;
    } else {
      break;
    }
  }
  *pp = p;
  return OK;
}

// FIXME
void happy()
{
  parse_one_cmd(NULL, NULL, 0, 0, 0, NULL, NULL);
}
