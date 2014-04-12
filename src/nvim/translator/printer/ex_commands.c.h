#ifndef NEOVIM_TRANSLATOR_PRINTER_EX_COMMANDS_C_H

#include "nvim/translator/printer/ch_macros.h"

// {{{ Function declarations
static FDEC(unumber_repr, uintmax_t unumber);
static FDEC(number_repr, intmax_t number);
static FDEC(glob_repr, Glob *glob);
static FDEC(regex_repr, Regex *regex);
static FDEC(address_followup_repr, AddressFollowup *followup);
static FDEC(address_repr, Address *address);
static FDEC(range_repr, Range *range);
static FDEC(node_repr, CommandNode *node, size_t indent, bool barnext);
// }}}

#ifndef DEFINE_LENGTH
# define DEFINE_LENGTH
# include "nvim/translator/printer/ex_commands.c.h"
# undef DEFINE_LENGTH
#endif
#define NEOVIM_TRANSLATOR_PRINTER_EX_COMMANDS_C_H

#include "nvim/translator/printer/ch_macros.h"

static FDEC(unumber_repr, uintmax_t unumber)
{
  FUNCTION_START;
#ifdef DEFINE_LENGTH
  uintmax_t i = unumber;
  do {
    i /= 10;
    ADD_CHAR(0);
  } while (i);
#else
  uintmax_t i = unumber_repr_len(po, unumber);
  do {
    uintmax_t digit;
    uintmax_t d = 1;
    for (uintmax_t j = 1; j < i; j++)
      d *= 10;
    digit = (unumber / d) % 10;
    ADD_CHAR('0' + (char) digit);
  } while (--i);
#endif
  FUNCTION_END;
}

static FDEC(number_repr, intmax_t number)
{
  FUNCTION_START;
  ADD_CHAR((number >= 0 ? '+' : '-'));
  F(unumber_repr, (uintmax_t) (number >= 0 ? number : -number));
  FUNCTION_END;
}

static FDEC(glob_repr, Glob *glob)
{
  FUNCTION_START;
  Glob *cur_glob;

  if (glob == NULL)
    RETURN;

  for (cur_glob = glob; cur_glob != NULL; cur_glob = cur_glob->next) {
    switch (cur_glob->type) {
      case kGlobExpression: {
        ADD_STATIC_STRING("`=");
        F(expr_node_dump, cur_glob->data.expr);
        ADD_CHAR('`');
        break;
      }
      case kGlobShell: {
        ADD_STRING(cur_glob->data.str)
        break;
      }
      case kPatHome: {
        ADD_CHAR('~');
        break;
      }
      case kPatCurrent: {
        ADD_CHAR('%');
        break;
      }
      case kPatAlternate: {
        ADD_CHAR('#');
        break;
      }
      case kPatCharacter: {
        ADD_CHAR('?');
        break;
      }
      case kPatAnything: {
        ADD_CHAR('*');
        break;
      }
      case kPatArguments: {
        ADD_STATIC_STRING("##");
        break;
      }
      case kPatAnyRecurse: {
        ADD_STATIC_STRING("**");
        break;
      }
      case kPatOldFile: {
        ADD_STATIC_STRING("#<");
        F(unumber_repr, (uintmax_t) cur_glob->data.number);
        break;
      }
      case kPatBufname: {
        ADD_CHAR('#');
        F(unumber_repr, (uintmax_t) cur_glob->data.number);
        break;
      }
      case kPatCollection: {
        // FIXME
        break;
      }
      case kPatEnviron: {
        // FIXME
        break;
      }
      case kPatBranch: {
        Glob *cglob;
        ADD_CHAR('{');
        for (cglob = cur_glob->data.glob; cglob != NULL; cglob = cglob->next) {
          F(glob_repr, cglob);
          if (cglob->next != NULL)
            ADD_CHAR(',');
        }
        ADD_CHAR('}');
        break;
      }
      case kPatLiteral: {
        ADD_STRING(cur_glob->data.str)
        break;
      }
      case kPatMissing: {
        assert(FALSE);
      }
    }
  }

  FUNCTION_END;
}

static FDEC(regex_repr, Regex *regex)
{
  FUNCTION_START;
  assert(regex->string != NULL);
  ADD_STRING(regex->string)
  FUNCTION_END;
}

