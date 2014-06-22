#ifndef NVIM_VIML_PRINTER_EX_COMMANDS_C_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "nvim/vim.h"
#include "nvim/memory.h"
#include "nvim/strings.h"


#include "nvim/viml/printer/printer.h"
#include "nvim/viml/printer/expressions.h"
#include "nvim/viml/parser/expressions.h"
#include "nvim/viml/parser/ex_commands.h"
#include "nvim/viml/dumpers/dumpers.h"

#if !defined(CH_MACROS_DEFINE_LENGTH) && !defined(CH_MACROS_DEFINE_FWRITE)
# define CH_MACROS_DEFINE_LENGTH
# include "nvim/viml/printer/ex_commands.c.h"
# undef CH_MACROS_DEFINE_LENGTH
#elif !defined(CH_MACROS_DEFINE_FWRITE)
# undef CH_MACROS_DEFINE_LENGTH
# define CH_MACROS_DEFINE_FWRITE
# include "nvim/viml/printer/ex_commands.c.h"
# undef CH_MACROS_DEFINE_FWRITE
# define CH_MACROS_DEFINE_LENGTH
#endif
#define NVIM_VIML_PRINTER_EX_COMMANDS_C_H

#ifndef NVIM_VIML_DUMPERS_CH_MACROS
# define CH_MACROS_OPTIONS_TYPE const PrinterOptions *const
# define CH_MACROS_INDENT_STR o->command.indent
#endif
#include "nvim/viml/dumpers/ch_macros.h"

#ifdef INCLUDE_GENERATED_DECLARATIONS
# include "viml/printer/ex_commands.c.h.generated.h"
#endif

static FDEC(print_glob, const Glob *const glob)
{
  FUNCTION_START;
  const Glob *cur_glob;

  if (glob == NULL)
    EARLY_RETURN;

  for (cur_glob = glob; cur_glob != NULL; cur_glob = cur_glob->next) {
    switch (cur_glob->type) {
      case kGlobExpression: {
        WS("`=");
        F(print_expr_node, cur_glob->data.expr);
        WC('`');
        break;
      }
      case kGlobShell: {
        W(cur_glob->data.str);
        break;
      }
      case kPatHome: {
        WC('~');
        break;
      }
      case kPatCurrent: {
        WC('%');
        break;
      }
      case kPatAlternate: {
        WC('#');
        break;
      }
      case kPatCharacter: {
        WC('?');
        break;
      }
      case kPatAnything: {
        WC('*');
        break;
      }
      case kPatArguments: {
        WS("##");
        break;
      }
      case kPatAnyRecurse: {
        WS("**");
        break;
      }
      case kPatOldFile: {
        WS("#<");
        F_NOOPT(dump_unumber, (uintmax_t) cur_glob->data.number);
        break;
      }
      case kPatBufname: {
        WC('#');
        F_NOOPT(dump_unumber, (uintmax_t) cur_glob->data.number);
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
        WC('{');
        for (cglob = cur_glob->data.glob; cglob != NULL; cglob = cglob->next) {
          F(print_glob, cglob);
          if (cglob->next != NULL)
            WC(',');
        }
        WC('}');
        break;
      }
      case kPatLiteral: {
        W(cur_glob->data.str);
        break;
      }
      case kPatMissing: {
        assert(false);
      }
    }
  }

  FUNCTION_END;
}

static FDEC(print_regex, const Regex *const regex)
{
  FUNCTION_START;
  assert(regex->string != NULL);
  W(regex->string);
  FUNCTION_END;
}

static FDEC(print_address_followup, const AddressFollowup *const followup)
{
  FUNCTION_START;

  if (followup == NULL)
    EARLY_RETURN;

  switch (followup->type) {
    case kAddressFollowupMissing: {
      EARLY_RETURN;
    }
    case kAddressFollowupShift: {
      if (followup->data.shift >= 0) {
        WC('+');
      }
      F_NOOPT(dump_number, (intmax_t) followup->data.shift);
      break;
    }
    case kAddressFollowupForwardPattern:
    case kAddressFollowupBackwardPattern: {
#ifndef CH_MACROS_DEFINE_LENGTH
      char_u ch = followup->type == kAddressFollowupForwardPattern
                  ? '/'
                  : '?';
#endif
      WC(ch);
      F(print_regex, followup->data.regex);
      WC(ch);
      break;
    }
  }

  F(print_address_followup, followup->next);

  FUNCTION_END;
}

