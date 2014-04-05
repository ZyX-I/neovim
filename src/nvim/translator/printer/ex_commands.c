#include <stddef.h>
#include <stdbool.h>

#include "nvim/vim.h"
#include "nvim/misc2.h"

#include "nvim/translator/printer/expressions.h"
#include "nvim/translator/parser/expressions.h"
#include "nvim/translator/parser/ex_commands.h"

#ifdef INCLUDE_GENERATED_DECLARATIONS
# include "translator/printer/ex_commands.c.generated.h"
#endif

static char_u *fgetline_test(int c, char_u **arg, int indent)
  FUNC_ATTR_NONNULL_ALL FUNC_ATTR_WARN_UNUSED_RESULT
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
  FUNC_ATTR_NONNULL_ALL FUNC_ATTR_CONST
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
  FUNC_ATTR_NONNULL_ALL FUNC_ATTR_CONST
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
  FUNC_ATTR_NONNULL_ALL FUNC_ATTR_CONST
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
  FUNC_ATTR_NONNULL_ALL
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
  FUNC_ATTR_NONNULL_ALL
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
  FUNC_ATTR_CONST
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
  FUNC_ATTR_NONNULL_ALL
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
  FUNC_ATTR_CONST
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
  FUNC_ATTR_NONNULL_ALL
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
  FUNC_ATTR_CONST
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
  FUNC_ATTR_NONNULL_ALL
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
  FUNC_ATTR_CONST
{
  size_t len = 0;
  size_t start_from_arg;
  bool do_arg_dump = FALSE;
  bool did_children = FALSE;

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

  if (node->optflags & FLAG_OPT_BIN_USE_FLAG) {
    if (node->optflags & FLAG_OPT_BIN)
      len += 6;
    else
      len += 8;
  }
  if (node->optflags & FLAG_OPT_EDIT)
    len += 7;

  switch (node->optflags & FLAG_OPT_FF_MASK) {
    case 0: {
      break;
    }
    case VAL_OPT_FF_DOS: {
      len += 9;
      break;
    }
    case VAL_OPT_FF_MAC: {
      len += 9;
      break;
    }
    case VAL_OPT_FF_UNIX: {
      len += 10;
      break;
    }
    default: {
      assert(FALSE);
    }
  }
  switch (node->optflags & FLAG_OPT_BAD_MASK) {
    case 0: {
      break;
    }
    case VAL_OPT_BAD_KEEP: {
      len += 11;
      break;
    }
    case VAL_OPT_BAD_DROP: {
      len += 11;
      break;
    }
    default: {
      len += 7;
      len++;
    }
  }
  if (node->enc != NULL) {
    len += 7;
    len += STRLEN(node->enc);
  }

  if (CMDDEF(node->type).flags & EDITCMD && node->children) {
    did_children = TRUE;
    // Worst case: we need to escape every single character
    len += node_repr_len(node->children, indent, TRUE) * 2;
  }

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
  } else if (CMDDEF(node->type).parse == CMDDEF(kCmdAppend).parse) {
    garray_T *ga = &(node->args[ARG_APPEND_LINES].arg.strs);
    int i = ga->ga_len;

    while (i--)
      len += 1 + STRLEN(((char_u **)ga->ga_data)[i]);

    len += 2;
  } else if (CMDDEF(node->type).parse == CMDDEF(kCmdMap).parse
             || CMDDEF(node->type).parse == CMDDEF(kCmdUnmap).parse) {
    uint_least32_t map_flags = node->args[ARG_MAP_FLAGS].arg.flags;
    bool unmap = CMDDEF(node->type).parse == CMDDEF(kCmdUnmap).parse;

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
        did_children = TRUE;
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
  } else if (CMDDEF(node->type).parse == CMDDEF(kCmdMapclear).parse) {
    if (node->args[ARG_CLEAR_BUFFER].arg.flags)
      len += 1 + 8;
  } else if (CMDDEF(node->type).parse == CMDDEF(kCmdMenu).parse) {
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
  } else if (CMDDEF(node->type).parse == CMDDEF(kCmdFor).parse) {
    len++;
    len += expr_node_dump_len(node->args[ARG_FOR_LHS].arg.expr);
    len += 4;
    len += expr_node_dump_len(node->args[ARG_FOR_RHS].arg.expr);
  } else if (CMDDEF(node->type).parse == CMDDEF(kCmdCaddexpr).parse
             || CMDDEF(node->type).parse == CMDDEF(kCmdEcho).parse
             || CMDDEF(node->type).parse == CMDDEF(kCmdDelfunction).parse) {
    start_from_arg = 1;
    do_arg_dump = TRUE;
  } else if (CMDDEF(node->type).parse == CMDDEF(kCmdLockvar).parse) {
    if (node->args[ARG_LOCKVAR_DEPTH].arg.number)
      len += 1 + unumber_repr_len(node->args[ARG_LOCKVAR_DEPTH].arg.number);
    len += 1 + expr_node_dump_len(node->args[ARG_EXPRS_EXPRS].arg.expr);
  } else if (CMDDEF(node->type).parse == CMDDEF(kCmdFunction).parse) {
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
  } else if (CMDDEF(node->type).parse == CMDDEF(kCmdLet).parse) {
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

  if (node->children && !did_children) {
    if (CMDDEF(node->type).flags & ISMODIFIER) {
      len += 1 + node_repr_len(node->children, indent, barnext);
    } else if (CMDDEF(node->type).parse == CMDDEF(kCmdArgdo).parse) {
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

#define ADD_STRING(p, s, l) memcpy(p, s, l); p += l

static void node_repr(CommandNode *node, size_t indent, bool barnext, char **pp)
  FUNC_ATTR_NONNULL_ALL
{
  char *p = *pp;
  size_t len = 0;
  size_t start_from_arg;
  bool do_arg_dump = FALSE;
  bool did_children = FALSE;
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

  if (node->optflags & FLAG_OPT_BIN_USE_FLAG) {
    if (node->optflags & FLAG_OPT_BIN) {
      ADD_STRING(p, " ++bin", 6);
    } else {
      ADD_STRING(p, " ++nobin", 8);
    }
  }
  if (node->optflags & FLAG_OPT_EDIT) {
    ADD_STRING(p, " ++edit", 7);
  }
  switch (node->optflags & FLAG_OPT_FF_MASK) {
    case 0: {
      break;
    }
    case VAL_OPT_FF_DOS: {
      ADD_STRING(p, " ++ff=dos", 9);
      break;
    }
    case VAL_OPT_FF_MAC: {
      ADD_STRING(p, " ++ff=mac", 9);
      break;
    }
    case VAL_OPT_FF_UNIX: {
      ADD_STRING(p, " ++ff=unix", 10);
      break;
    }
    default: {
      assert(FALSE);
    }
  }
  switch (node->optflags & FLAG_OPT_BAD_MASK) {
    case 0: {
      break;
    }
    case VAL_OPT_BAD_KEEP: {
      ADD_STRING(p, " ++bad=keep", 11);
      break;
    }
    case VAL_OPT_BAD_DROP: {
      ADD_STRING(p, " ++bad=drop", 11);
      break;
    }
    default: {
      ADD_STRING(p, " ++bad=", 7);
      *p++ = VAL_OPT_BAD_TO_CHAR(node->optflags);
    }
  }
  if (node->enc != NULL) {
    ADD_STRING(p, " ++enc=", 7);
    len = STRLEN(node->enc);
    ADD_STRING(p, node->enc, len);
  }

  if (CMDDEF(node->type).flags & EDITCMD && node->children) {
    char *arg_start;
    did_children = TRUE;
    *p++ = ' ';
    *p++ = '+';
    arg_start = p;
    node_repr(node->children, indent, TRUE, &p);
    while (arg_start < p) {
      if (vim_isspace(*arg_start) || *arg_start == '\\'
          || ENDS_EXCMD(*arg_start)) {
        STRMOVE(arg_start + 1, arg_start);
        *arg_start = '\\';
        arg_start += 2;
        p++;
      } else {
        arg_start++;
      }
    }
  }

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

    ADD_STRING(p, "\\ error: ", 9);

    len = STRLEN(message);
    ADD_STRING(p, node->args[ARG_ERROR_MESSAGE].arg.str, len);

    *p++ = ':';
    *p++ = ' ';

    len = STRLEN(line);

    if (offset < len) {
      if (offset) {
        ADD_STRING(p, line, offset);
      }
      *p++ = '!';
      *p++ = '!';
      *p++ = line[offset];
      *p++ = '!';
      *p++ = '!';

      ADD_STRING(p, line + offset + 1, len - offset - 1);
    } else {
      ADD_STRING(p, line, len);
      *p++ = '!';
      *p++ = '!';
      *p++ = '!';
    }
  } else if (CMDDEF(node->type).parse == CMDDEF(kCmdAppend).parse) {
    garray_T *ga = &(node->args[ARG_APPEND_LINES].arg.strs);
    int ga_len = ga->ga_len;
    int i;

    for (i = 0; i < ga_len ; i++) {
      *p++ = '\n';
      len = STRLEN(((char_u **)ga->ga_data)[i]);
      ADD_STRING(p, ((char_u **)ga->ga_data)[i], len);
    }
    *p++ = '\n';
    *p++ = '.';
  } else if (CMDDEF(node->type).parse == CMDDEF(kCmdMap).parse
             || CMDDEF(node->type).parse == CMDDEF(kCmdUnmap).parse) {
    uint_least32_t map_flags = node->args[ARG_MAP_FLAGS].arg.flags;
    bool unmap = CMDDEF(node->type).parse == CMDDEF(kCmdUnmap).parse;
    char_u *hs;

    if (map_flags)
      *p++ = ' ';

    if (map_flags & FLAG_MAP_BUFFER) {
      ADD_STRING(p, "<buffer>", 8);
    }
    if (map_flags & FLAG_MAP_NOWAIT) {
      ADD_STRING(p, "<nowait>", 8);
    }
    if (map_flags & FLAG_MAP_SILENT) {
      ADD_STRING(p, "<silent>", 8);
    }
    if (map_flags & FLAG_MAP_SPECIAL) {
      ADD_STRING(p, "<special>", 9);
    }
    if (map_flags & FLAG_MAP_SCRIPT) {
      ADD_STRING(p, "<script>", 8);
    }
    if (map_flags & FLAG_MAP_EXPR) {
      ADD_STRING(p, "<expr>", 6);
    }
    if (map_flags & FLAG_MAP_UNIQUE) {
      ADD_STRING(p, "<unique>", 8);
    }

    if (node->args[ARG_MAP_LHS].arg.str != NULL) {
      *p++ = ' ';

      hs = node->args[ARG_MAP_LHS].arg.str;
      len = STRLEN(hs);
      ADD_STRING(p, hs, len);

      if (unmap) {
      } else if (node->args[ARG_MAP_EXPR].arg.expr != NULL) {
        *p++ = ' ';
        expr_node_dump(node->args[ARG_MAP_EXPR].arg.expr, &p);
      } else if (node->children != NULL) {
        *p++ = '\n';
        node_repr(node->children, indent, barnext, &p);
        did_children = TRUE;
      } else if (node->args[ARG_MAP_RHS].arg.str != NULL) {
        *p++ = ' ';

        hs = node->args[ARG_MAP_RHS].arg.str;
        len = STRLEN(hs);
        ADD_STRING(p, hs, len);
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
      ADD_STRING(p, hs, len);
      vim_free(hs);

      if (node->args[ARG_MAP_RHS].arg.str != NULL) {
        *p++ = ' ';

        hs = translate_mapping(
            node->args[ARG_MAP_LHS].arg.str, FALSE,
            0);
        len = STRLEN(hs);
        ADD_STRING(p, hs, len);
        vim_free(hs);
      }
    }
#endif
  } else if (CMDDEF(node->type).parse == CMDDEF(kCmdMapclear).parse) {
    if (node->args[ARG_CLEAR_BUFFER].arg.flags) {
      *p++ = ' ';
      ADD_STRING(p, "<buffer>", 8);
    }
  } else if (CMDDEF(node->type).parse == CMDDEF(kCmdMenu).parse) {
    uint_least32_t menu_flags = node->args[ARG_MENU_FLAGS].arg.flags;

    if (menu_flags & (FLAG_MENU_SILENT|FLAG_MENU_SPECIAL|FLAG_MENU_SCRIPT))
      *p++ = ' ';

    if (menu_flags & FLAG_MENU_SILENT) {
      ADD_STRING(p, "<silent>", 8);
    }
    if (menu_flags & FLAG_MENU_SPECIAL) {
      ADD_STRING(p, "<special>", 9);
    }
    if (menu_flags & FLAG_MENU_SCRIPT) {
      ADD_STRING(p, "<script>", 8);
    }

    if (node->args[ARG_MENU_ICON].arg.str != NULL) {
      *p++ = ' ';
      ADD_STRING(p, "icon=", 5);
      len = STRLEN(node->args[ARG_MENU_ICON].arg.str);
      ADD_STRING(p, node->args[ARG_MENU_ICON].arg.str, len);
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
      ADD_STRING(p, "disable", 7);
    }
    if (menu_flags & FLAG_MENU_ENABLE) {
      *p++ = ' ';
      ADD_STRING(p, "enable", 6);
    }

    if (node->args[ARG_MENU_NAME].arg.menu_item != NULL) {
      MenuItem *cur = node->args[ARG_MENU_NAME].arg.menu_item;

      while (cur != NULL) {
        if (cur == node->args[ARG_MENU_NAME].arg.menu_item)
          *p++ = ' ';
        else
          *p++ = '.';
        len = STRLEN(cur->name);
        ADD_STRING(p, cur->name, len);
        cur = cur->subitem;
      }

      if (node->args[ARG_MENU_TEXT].arg.str != NULL) {
        *p++ = '<';
        *p++ = 'T';
        *p++ = 'a';
        *p++ = 'b';
        *p++ = '>';
        len = STRLEN(node->args[ARG_MENU_TEXT].arg.str);
        ADD_STRING(p, node->args[ARG_MENU_TEXT].arg.str, len);
      }
    }

    start_from_arg = ARG_MENU_RHS;
    do_arg_dump = TRUE;
  } else if (CMDDEF(node->type).parse == CMDDEF(kCmdFor).parse) {
    *p++ = ' ';
    expr_node_dump(node->args[ARG_FOR_LHS].arg.expr, &p);
    ADD_STRING(p, " in ", 4);
    expr_node_dump(node->args[ARG_FOR_RHS].arg.expr, &p);
  } else if (CMDDEF(node->type).parse == CMDDEF(kCmdCaddexpr).parse
             || CMDDEF(node->type).parse == CMDDEF(kCmdEcho).parse
             || CMDDEF(node->type).parse == CMDDEF(kCmdDelfunction).parse) {
    start_from_arg = 1;
    do_arg_dump = TRUE;
  } else if (CMDDEF(node->type).parse == CMDDEF(kCmdLockvar).parse) {
    if (node->args[ARG_LOCKVAR_DEPTH].arg.number) {
      *p++ = ' ';
      unumber_repr(node->args[ARG_LOCKVAR_DEPTH].arg.number, &p);
    }
    *p++ = ' ';
    expr_node_dump(node->args[ARG_EXPRS_EXPRS].arg.expr, &p);
  } else if (CMDDEF(node->type).parse == CMDDEF(kCmdFunction).parse) {
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
            ADD_STRING(p, ((char_u **)ga->ga_data)[i], len);
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
            ADD_STRING(p, "range", 5);
          }
          if (flags&FLAG_FUNC_DICT) {
            *p++ = ' ';
            ADD_STRING(p, "dict", 4);
          }
          if (flags&FLAG_FUNC_ABORT) {
            *p++ = ' ';
            ADD_STRING(p, "abort", 5);
          }
        }
      }
    }
  } else if (CMDDEF(node->type).parse == CMDDEF(kCmdLet).parse) {
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
            ADD_STRING(p, node->args[i].arg.str, len);
          }
          break;
        }
        default: {
          break;
        }
      }
    }
  }

  if (node->children && !did_children) {
    if (CMDDEF(node->type).flags & ISMODIFIER) {
      *p++ = ' ';
      node_repr(node->children, indent, barnext, &p);
    } else if (CMDDEF(node->type).parse == CMDDEF(kCmdArgdo).parse) {
      *p++ = ' ';
      node_repr(node->children, indent, TRUE, &p);
    } else {
      if (barnext) {
        ADD_STRING(p, " | ", 3);
      } else {
        *p++ = '\n';
      }
      node_repr(node->children, indent + 2, barnext, &p);
    }
  }

  if (node->next != NULL) {
    if (barnext) {
      ADD_STRING(p, " | ", 3);
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