static FDEC(address_followup_repr, AddressFollowup *followup)
{
  FUNCTION_START;

  if (followup == NULL)
    RETURN;

  switch (followup->type) {
    case kAddressFollowupMissing: {
      RETURN;
    }
    case kAddressFollowupShift: {
      F(number_repr, (intmax_t) followup->data.shift);
      break;
    }
    case kAddressFollowupForwardPattern:
    case kAddressFollowupBackwardPattern: {
#ifndef DEFINE_LENGTH
      char_u ch = followup->type == kAddressFollowupForwardPattern
                  ? '/'
                  : '?';
#endif
      ADD_CHAR(ch);
      F(regex_repr, followup->data.regex);
      ADD_CHAR(ch);
      break;
    }
  }

  F(address_followup_repr, followup->next);

  FUNCTION_END;
}

static FDEC(address_repr, Address *address)
{
  FUNCTION_START;

  if (address == NULL)
    RETURN;

  switch (address->type) {
    case kAddrMissing: {
      RETURN;
    }
    case kAddrFixed: {
      F(unumber_repr, (uintmax_t) address->data.lnr);
      break;
    }
    case kAddrEnd: {
      ADD_CHAR('$');
      break;
    }
    case kAddrCurrent: {
      ADD_CHAR('.');
      break;
    }
    case kAddrMark: {
      ADD_CHAR('\'');
      ADD_CHAR(address->data.mark);
      break;
    }
    case kAddrForwardSearch:
    case kAddrBackwardSearch: {
#ifndef DEFINE_LENGTH
      char_u ch = address->type == kAddrForwardSearch
                  ? '/'
                  : '?';
#endif
      ADD_CHAR(ch);
      F(regex_repr, address->data.regex);
      ADD_CHAR(ch);
      break;
    }
    case kAddrForwardPreviousSearch: {
      ADD_STATIC_STRING("\\/");
      break;
    }
    case kAddrBackwardPreviousSearch: {
      ADD_STATIC_STRING("\\?");
      break;
    }
    case kAddrSubstituteSearch: {
      ADD_STATIC_STRING("\\&");
      break;
    }
  }

  FUNCTION_END;
}

static FDEC(range_repr, Range *range)
{
  FUNCTION_START;

  if (range->address.type == kAddrMissing)
    RETURN;

  F(address_repr, &(range->address));
  F(address_followup_repr, range->address.followups);
  if (range->next != NULL) {
    ADD_CHAR(range->setpos ? ';' : ',');
    F(range_repr, range->next);
  }

  FUNCTION_END;
}