static FDEC(print_address, const Address *const address)
{
  FUNCTION_START;

  if (address == NULL)
    EARLY_RETURN;

  switch (address->type) {
    case kAddrMissing: {
      EARLY_RETURN;
    }
    case kAddrFixed: {
      F_NOOPT(dump_unumber, (uintmax_t) address->data.lnr);
      break;
    }
    case kAddrEnd: {
      WC('$');
      break;
    }
    case kAddrCurrent: {
      WC('.');
      break;
    }
    case kAddrMark: {
      WC('\'');
      WC(address->data.mark);
      break;
    }
    case kAddrForwardSearch:
    case kAddrBackwardSearch: {
#ifndef CH_MACROS_DEFINE_LENGTH
      char_u ch = address->type == kAddrForwardSearch
                  ? '/'
                  : '?';
#endif
      WC(ch);
      F(print_regex, address->data.regex);
      WC(ch);
      break;
    }
    case kAddrForwardPreviousSearch: {
      WS("\\/");
      break;
    }
    case kAddrBackwardPreviousSearch: {
      WS("\\?");
      break;
    }
    case kAddrSubstituteSearch: {
      WS("\\&");
      break;
    }
  }

  FUNCTION_END;
}

static FDEC(print_range, const Range *const range)
{
  FUNCTION_START;

  if (range->address.type == kAddrMissing)
    EARLY_RETURN;

  F(print_address, &(range->address));
  F(print_address_followup, range->address.followups);
  if (range->next != NULL) {
    WC(range->setpos ? ';' : ',');
    F(print_range, range->next);
  }

  FUNCTION_END;
}

static FDEC(print_node_name, const CommandType node_type,
                            const char_u *const node_name,
                            const bool node_bang)
{
  FUNCTION_START;
  const char_u *name;

  if (node_name != NULL)
    name = node_name;
  else if (CMDDEF(node_type).name == NULL)
    name = (const char_u *) "";
  else
    name = CMDDEF(node_type).name;

  W(name);

  if (node_bang)
    WC('!');

  FUNCTION_END;
}

static FDEC(print_optflags, const uint_least32_t optflags,
                           const char_u *const enc)
{
  FUNCTION_START;

  if (optflags & FLAG_OPT_BIN_USE_FLAG) {
    if (optflags & FLAG_OPT_BIN) {
      WS(" ++bin");
    } else {
      W(" ++nobin");
    }
  }
  if (optflags & FLAG_OPT_EDIT) {
    WS(" ++edit");
  }
  switch (optflags & FLAG_OPT_FF_MASK) {
    case 0: {
      break;
    }
    case VAL_OPT_FF_DOS: {
      WS(" ++ff=dos");
      break;
    }
    case VAL_OPT_FF_MAC: {
      WS(" ++ff=mac");
      break;
    }
    case VAL_OPT_FF_UNIX: {
      WS(" ++ff=unix");
      break;
    }
    default: {
      assert(false);
    }
  }
  switch (optflags & FLAG_OPT_BAD_MASK) {
    case 0: {
      break;
    }
    case VAL_OPT_BAD_KEEP: {
      WS(" ++bad=keep");
      break;
    }
    case VAL_OPT_BAD_DROP: {
      WS(" ++bad=drop");
      break;
    }
    default: {
      WS(" ++bad=");
      WC(VAL_OPT_BAD_TO_CHAR(optflags));
    }
  }
  if (enc != NULL) {
    WS(" ++enc=");
    W(enc);
  }

  FUNCTION_END;
}

static FDEC(print_count, const CommandNode *const node)
{
  FUNCTION_START;

  switch (node->cnt_type) {
    case kCntMissing: {
      break;
    }
    case kCntCount:
    case kCntBuffer: {
      WC(' ');
      F_NOOPT(dump_unumber, (uintmax_t) node->cnt.count);
      break;
    }
    case kCntReg: {
      WC(' ');
      WC(node->cnt.reg);
      break;
    }
    case kCntExprReg: {
      // FIXME
      break;
    }
  }

  FUNCTION_END;
}

static FDEC(print_exflags, const uint_least8_t exflags)
{
  FUNCTION_START;

  if (exflags) {
    WC(' ');
    if (exflags & FLAG_EX_LIST)
      WC('l');
    if (exflags & FLAG_EX_LNR)
      WC('#');
    if (exflags & FLAG_EX_PRINT)
      WC('p');
  }

  FUNCTION_END;
}

static FDEC(print_node, const CommandNode *const node,
                        const size_t indent,
                        const bool barnext)
{
  FUNCTION_START;
  size_t start_from_arg;
  bool do_arg_dump = false;
  bool did_children = false;

  if (node == NULL)
    EARLY_RETURN;

  if (!barnext)
    WINDENT(indent);

  F(print_range, &(node->range));
  F(print_node_name, node->type, node->name, node->bang);
  F(print_optflags, node->optflags, node->enc);

  if (CMDDEF(node->type).flags & EDITCMD && node->children) {
    did_children = true;
    WS(" +");
#ifndef CH_MACROS_DEFINE_LENGTH
    const char *const escapes = "\011\012\013\014\015 \\\n|\"";
    // vim_isspace:              ^9  ^10 ^11 ^12 ^13 ^  | ||
    // ENDS_EXCMD:                                      ^ ^^
#endif
    F_ESCAPED(print_node, escapes, node->children, indent, true);
  }

  F(print_count, node);
  F(print_exflags, node->exflags);

  if (node->glob != NULL) {
    WC(' ');
    F(print_glob, node->glob);
  }

  if (node->type == kCmdSyntaxError) {
    const char_u *line = node->args[ARG_ERROR_LINESTR].arg.str;
    const char_u *message = node->args[ARG_ERROR_MESSAGE].arg.str;
    const size_t offset = node->args[ARG_ERROR_OFFSET].arg.flags;
    size_t line_len;

    WS("\\ error: ");

    W(message);

    WS(": ");

    line_len = STRLEN(line);

    if (offset < line_len) {
      if (offset) {
        W_LEN(line, offset);
      }
      WS("!!");
      W_LEN(line + offset, 1);
      WS("!!");

      W_LEN(line + offset + 1, line_len - offset - 1);
    } else {
      W_LEN(line, line_len);
      WS("!!!");
    }
  } else if (CMDDEF(node->type).parse == CMDDEF(kCmdAppend).parse) {
    const garray_T *ga = &(node->args[ARG_APPEND_LINES].arg.strs);
    const int ga_len = ga->ga_len;
    int i;

    for (i = 0; i < ga_len ; i++) {
      WC('\n');
      W(((char_u **)ga->ga_data)[i]);
    }
    WC('\n');
    WC('.');
  } else if (CMDDEF(node->type).parse == CMDDEF(kCmdMap).parse
             || CMDDEF(node->type).parse == CMDDEF(kCmdUnmap).parse) {
    const uint_least32_t map_flags = node->args[ARG_MAP_FLAGS].arg.flags;

    if (map_flags)
      WC(' ');

    if (map_flags & FLAG_MAP_BUFFER) {
      WS("<buffer>");
    }
    if (map_flags & FLAG_MAP_NOWAIT) {
      WS("<nowait>");
    }
    if (map_flags & FLAG_MAP_SILENT) {
      WS("<silent>");
    }
    if (map_flags & FLAG_MAP_SPECIAL) {
      WS("<special>");
    }
    if (map_flags & FLAG_MAP_SCRIPT) {
      WS("<script>");
    }
    if (map_flags & FLAG_MAP_EXPR) {
      WS("<expr>");
    }
    if (map_flags & FLAG_MAP_UNIQUE) {
      WS("<unique>");
    }

    if (node->args[ARG_MAP_LHS].arg.str != NULL) {
      bool unmap = CMDDEF(node->type).parse == CMDDEF(kCmdUnmap).parse;
      WC(' ');

      // FIXME untranslate mappings
      W(node->args[ARG_MAP_LHS].arg.str);

      if (unmap) {
      } else if (node->args[ARG_MAP_EXPR].arg.expr != NULL) {
        WC(' ');
        F(print_expr_node, node->args[ARG_MAP_EXPR].arg.expr);
      } else if (node->children != NULL) {
        WC('\n');
        // FIXME untranslate mappings
        F(print_node, node->children, indent, barnext);
        did_children = true;
      } else if (node->args[ARG_MAP_RHS].arg.str != NULL) {
        WC(' ');
        // FIXME untranslate mappings
        W(node->args[ARG_MAP_RHS].arg.str);
      }
    }
  } else if (CMDDEF(node->type).parse == CMDDEF(kCmdMapclear).parse) {
    if (node->args[ARG_CLEAR_BUFFER].arg.flags) {
      WC(' ');
      WS("<buffer>");
    }
  } else if (CMDDEF(node->type).parse == CMDDEF(kCmdMenu).parse) {
    const uint_least32_t menu_flags = node->args[ARG_MENU_FLAGS].arg.flags;

    if (menu_flags & (FLAG_MENU_SILENT|FLAG_MENU_SPECIAL|FLAG_MENU_SCRIPT))
      WC(' ');

    if (menu_flags & FLAG_MENU_SILENT) {
      WS("<silent>");
    }
    if (menu_flags & FLAG_MENU_SPECIAL) {
      WS("<special>");
    }
    if (menu_flags & FLAG_MENU_SCRIPT) {
      WS("<script>");
    }

    if (node->args[ARG_MENU_ICON].arg.str != NULL) {
      WC(' ');
      WS("icon=");
      W(node->args[ARG_MENU_ICON].arg.str);
    }

    if (node->args[ARG_MENU_PRI].arg.numbers != NULL) {
      const int *number = node->args[ARG_MENU_PRI].arg.numbers;

      WC(' ');

      while (*number) {
        if (*number != MENU_DEFAULT_PRI)
          F_NOOPT(dump_unumber, (uintmax_t) *number);
        number++;
        if (*number)
          WC('.');
      }
    }

    if (menu_flags & FLAG_MENU_DISABLE) {
      WC(' ');
      WS("disable");
    }
    if (menu_flags & FLAG_MENU_ENABLE) {
      WC(' ');
      WS("enable");
    }

    if (node->args[ARG_MENU_NAME].arg.menu_item != NULL) {
      MenuItem *cur = node->args[ARG_MENU_NAME].arg.menu_item;

      while (cur != NULL) {
        if (cur == node->args[ARG_MENU_NAME].arg.menu_item)
          WC(' ');
        else
          WC('.');
        W(cur->name);
        cur = cur->subitem;
      }

      if (node->args[ARG_MENU_TEXT].arg.str != NULL) {
        WS("<Tab>");
        W(node->args[ARG_MENU_TEXT].arg.str);
      }
    }

    start_from_arg = ARG_MENU_RHS;
    do_arg_dump = true;
  } else if (CMDDEF(node->type).parse == CMDDEF(kCmdFor).parse) {
    WC(' ');
    F(print_expr_node, node->args[ARG_FOR_LHS].arg.expr);
    WS(" in ");
    F(print_expr_node, node->args[ARG_FOR_RHS].arg.expr);
  } else if (CMDDEF(node->type).parse == CMDDEF(kCmdCaddexpr).parse
             || CMDDEF(node->type).parse == CMDDEF(kCmdEcho).parse
             || CMDDEF(node->type).parse == CMDDEF(kCmdDelfunction).parse) {
    start_from_arg = 1;
    do_arg_dump = true;
  } else if (CMDDEF(node->type).parse == CMDDEF(kCmdLockvar).parse) {
    if (node->args[ARG_LOCKVAR_DEPTH].arg.number) {
      WC(' ');
      F_NOOPT(dump_unumber, node->args[ARG_LOCKVAR_DEPTH].arg.number);
    }
    WC(' ');
    F(print_expr_node, node->args[ARG_EXPRS_EXPRS].arg.expr);
  } else if (CMDDEF(node->type).parse == CMDDEF(kCmdFunction).parse) {
    if (node->args[ARG_FUNC_REG].arg.reg == NULL) {
      if (node->args[ARG_FUNC_NAME].arg.expr != NULL) {
        WC(' ');
        F(print_expr_node, node->args[ARG_FUNC_NAME].arg.expr);
        if (node->args[ARG_FUNC_ARGS].arg.strs.ga_itemsize != 0) {
          const uint_least32_t flags = node->args[ARG_FUNC_FLAGS].arg.flags;
          const garray_T *ga = &(node->args[ARG_FUNC_ARGS].arg.strs);
          SPACES_BEFORE_SUBSCRIPT2(command, function);
          WC('(');
          SPACES_AFTER_START3(command, function, call);
          for (int i = 0; i < ga->ga_len; i++) {
            W(((char_u **)ga->ga_data)[i]);
            if (i < ga->ga_len - 1 || flags&FLAG_FUNC_VARARGS) {
              SPACES_BEFORE3(command, function, argument);
              WC(',');
              SPACES_AFTER3(command, function, argument);
            }
          }
          if (flags&FLAG_FUNC_VARARGS) {
            WS("...");
          }
          SPACES_BEFORE_END3(command, function, call);
          WC(')');
          if (flags&FLAG_FUNC_RANGE) {
            SPACES_BEFORE_ATTRIBUTE2(command, function);
            WS("range");
          }
          if (flags&FLAG_FUNC_DICT) {
            SPACES_BEFORE_ATTRIBUTE2(command, function);
            WS("dict");
          }
          if (flags&FLAG_FUNC_ABORT) {
            SPACES_BEFORE_ATTRIBUTE2(command, function);
            WS("abort");
          }
        }
      }
    }
  } else if (CMDDEF(node->type).parse == CMDDEF(kCmdLet).parse) {
    if (node->args[ARG_LET_LHS].arg.expr != NULL) {
      const LetAssignmentType ass_type =
          (LetAssignmentType) node->args[ARG_LET_ASS_TYPE].arg.flags;
      bool add_rval = true;

      WC(' ');
      F(print_expr_node, node->args[ARG_LET_LHS].arg.expr);
      switch (ass_type) {
        case VAL_LET_NO_ASS: {
          add_rval = false;
          break;
        }
        case VAL_LET_ASSIGN: {
          SPACES_BEFORE3(command, let, assign);
          WC('=');
          SPACES_AFTER3(command, let, assign);
          break;
        }
        case VAL_LET_ADD: {
          SPACES_BEFORE3(command, let, add);
          WS("+=");
          SPACES_AFTER3(command, let, add);
          break;
        }
        case VAL_LET_SUBTRACT: {
          SPACES_BEFORE3(command, let, subtract);
          WS("-=");
          SPACES_AFTER3(command, let, subtract);
          break;
        }
        case VAL_LET_APPEND: {
          SPACES_BEFORE3(command, let, concat);
          WS(".=");
          SPACES_AFTER3(command, let, concat);
          break;
        }
      }
      if (add_rval) {
        F(print_expr_node, node->args[ARG_LET_RHS].arg.expr);
      }
    }
  } else {
    start_from_arg = 0;
    do_arg_dump = true;
  }

  if (do_arg_dump) {
    size_t i;

    for (i = start_from_arg; i < CMDDEF(node->type).num_args; i++) {
      switch (CMDDEF(node->type).arg_types[i]) {
        case kArgExpressions:
        case kArgExpression: {
          if (node->args[i].arg.expr != NULL) {
            WC(' ');
            F(print_expr_node, node->args[i].arg.expr);
          }
          break;
        }
        case kArgString: {
          if (node->args[i].arg.str != NULL) {
            if (node->type == kCmdComment) {
              SPACES_BEFORE_TEXT2(command, comment);
            } else if (node->type != kCmdHashbangComment) {
              WC(' ');
            }
            // FIXME Escape arguments if necessary
            W(node->args[i].arg.str);
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
      WC(' ');
      F(print_node, node->children, indent, barnext);
    } else if (CMDDEF(node->type).parse == CMDDEF(kCmdArgdo).parse) {
      WC(' ');
      F(print_node, node->children, indent, true);
    } else {
      if (barnext) {
        WS(" | ");
      } else {
        WC('\n');
      }
      F(print_node, node->children, indent + 1, barnext);
    }
  }

  if (node->next != NULL) {
    if (barnext) {
      WS(" | ");
    } else {
      WC('\n');
    }
    F(print_node, node->next, indent, barnext);
  }

  FUNCTION_END;
}

#undef CH_MACROS_OPTIONS_TYPE

#endif  // NVIM_VIML_PRINTER_EX_COMMANDS_C_H