static FDEC(node_repr, CommandNode *node, size_t indent, bool barnext)
{
  FUNCTION_START;
  size_t start_from_arg;
  bool do_arg_dump = FALSE;
  bool did_children = FALSE;
  char_u *name;

  if (node == NULL)
    RETURN;

  if (!barnext) {
    INDENT(indent)
  }

  F(range_repr, &(node->range));

  if (node->name != NULL)
    name = node->name;
  else if (CMDDEF(node->type).name == NULL)
    name = (char_u *) "";
  else
    name = CMDDEF(node->type).name;

  ADD_STRING(name)

  if (node->bang)
    ADD_CHAR('!');

  if (node->optflags & FLAG_OPT_BIN_USE_FLAG) {
    if (node->optflags & FLAG_OPT_BIN) {
      ADD_STATIC_STRING(" ++bin");
    } else {
      ADD_STRING(" ++nobin");
    }
  }
  if (node->optflags & FLAG_OPT_EDIT) {
    ADD_STATIC_STRING(" ++edit");
  }
  switch (node->optflags & FLAG_OPT_FF_MASK) {
    case 0: {
      break;
    }
    case VAL_OPT_FF_DOS: {
      ADD_STATIC_STRING(" ++ff=dos");
      break;
    }
    case VAL_OPT_FF_MAC: {
      ADD_STATIC_STRING(" ++ff=mac");
      break;
    }
    case VAL_OPT_FF_UNIX: {
      ADD_STATIC_STRING(" ++ff=unix");
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
      ADD_STATIC_STRING(" ++bad=keep");
      break;
    }
    case VAL_OPT_BAD_DROP: {
      ADD_STATIC_STRING(" ++bad=drop");
      break;
    }
    default: {
      ADD_STATIC_STRING(" ++bad=");
      ADD_CHAR(VAL_OPT_BAD_TO_CHAR(node->optflags));
    }
  }
  if (node->enc != NULL) {
    ADD_STATIC_STRING(" ++enc=");
    ADD_STRING(node->enc);
  }

  if (CMDDEF(node->type).flags & EDITCMD && node->children) {
    did_children = TRUE;
    ADD_STATIC_STRING(" +");
#ifndef DEFINE_LENGTH
    char *arg_start = p;
#endif
    F2(node_repr, node->children, indent, TRUE);
#ifndef DEFINE_LENGTH
    {
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
#endif
  }

  switch (node->cnt_type) {
    case kCntMissing: {
      break;
    }
    case kCntCount:
    case kCntBuffer: {
      ADD_CHAR(' ');
      F(unumber_repr, (uintmax_t) node->cnt.count);
      break;
    }
    case kCntReg: {
      ADD_CHAR(' ');
      ADD_CHAR(node->cnt.reg);
      break;
    }
    case kCntExprReg: {
      // FIXME
      break;
    }
  }

  if (node->exflags)
    ADD_CHAR(' ');
  if (node->exflags & FLAG_EX_LIST)
    ADD_CHAR('l');
  if (node->exflags & FLAG_EX_LNR)
    ADD_CHAR('#');
  if (node->exflags & FLAG_EX_PRINT)
    ADD_CHAR('p');

  if (node->glob != NULL) {
    ADD_CHAR(' ');
    F(glob_repr, node->glob);
  }

  if (node->type == kCmdSyntaxError) {
    char_u *line = node->args[ARG_ERROR_LINESTR].arg.str;
    char_u *message = node->args[ARG_ERROR_MESSAGE].arg.str;
    size_t offset = node->args[ARG_ERROR_OFFSET].arg.flags;
    size_t line_len;

    ADD_STATIC_STRING("\\ error: ");

    ADD_STRING(message);

    ADD_STATIC_STRING(": ");

    line_len = STRLEN(line);

    if (offset < line_len) {
      if (offset) {
        ADD_STRING_LEN(line, offset);
      }
      ADD_STATIC_STRING("!!");
      ADD_CHAR(line[offset]);
      ADD_STATIC_STRING("!!");

      ADD_STRING_LEN(line + offset + 1, line_len - offset - 1);
    } else {
      ADD_STRING_LEN(line, line_len);
      ADD_STATIC_STRING("!!!");
    }
  } else if (CMDDEF(node->type).parse == CMDDEF(kCmdAppend).parse) {
    garray_T *ga = &(node->args[ARG_APPEND_LINES].arg.strs);
    int ga_len = ga->ga_len;
    int i;

    for (i = 0; i < ga_len ; i++) {
      ADD_CHAR('\n');
      ADD_STRING(((char_u **)ga->ga_data)[i]);
    }
    ADD_CHAR('\n');
    ADD_CHAR('.');
  } else if (CMDDEF(node->type).parse == CMDDEF(kCmdMap).parse
             || CMDDEF(node->type).parse == CMDDEF(kCmdUnmap).parse) {
    uint_least32_t map_flags = node->args[ARG_MAP_FLAGS].arg.flags;

    if (map_flags)
      ADD_CHAR(' ');

    if (map_flags & FLAG_MAP_BUFFER) {
      ADD_STATIC_STRING("<buffer>");
    }
    if (map_flags & FLAG_MAP_NOWAIT) {
      ADD_STATIC_STRING("<nowait>");
    }
    if (map_flags & FLAG_MAP_SILENT) {
      ADD_STATIC_STRING("<silent>");
    }
    if (map_flags & FLAG_MAP_SPECIAL) {
      ADD_STATIC_STRING("<special>");
    }
    if (map_flags & FLAG_MAP_SCRIPT) {
      ADD_STATIC_STRING("<script>");
    }
    if (map_flags & FLAG_MAP_EXPR) {
      ADD_STATIC_STRING("<expr>");
    }
    if (map_flags & FLAG_MAP_UNIQUE) {
      ADD_STATIC_STRING("<unique>");
    }

    if (node->args[ARG_MAP_LHS].arg.str != NULL) {
      bool unmap = CMDDEF(node->type).parse == CMDDEF(kCmdUnmap).parse;
      ADD_CHAR(' ');

      // FIXME untranslate mappings
      ADD_STRING(node->args[ARG_MAP_LHS].arg.str);

      if (unmap) {
      } else if (node->args[ARG_MAP_EXPR].arg.expr != NULL) {
        ADD_CHAR(' ');
        F(expr_node_dump, node->args[ARG_MAP_EXPR].arg.expr);
      } else if (node->children != NULL) {
        ADD_CHAR('\n');
        // FIXME untranslate mappings
        F(node_repr, node->children, indent, barnext);
        did_children = TRUE;
      } else if (node->args[ARG_MAP_RHS].arg.str != NULL) {
        ADD_CHAR(' ');
        // FIXME untranslate mappings
        ADD_STRING(node->args[ARG_MAP_RHS].arg.str);
      }
    }
  } else if (CMDDEF(node->type).parse == CMDDEF(kCmdMapclear).parse) {
    if (node->args[ARG_CLEAR_BUFFER].arg.flags) {
      ADD_CHAR(' ');
      ADD_STATIC_STRING("<buffer>");
    }
  } else if (CMDDEF(node->type).parse == CMDDEF(kCmdMenu).parse) {
    uint_least32_t menu_flags = node->args[ARG_MENU_FLAGS].arg.flags;

    if (menu_flags & (FLAG_MENU_SILENT|FLAG_MENU_SPECIAL|FLAG_MENU_SCRIPT))
      ADD_CHAR(' ');

    if (menu_flags & FLAG_MENU_SILENT) {
      ADD_STATIC_STRING("<silent>");
    }
    if (menu_flags & FLAG_MENU_SPECIAL) {
      ADD_STATIC_STRING("<special>");
    }
    if (menu_flags & FLAG_MENU_SCRIPT) {
      ADD_STATIC_STRING("<script>");
    }

    if (node->args[ARG_MENU_ICON].arg.str != NULL) {
      ADD_CHAR(' ');
      ADD_STATIC_STRING("icon=");
      ADD_STRING(node->args[ARG_MENU_ICON].arg.str);
    }

    if (node->args[ARG_MENU_PRI].arg.numbers != NULL) {
      int *number = node->args[ARG_MENU_PRI].arg.numbers;

      ADD_CHAR(' ');

      while (*number) {
        if (*number != MENU_DEFAULT_PRI)
          F(unumber_repr, (uintmax_t) *number);
        number++;
        if (*number)
          ADD_CHAR('.');
      }
    }

    if (menu_flags & FLAG_MENU_DISABLE) {
      ADD_CHAR(' ');
      ADD_STATIC_STRING("disable");
    }
    if (menu_flags & FLAG_MENU_ENABLE) {
      ADD_CHAR(' ');
      ADD_STATIC_STRING("enable");
    }

    if (node->args[ARG_MENU_NAME].arg.menu_item != NULL) {
      MenuItem *cur = node->args[ARG_MENU_NAME].arg.menu_item;

      while (cur != NULL) {
        if (cur == node->args[ARG_MENU_NAME].arg.menu_item)
          ADD_CHAR(' ');
        else
          ADD_CHAR('.');
        ADD_STRING(cur->name);
        cur = cur->subitem;
      }

      if (node->args[ARG_MENU_TEXT].arg.str != NULL) {
        ADD_STATIC_STRING("<Tab>");
        ADD_STRING(node->args[ARG_MENU_TEXT].arg.str);
      }
    }

    start_from_arg = ARG_MENU_RHS;
    do_arg_dump = TRUE;
  } else if (CMDDEF(node->type).parse == CMDDEF(kCmdFor).parse) {
    ADD_CHAR(' ');
    F(expr_node_dump, node->args[ARG_FOR_LHS].arg.expr);
    ADD_STATIC_STRING(" in ");
    F(expr_node_dump, node->args[ARG_FOR_RHS].arg.expr);
  } else if (CMDDEF(node->type).parse == CMDDEF(kCmdCaddexpr).parse
             || CMDDEF(node->type).parse == CMDDEF(kCmdEcho).parse
             || CMDDEF(node->type).parse == CMDDEF(kCmdDelfunction).parse) {
    start_from_arg = 1;
    do_arg_dump = TRUE;
  } else if (CMDDEF(node->type).parse == CMDDEF(kCmdLockvar).parse) {
    if (node->args[ARG_LOCKVAR_DEPTH].arg.number) {
      ADD_CHAR(' ');
      F(unumber_repr, node->args[ARG_LOCKVAR_DEPTH].arg.number);
    }
    ADD_CHAR(' ');
    F(expr_node_dump, node->args[ARG_EXPRS_EXPRS].arg.expr);
  } else if (CMDDEF(node->type).parse == CMDDEF(kCmdFunction).parse) {
    if (node->args[ARG_FUNC_REG].arg.reg == NULL) {
      if (node->args[ARG_FUNC_NAME].arg.expr != NULL) {
        ADD_CHAR(' ');
        F(expr_node_dump, node->args[ARG_FUNC_NAME].arg.expr);
        if (node->args[ARG_FUNC_ARGS].arg.strs.ga_itemsize != 0) {
          uint_least32_t flags = node->args[ARG_FUNC_FLAGS].arg.flags;
          garray_T *ga = &(node->args[ARG_FUNC_ARGS].arg.strs);
          SPACES(po->command.function.before_sub)
          ADD_CHAR('(');
          SPACES(po->command.function.call.after_start)
          for (int i = 0; i < ga->ga_len; i++) {
            ADD_STRING(((char_u **)ga->ga_data)[i]);
            if (i < ga->ga_len - 1 || flags&FLAG_FUNC_VARARGS) {
              SPACES(po->command.function.argument.before)
              ADD_CHAR(',');
              SPACES(po->command.function.argument.after)
            }
          }
          if (flags&FLAG_FUNC_VARARGS) {
            ADD_STATIC_STRING("...");
          }
          SPACES(po->command.function.call.before_end)
          ADD_CHAR(')');
          if (flags&FLAG_FUNC_RANGE) {
            SPACES(po->command.function.attribute)
            ADD_STATIC_STRING("range");
          }
          if (flags&FLAG_FUNC_DICT) {
            SPACES(po->command.function.attribute)
            ADD_STATIC_STRING("dict");
          }
          if (flags&FLAG_FUNC_ABORT) {
            SPACES(po->command.function.attribute)
            ADD_STATIC_STRING("abort");
          }
        }
      }
    }
  } else if (CMDDEF(node->type).parse == CMDDEF(kCmdLet).parse) {
    if (node->args[ARG_LET_LHS].arg.expr != NULL) {
      LetAssignmentType ass_type =
          (LetAssignmentType) node->args[ARG_LET_ASS_TYPE].arg.flags;
      bool add_rval = TRUE;

      ADD_CHAR(' ');
      F(expr_node_dump, node->args[ARG_LET_LHS].arg.expr);
      switch (ass_type) {
        case VAL_LET_NO_ASS: {
          add_rval = FALSE;
          break;
        }
        case VAL_LET_ASSIGN: {
          SPACES(po->command.let.assign.before)
          ADD_CHAR('=');
          SPACES(po->command.let.assign.after)
          break;
        }
        case VAL_LET_ADD: {
          SPACES(po->command.let.add.before)
          ADD_STATIC_STRING("+=");
          SPACES(po->command.let.add.after)
          break;
        }
        case VAL_LET_SUBTRACT: {
          SPACES(po->command.let.subtract.before)
          ADD_STATIC_STRING("-=");
          SPACES(po->command.let.subtract.after)
          break;
        }
        case VAL_LET_APPEND: {
          SPACES(po->command.let.concat.before)
          ADD_STATIC_STRING(".=");
          SPACES(po->command.let.concat.after)
          break;
        }
      }
      if (add_rval) {
        F(expr_node_dump, node->args[ARG_LET_RHS].arg.expr);
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
            ADD_CHAR(' ');
            F(expr_node_dump, node->args[i].arg.expr);
          }
          break;
        }
        case kArgString: {
          if (node->args[i].arg.str != NULL) {
            ADD_CHAR(' ');
            // FIXME Escape arguments if necessary
            ADD_STRING(node->args[i].arg.str);
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
      ADD_CHAR(' ');
      F(node_repr, node->children, indent, barnext);
    } else if (CMDDEF(node->type).parse == CMDDEF(kCmdArgdo).parse) {
      ADD_CHAR(' ');
      F(node_repr, node->children, indent, TRUE);
    } else {
      if (barnext) {
        ADD_STATIC_STRING(" | ");
      } else {
        ADD_CHAR('\n');
      }
      F(node_repr, node->children, indent + 1, barnext);
    }
  }

  if (node->next != NULL) {
    if (barnext) {
      ADD_STATIC_STRING(" | ");
    } else {
      ADD_CHAR('\n');
    }
    F(node_repr, node->next, indent, barnext);
  }

  FUNCTION_END;
}

#endif  // NEOVIM_TRANSLATOR_PRINTER_EX_COMMANDS_C_H
